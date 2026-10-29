/**
  ******************************************************************************
  * @file    led_effect.c
  * @brief   实验一：LED 往返流水灯效果
  *
  * 设计要点：
  *   1. 用 BSRR 寄存器一次性完成"点亮一个 + 熄灭其余"，写操作是原子的，
  *      不会出现先灭后亮的中间态（直接写 ODR 是整端口覆盖，有这个隐患）。
  *   2. 计时用 HAL_GetTick()（SysTick 中断维护），不依赖空循环，
  *      换编译优化等级或改主频都不会让节奏跑偏。
  *   3. 效果改成"往返"：0→7 再从 7→0，视觉上连成一个循环，
  *      不会出现从头跳回第一位时的突兀感。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "led_effect.h"
#include "stm32f4xx_hal.h"

/* Private define ------------------------------------------------------------*/
#define LED_COUNT        8u          /* PF0~PF7 共 8 个 LED */
#define LED_STEP_MS      120u        /* 每个灯停留时间（毫秒） */
#define LED_PORT         GPIOF
#define LED_ALL_MASK     0x00FFu     /* 只操作 PF0~PF7，不碰同端口其他引脚 */

/* Private variables ---------------------------------------------------------*/
static uint8_t  s_index;             /* 当前点亮的 LED 序号 0~7 */
static int8_t   s_step;              /* 移动方向：+1 向右，-1 向左 */
static uint32_t s_last_tick;         /* 上一次推进的时间戳 */

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  只点亮序号为 index 的 LED，其余熄灭
  * @note   BSRR 低 16 位写 1 → 输出高（LED 灭）；高 16 位写 1 → 输出低（LED 亮）
  */
static void led_show_only(uint8_t index)
{
  uint32_t lit = (uint32_t)(1u << index) & LED_ALL_MASK;   /* 要亮的位 */
  uint32_t off = LED_ALL_MASK & ~lit;                      /* 要灭的位 */

  LED_PORT->BSRR = off | (lit << 16u);
}

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  初始化流水灯状态：全部熄灭，指针回到第一个 LED
  */
void LED_Effect_Init(void)
{
  s_index     = 0u;
  s_step      = 1;
  s_last_tick = HAL_GetTick();

  /* 全部置高，上电先全灭，避免上电瞬间乱亮 */
  LED_PORT->BSRR = LED_ALL_MASK;
}

/**
  * @brief  流水灯任务：非阻塞，没到时间片立即返回
  */
void LED_Effect_Task(void)
{
  uint32_t now = HAL_GetTick();

  /* 还没到时间片就直接返回，不阻塞主循环 */
  if ((now - s_last_tick) < LED_STEP_MS)
  {
    return;
  }
  s_last_tick = now;

  led_show_only(s_index);

  /* 推进指针：到两端后掉头，并立即向内走一步，避免端点被重复点亮 */
  if (s_step > 0)
  {
    if (s_index >= (uint8_t)(LED_COUNT - 1u))
    {
      s_step  = -1;
      s_index = (uint8_t)(LED_COUNT - 2u);
    }
    else
    {
      s_index++;
    }
  }
  else
  {
    if (s_index == 0u)
    {
      s_step  = 1;
      s_index = 1u;
    }
    else
    {
      s_index--;
    }
  }
}
