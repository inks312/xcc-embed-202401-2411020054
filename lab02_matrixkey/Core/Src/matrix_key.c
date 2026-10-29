/**
  ******************************************************************************
  * @file    matrix_key.c
  * @brief   3×4 矩阵键盘驱动实现
  ******************************************************************************
  * 扫描顺序：先选通行 → 等电平稳定 → 读列 → 查表得到列号 → 恢复所有行。
  *
  * 原代码的写法是 "先读列，再换行"，靠 PreLine[3] = {2,0,1} 这张映射表
  * 把 "上一次设置的行" 和 "这一次读到的列" 对上号，属于典型的"错位一拍"，
  * 第一次扫描读到的是上电时全部行都是高电平的无效状态，逻辑很难讲清楚。
  * 这里改成老老实实的 "先选行、再读列"，一眼就能看懂。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>
#include "matrix_key.h"

/* Private define ------------------------------------------------------------*/
/* 消抖状态机的 4 个状态 */
#define KEY_ST_RELEASED           0U   /* 等待按下             */
#define KEY_ST_PRESS_DEBOUNCE     1U   /* 按下消抖中           */
#define KEY_ST_PRESSED            2U   /* 已确认按下，等待释放 */
#define KEY_ST_RELEASE_DEBOUNCE   3U   /* 释放消抖中           */

/* Private variables ---------------------------------------------------------*/
/* 列线 IDR[3:0] → 列号（1~4）。
   某一位为 0 表示该列被拉低；0x0F 表示全部为高（没有键按下）；
   出现两个及以上 0（多点按键 / 鬼键）一律视为无效，返回 0xFF。 */
static const uint8_t s_col_lut[16] =
{
  0xFFU, 0xFFU, 0xFFU, 0xFFU,   /* 0x00 ~ 0x03 无效 */
  0xFFU, 0xFFU, 0xFFU, 0x04U,   /* 0x07 = COL3 按下 */
  0xFFU, 0xFFU, 0xFFU, 0x03U,   /* 0x0B = COL2 按下 */
  0xFFU, 0x02U, 0x01U, 0x00U    /* 0x0D = COL1，0x0E = COL0，0x0F = 无键 */
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  选通某一行（该行拉低，其余行释放为高阻）
  * @param  row：0 ~ 2
  * @retval 无
  * @note   开漏输出：BSRR 置位 = 释放（高阻），复位 = 拉低。
  *         一次 32 位写入同时完成 "选中行拉低 + 其余行释放"。
  */
static void MatrixKey_SelectRow(uint8_t row)
{
  uint32_t bit = (uint32_t)1U << (MATRIX_KEY_ROW_FIRST + row);

  MATRIX_KEY_ROW_PORT->BSRR = ((uint32_t)MATRIX_KEY_ROW_MASK & ~bit) | (bit << 16);
}

/**
  * @brief  释放所有行线（等待下一次扫描，期间不驱动任何行，功耗最低）
  * @retval 无
  */
static void MatrixKey_ReleaseAllRows(void)
{
  MATRIX_KEY_ROW_PORT->BSRR = (uint32_t)MATRIX_KEY_ROW_MASK;
}

/**
  * @brief  换行后等待列线电平恢复稳定
  * @retval 无
  */
static void MatrixKey_SettleDelay(void)
{
  volatile uint32_t i;

  for (i = 0U; i < MATRIX_KEY_SETTLE_LOOPS; i++)
  {
    __NOP();
  }
}

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  初始化矩阵键盘：释放所有行线
  * @retval 无
  * @note   引脚方向由 CubeMX 生成的 MX_GPIO_Init() 配置：
  *           PE4~PE6 → GPIO_MODE_OUTPUT_OD（开漏输出，见 gpio.c 注释）
  *           PE0~PE3 → GPIO_MODE_INPUT + GPIO_PULLUP（内部上拉，约 40k）
  *         若用 CubeMX 重新生成代码，务必把 PE4~PE6 保持为 Output Open Drain。
  */
void MatrixKey_Init(void)
{
  MatrixKey_ReleaseAllRows();
}

/**
  * @brief  完整扫描一次键盘
  * @retval 1~12：有效键值；0：没有按键或结果无效
  */
uint8_t MatrixKey_Scan(void)
{
  uint8_t key = MATRIX_KEY_NONE;
  uint8_t row;
  uint8_t col;

  for (row = 0U; row < MATRIX_KEY_ROW_NUM; row++)
  {
    MatrixKey_SelectRow(row);
    MatrixKey_SettleDelay();

    col = s_col_lut[MATRIX_KEY_COL_PORT->IDR & MATRIX_KEY_COL_MASK];

    if ((col >= 1U) && (col <= MATRIX_KEY_COL_NUM))
    {
      /* 同一个键按下时，只可能出现在一个行上，找到即可退出 */
      key = (uint8_t)(row * MATRIX_KEY_COL_NUM + col);
      break;
    }
  }

  MatrixKey_ReleaseAllRows();
  return key;
}

/**
  * @brief  带消抖的按键事件获取（非阻塞）
  * @param  key：输出参数，事件对应的键值（1~12）
  * @retval 事件类型：无 / 按下 / 释放
  * @note   必须在主循环中反复调用。函数内部不调用 HAL_Delay()：默认每 1ms
  *         真正扫描一次（单次约 60~80µs），其余调用立即返回。
  */
MatrixKeyEvent_t MatrixKey_GetEvent(uint8_t *key)
{
  static uint8_t  s_state     = KEY_ST_RELEASED;
  static uint8_t  s_candidate = MATRIX_KEY_NONE;   /* 消抖中的候选键值 */
  static uint8_t  s_pressed   = MATRIX_KEY_NONE;   /* 已确认按下的键值 */
  static uint32_t s_time_ms   = 0U;
  static uint32_t s_scan_ms   = 0U;                /* 上次扫描时刻 */

  uint8_t  raw;
  uint32_t now = HAL_GetTick();

  /* 限速：1ms 扫一次就够了，没必要让 CPU 一直抱着键盘扫 */
  if ((uint32_t)(now - s_scan_ms) < (uint32_t)MATRIX_KEY_SCAN_PERIOD_MS)
  {
    return MATRIX_KEY_EVT_NONE;
  }
  s_scan_ms = now;

  raw = MatrixKey_Scan();

  switch (s_state)
  {
    /* ---------------- 空闲：等一个键出现 ---------------- */
    case KEY_ST_RELEASED:
      if (raw != MATRIX_KEY_NONE)
      {
        s_candidate = raw;
        s_time_ms   = now;
        s_state     = KEY_ST_PRESS_DEBOUNCE;
      }
      break;

    /* ---------------- 按下消抖：连续保持 DB 时间才算有效 ---------------- */
    case KEY_ST_PRESS_DEBOUNCE:
      if (raw != s_candidate)
      {
        /* 抖动，或者中途换成了另一个键：重新开始计时 */
        s_candidate = MATRIX_KEY_NONE;
        s_state     = KEY_ST_RELEASED;
      }
      else if ((uint32_t)(now - s_time_ms) >= MATRIX_KEY_DEBOUNCE_MS)
      {
        s_pressed = s_candidate;
        s_state   = KEY_ST_PRESSED;
        if (key != NULL)
        {
          *key = s_pressed;
        }
        return MATRIX_KEY_EVT_DOWN;
      }
      break;

    /* ---------------- 已按下：等它松开 ---------------- */
    case KEY_ST_PRESSED:
      if (raw == MATRIX_KEY_NONE)
      {
        s_time_ms = now;
        s_state   = KEY_ST_RELEASE_DEBOUNCE;
      }
      break;

    /* ---------------- 释放消抖 ---------------- */
    case KEY_ST_RELEASE_DEBOUNCE:
      if (raw != MATRIX_KEY_NONE)
      {
        /* 只是抖动，仍然是按下的 */
        s_state = KEY_ST_PRESSED;
      }
      else if ((uint32_t)(now - s_time_ms) >= MATRIX_KEY_DEBOUNCE_MS)
      {
        uint8_t released = s_pressed;

        s_state     = KEY_ST_RELEASED;
        s_pressed   = MATRIX_KEY_NONE;
        s_candidate = MATRIX_KEY_NONE;

        if (key != NULL)
        {
          *key = released;
        }
        return MATRIX_KEY_EVT_UP;
      }
      break;

    default:
      /* 状态异常时自动复位，避免死锁 */
      s_state     = KEY_ST_RELEASED;
      s_candidate = MATRIX_KEY_NONE;
      break;
  }

  return MATRIX_KEY_EVT_NONE;
}

/************************ (C) COPYRIGHT *****END OF FILE****/
