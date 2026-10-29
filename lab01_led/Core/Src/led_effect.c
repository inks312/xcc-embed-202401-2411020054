/**
  ******************************************************************************
  * @file    led_effect.c
  * @brief   流水灯花样引擎实现
  ******************************************************************************
  * 时间基准用的是 HAL_GetTick()，也就是 SysTick 每 1ms 加 1 的计数器，
  * 由系统时钟保证精度，和编译器优化等级、CPU 主频都没有关系
  * （这正是原代码 "delay(24000000)" 空循环延时最致命的地方：
  *   换一个优化等级或换一颗晶振，1 秒就变成 0.3 秒或 3 秒）。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "led_effect.h"
#include "bsp_led.h"

/* Private typedef -----------------------------------------------------------*/
/* 一种花样 = 一张帧表 + 节拍 + 帧数 */
typedef struct
{
  const uint8_t *frames;       /* 帧表：每一项是 8 颗灯的图案             */
  uint8_t        frame_num;    /* 帧数                                    */
  uint16_t       interval_ms;  /* 每帧停留时间（毫秒）                    */
} LedFxDef_t;

/* Private define ------------------------------------------------------------*/
/* 帧表长度（由编译器按数组大小自动算出，增删帧不用改代码） */
#define FX_FRAME_NUM(array)   ((uint8_t)(sizeof(array) / sizeof((array)[0])))

/* Private variables ---------------------------------------------------------*/
/* 四种花样的帧表：bit0 = LED1(PF0) … bit7 = LED8(PF7) */
static const uint8_t s_fx_single_run[] =
{
  0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U
};

/* 往复：去程 8 帧 + 回程 6 帧（首尾不重复，视觉上才连贯） */
static const uint8_t s_fx_pingpong[] =
{
  0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U,
  0x40U, 0x20U, 0x10U, 0x08U, 0x04U, 0x02U
};

/* 累积：每帧多亮一颗，最后一帧全亮 */
static const uint8_t s_fx_accumulate[] =
{
  0x01U, 0x03U, 0x07U, 0x0FU, 0x1FU, 0x3FU, 0x7FU, 0xFFU
};

/* 奇偶交替：10101010 / 01010101 */
static const uint8_t s_fx_alternate[] =
{
  0xAAU, 0x55U
};

/* 花样总表：顺序必须与 LedEffect_t 枚举一致 */
static const LedFxDef_t s_fx_table[LED_FX_COUNT] =
{
  { s_fx_single_run, FX_FRAME_NUM(s_fx_single_run), 250U },
  { s_fx_pingpong,   FX_FRAME_NUM(s_fx_pingpong),   200U },
  { s_fx_accumulate, FX_FRAME_NUM(s_fx_accumulate), 180U },
  { s_fx_alternate,  FX_FRAME_NUM(s_fx_alternate),  400U }
};

/* 运行状态 */
static uint8_t  s_cur_fx   = 0U;   /* 当前花样序号     */
static uint8_t  s_frame    = 0U;   /* 当前播放到第几帧 */
static uint8_t  s_cycle    = 0U;   /* 当前花样已循环几遍 */
static uint32_t s_last_ms  = 0U;   /* 上一帧的显示时刻 */

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  载入一种花样并立即显示它的第一帧
  * @param  fx：花样序号
  * @retval 无
  */
static void LedEffect_Load(uint8_t fx)
{
  const LedFxDef_t *def;

  s_cur_fx = (uint8_t)(fx % (uint8_t)LED_FX_COUNT);
  def      = &s_fx_table[s_cur_fx];

  s_frame = 0U;
  s_cycle = 0U;

  /* 让第一帧在下次调用 Task 时立即生效：把 "上次时刻" 往前推一个节拍。
     无符号数溢出是良性的（now - s_last_ms 依旧等于 interval_ms）。 */
  s_last_ms = HAL_GetTick() - def->interval_ms;
}

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  初始化花样引擎
  * @retval 无
  */
void LedEffect_Init(void)
{
  LedEffect_Load((uint8_t)LED_FX_DEFAULT_INDEX);
}

/**
  * @brief  花样引擎任务：必须在主循环中反复调用
  * @note   函数内部没有任何阻塞等待，正常情况下几个指令周期就返回
  * @retval 无
  */
void LedEffect_Task(void)
{
  const LedFxDef_t *def;
  uint32_t now;

  def = &s_fx_table[s_cur_fx];
  now = HAL_GetTick();

  /* 还没到下一帧的时间，直接返回（这就是 "非阻塞" 的全部秘密） */
  if ((uint32_t)(now - s_last_ms) < (uint32_t)def->interval_ms)
  {
    return;
  }

  /* 用 "上一次时刻 + 节拍" 递推，而不是 "now"，可以消除每帧的微小误差累积 */
  s_last_ms += def->interval_ms;

  /* 如果程序被长时间打断（例如调试断点），重新对齐，避免补播一长串帧 */
  if ((uint32_t)(now - s_last_ms) > 1000U)
  {
    s_last_ms = now;
  }

  /* 输出当前帧 */
  LED_Write(def->frames[s_frame]);

  /* 帧游标后移，播完一轮后判断是否需要切换花样 */
  s_frame++;
  if (s_frame >= def->frame_num)
  {
    s_frame = 0U;
    s_cycle++;

#if (LED_FX_AUTO_CYCLE == 1)
    if (s_cycle >= LED_FX_DWELL_CYCLES)
    {
      LedEffect_Load((uint8_t)((s_cur_fx + 1U) % (uint8_t)LED_FX_COUNT));
    }
#endif
  }
}

/**
  * @brief  手动切换到指定花样（例如由按键触发）
  * @param  fx：花样序号，超出范围会自动取模
  * @retval 无
  */
void LedEffect_Select(uint8_t fx)
{
  LedEffect_Load(fx);
}

/**
  * @brief  查询当前花样序号
  * @retval 花样序号（0 ~ LED_FX_COUNT-1）
  */
uint8_t LedEffect_Current(void)
{
  return s_cur_fx;
}

/**
  * @brief  查询花样总数
  * @retval 花样总数
  */
uint8_t LedEffect_Count(void)
{
  return (uint8_t)LED_FX_COUNT;
}

/************************ (C) COPYRIGHT *****END OF FILE****/
