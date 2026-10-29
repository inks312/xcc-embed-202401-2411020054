/**
  ******************************************************************************
  * @file    bsp_led.h
  * @brief   LED 底层驱动（PF0~PF7，低电平点亮）
  ******************************************************************************
  * 设计要点：
  *   1) 只操作本模块用到的 8 个引脚，写 BSRR 寄存器一条指令完成，
  *      不会像 "GPIOF->ODR = 0xXXXX" 那样把 PF8~PF15 一起改写。
  *   2) 对外接口统一用位掩码：bit0~bit7 对应 LED1~LED8，1 表示点亮。
  *      上层的流水灯花样只需给出 "哪些灯该亮" 的字节，与高低电平无关。
  *   3) 低电平点亮这一硬件特性在本文件内消化，上层代码不用关心 ACTIVE_LOW。
  ******************************************************************************
  */
#ifndef __BSP_LED_H
#define __BSP_LED_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "stm32f4xx_hal.h"

/* 硬件参数：换板子时只改这一段 ------------------------------------------------*/
#define LED_PORT          GPIOF          /* LED 所在端口                       */
#define LED_PIN_FIRST     0U             /* 起始引脚号：PF0                    */
#define LED_NUM           8U             /* LED 个数                           */
#define LED_ACTIVE_LOW    1              /* 1：低电平点亮；0：高电平点亮       */

#define LED_MASK          ((uint16_t)((1UL << LED_NUM) - 1UL))   /* 0x00FF */
#define LED_PATTERN_ALL   LED_MASK       /* 全亮图案 */
#define LED_PATTERN_NONE  ((uint16_t)0U) /* 全灭图案 */

/* Exported functions --------------------------------------------------------*/
/* 初始化：保证上电时 LED 全灭（引脚方向由 MX_GPIO_Init() 配置） */
void     LED_Init(void);

/* 一次性显示一个图案（bit=1 点亮），内部使用 BSRR 原子写 */
void     LED_Write(uint16_t lit_mask);

/* 读回当前图案（bit=1 表示亮） */
uint16_t LED_Get(void);

/* 单颗 LED 操作，index = 0~7 对应 LED1~LED8 */
void     LED_On(uint8_t index);
void     LED_Off(uint8_t index);
void     LED_Toggle(uint8_t index);

/* 全亮 / 全灭 */
void     LED_AllOn(void);
void     LED_AllOff(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LED_H */

/************************ (C) COPYRIGHT *****END OF FILE****/
