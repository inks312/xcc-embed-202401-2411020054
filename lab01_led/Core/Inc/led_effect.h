/**
  ******************************************************************************
  * @file    led_effect.h
  * @brief   实验一：LED 往返流水灯效果
  ******************************************************************************
  */

#ifndef __LED_EFFECT_H
#define __LED_EFFECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
  * @brief  初始化流水灯状态：全部熄灭，指针停在第一个 LED
  */
void LED_Effect_Init(void);

/**
  * @brief  流水灯任务：非阻塞，调用后立即返回
  * @note   内部按时间片推进，主循环可以继续执行其他任务
  */
void LED_Effect_Task(void);

#ifdef __cplusplus
}
#endif

#endif /* __LED_EFFECT_H */
