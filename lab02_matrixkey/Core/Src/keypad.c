/**
  ******************************************************************************
  * @file    keypad.c
  * @brief   实验二：3×4 矩阵键盘扫描（非阻塞 + 软件消抖）
  *
  * 设计要点：
  *   1. 不再用 HAL_Delay 阻塞消抖。改成"定周期采样 + 稳定时间判定"：
  *      每 2ms 扫一遍 12 个按键，状态连续 10ms 不变才认为有效，
  *      这样主循环任何时候都不会被卡住。
  *   2. 只在"按下沿"产生一次键值（bit 由 0 变 1），按住不放不会连续触发。
  *   3. 行线用 BSRR 操作，只动 PE4~PE6，不会把输入脚 PE0~PE3 一起写掉。
  *   4. 行序交给 s_row_map 一个表描述，换块板子只改这张表即可。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "keypad.h"
#include "stm32f4xx_hal.h"

/* Private define ------------------------------------------------------------*/
#define ROW_COUNT        3u
#define COL_COUNT        4u
#define KEY_TOTAL        (ROW_COUNT * COL_COUNT)      /* 12 个按键 */

#define ROW_PORT         GPIOE
#define ROW_FIRST_PIN    GPIO_PIN_4                   /* 行线 PE4、PE5、PE6 */
#define ROW_MASK         (GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6)

#define COL_PORT         GPIOE
#define COL_MASK         (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)

#define SCAN_PERIOD_MS   2u                           /* 采样周期 */
#define DEBOUNCE_MS      10u                          /* 连续稳定多久算有效 */
#define SETTLE_LOOPS     40u                          /* 切换行后等待列线稳定的空转次数 */

/* Private variables ---------------------------------------------------------*/
/* 扫描序号 → 键盘实际行号，按开发板接线顺序填写 */
static const uint8_t s_row_map[ROW_COUNT] = { 2u, 0u, 1u };

static uint16_t s_stable_mask;    /* 已确认的按键位图，bit=1 表示按下 */
static uint16_t s_raw_prev;       /* 上一次原始采样 */
static uint32_t s_raw_tick;       /* 原始采样发生变化的时刻 */
static uint32_t s_scan_tick;      /* 上一次扫描的时刻 */
static uint8_t  s_pending;        /* 待取走的键值 */

/* Private functions ---------------------------------------------------------*/

/** 三根行线全部恢复高电平（无按键扫描时保持闲置） */
static void row_release_all(void)
{
  ROW_PORT->BSRR = (uint32_t)ROW_MASK;
}

/** 只把第 row 行拉低，其余行保持高电平 */
static void row_drive_low(uint8_t row)
{
  uint32_t pin = (uint32_t)ROW_FIRST_PIN << row;

  ROW_PORT->BSRR = (uint32_t)ROW_MASK;    /* 先全部拉高 */
  ROW_PORT->BSRR = pin << 16u;            /* 再拉低选中行（BR 位写 1 → 输出低） */
}

/**
  * @brief  完整扫描一遍 12 个按键
  * @retval 12 位按键位图，bit=1 表示该键处于按下状态
  * @note   行扫描期间会短暂阻塞约十几微秒，属于端口电平稳定所需，可忽略
  */
static uint16_t scan_raw(void)
{
  uint16_t bitmap = 0u;
  uint8_t  row;
  uint8_t  col;
  uint32_t cols;

  for (row = 0u; row < ROW_COUNT; row++)
  {
    row_drive_low(row);

    /* 列线是上拉输入，换行后要给它一点时间回到高电平 */
    for (volatile uint32_t wait = 0u; wait < SETTLE_LOOPS; wait++)
    {
      __NOP();
    }

    cols = COL_PORT->IDR & (uint32_t)COL_MASK;

    for (col = 0u; col < COL_COUNT; col++)
    {
      if ((cols & (uint32_t)(1u << col)) == 0u)          /* 列线被拉低 = 该列有键按下 */
      {
        bitmap |= (uint16_t)(1u << (row * COL_COUNT + col));
      }
    }
  }

  row_release_all();
  return bitmap;
}

/**
  * @brief  把按键位图转换成 1~12 的键值
  * @note   同时按下多个键时只取编号最小的那个
  */
static uint8_t decode_key(uint16_t bitmap)
{
  uint8_t bit;

  for (bit = 0u; bit < KEY_TOTAL; bit++)
  {
    if ((bitmap & (uint16_t)(1u << bit)) != 0u)
    {
      uint8_t row = (uint8_t)(bit / COL_COUNT);
      uint8_t col = (uint8_t)(bit % COL_COUNT);

      return (uint8_t)(s_row_map[row] * COL_COUNT + col + 1u);
    }
  }

  return KEYPAD_KEY_NONE;
}

/* Exported functions --------------------------------------------------------*/

void KEYPAD_Init(void)
{
  s_stable_mask = 0u;
  s_raw_prev    = 0u;
  s_raw_tick    = 0u;
  s_scan_tick   = 0u;
  s_pending     = KEYPAD_KEY_NONE;

  row_release_all();
}

uint8_t KEYPAD_GetKey(void)
{
  uint32_t now = HAL_GetTick();
  uint8_t  key;
  uint16_t raw;
  uint16_t pressed_edge;

  /* 定周期采样，避免主循环空把 CPU 跑满 */
  if ((now - s_scan_tick) >= SCAN_PERIOD_MS)
  {
    raw = scan_raw();
    s_scan_tick = now;

    if (raw != s_raw_prev)
    {
      /* 状态还在抖，重新计时 */
      s_raw_prev = raw;
      s_raw_tick = now;
    }
    else if ((raw != s_stable_mask) && ((now - s_raw_tick) >= DEBOUNCE_MS))
    {
      /* 连续稳定够久，正式生效；只取按下沿（0 → 1 的位） */
      pressed_edge = (uint16_t)(raw & (uint16_t)(~s_stable_mask));
      s_stable_mask = raw;

      if (pressed_edge != 0u)
      {
        s_pending = decode_key(pressed_edge);
      }
    }
    else
    {
      /* 状态稳定但没有新的按下，什么都不做 */
    }
  }

  key = s_pending;
  s_pending = KEYPAD_KEY_NONE;
  return key;
}
