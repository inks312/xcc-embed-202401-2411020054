/**
  ******************************************************************************
  * @file    bsp_led.c
  * @brief   LED 底层驱动实现
  ******************************************************************************
  * 为什么用 BSRR 而不用 ODR：
  *   GPIOx->ODR 是一个 16 位的输出数据寄存器，直接赋值 "ODR = ~(1<<i)" 会
  *   把整口 16 个引脚全部改写一遍。虽然这里 PF8~PF15 没接东西，但一旦以后
  *   在同一端口上加了别的外设（比如 PF9 做 PWM），这种写法就会误伤。
  *   BSRR 是 "置位/复位寄存器"，写 1 有效、写 0 无动作：
  *       低 16 位 = 置位（输出高电平）
  *       高 16 位 = 复位（输出低电平）
  *   一次 32 位写入即可同时点亮一些灯、熄灭另一些灯，而且是硬件原子操作，
  *   不会出现 "先熄灭再点亮" 的中间状态。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "bsp_led.h"

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  初始化 LED（引脚方向/电平由 CubeMX 生成的 MX_GPIO_Init() 配置好）
  * @retval 无
  */
void LED_Init(void)
{
  LED_AllOff();
}

/**
  * @brief  显示一个 LED 图案
  * @param  lit_mask：bit0~bit7 对应 LED1~LED8，1 = 点亮
  * @note   只需要一条 BSRR 写入，执行时间固定（约几十 ns），与图案无关
  * @retval 无
  */
void LED_Write(uint16_t lit_mask)
{
  uint16_t lit;
  uint16_t off;
  uint32_t bsrr;

  lit = (uint16_t)(lit_mask & LED_MASK);            /* 需要点亮的灯 */
  off = (uint16_t)(~lit & LED_MASK);                /* 需要熄灭的灯 */

#if (LED_ACTIVE_LOW == 1)
  /* 低电平点亮：点亮的灯写复位位(高 16 位)，熄灭的灯写置位位(低 16 位) */
  bsrr = ((uint32_t)lit << 16) | (uint32_t)off;
#else
  /* 高电平点亮：点亮的灯写置位位，熄灭的灯写复位位 */
  bsrr = ((uint32_t)off << 16) | (uint32_t)lit;
#endif

  /* LED_PIN_FIRST 用于把图案对齐到起始引脚（本实验为 PF0，移位量为 0） */
  LED_PORT->BSRR = bsrr << LED_PIN_FIRST;
}

/**
  * @brief  读回当前 LED 图案
  * @retval bit=1 表示对应的灯正在点亮
  */
uint16_t LED_Get(void)
{
  uint16_t odr;

  odr = (uint16_t)(LED_PORT->ODR & LED_MASK);

#if (LED_ACTIVE_LOW == 1)
  return (uint16_t)(~odr & LED_MASK);
#else
  return odr;
#endif
}

/**
  * @brief  点亮一颗 LED
  * @param  index：0~7，对应 LED1~LED8
  * @retval 无
  */
void LED_On(uint8_t index)
{
  if (index < LED_NUM)
  {
    LED_Write((uint16_t)(LED_Get() | (uint16_t)(1UL << index)));
  }
}

/**
  * @brief  熄灭一颗 LED
  * @param  index：0~7，对应 LED1~LED8
  * @retval 无
  */
void LED_Off(uint8_t index)
{
  if (index < LED_NUM)
  {
    LED_Write((uint16_t)(LED_Get() & (uint16_t)~(1UL << index)));
  }
}

/**
  * @brief  翻转一颗 LED
  * @param  index：0~7，对应 LED1~LED8
  * @retval 无
  */
void LED_Toggle(uint8_t index)
{
  if (index < LED_NUM)
  {
    LED_Write((uint16_t)(LED_Get() ^ (uint16_t)(1UL << index)));
  }
}

/**
  * @brief  全部点亮
  * @retval 无
  */
void LED_AllOn(void)
{
  LED_Write(LED_PATTERN_ALL);
}

/**
  * @brief  全部熄灭
  * @retval 无
  */
void LED_AllOff(void)
{
  LED_Write(LED_PATTERN_NONE);
}

/************************ (C) COPYRIGHT *****END OF FILE****/
