/**
  ******************************************************************************
  * @file    led_effect.h
  * @brief   流水灯花样引擎（表驱动 + 非阻塞）
  ******************************************************************************
  * 思路：
  *   把每一种花样写成一张 "帧表"（每个字节 = 8 颗灯的状态），
  *   再给这张表配一个节拍时间。主循环里每调用一次 LedEffect_Task()，
  *   引擎只做一次 "现在到时间了吗" 的判断：
  *       到时间  → 输出下一帧，游标后移
  *       没到点  → 立即返回（耗时只有几个时钟周期）
  *
  *   因此主循环从来不 "卡" 在延时里，CPU 的 99.99% 时间都是空闲的，
  *   随时可以插入按键扫描、串口收发、ADC 采样等其它任务。
  ******************************************************************************
  */
#ifndef __LED_EFFECT_H
#define __LED_EFFECT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* 可配置参数 ----------------------------------------------------------------*/
/* 1 = 自动轮播所有花样；0 = 只播放下面指定的那一种 */
#ifndef LED_FX_AUTO_CYCLE
#define LED_FX_AUTO_CYCLE        1
#endif

/* 每个花样循环播放多少遍后切换到下一种（仅自动轮播时有效） */
#ifndef LED_FX_DWELL_CYCLES
#define LED_FX_DWELL_CYCLES      2U
#endif

/* 上电后从第几种花样开始（0 = 单向流水，即实验要求的经典效果） */
#ifndef LED_FX_DEFAULT_INDEX
#define LED_FX_DEFAULT_INDEX     0U
#endif

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  LED_FX_SINGLE_RUN = 0,   /* 单向流水：一颗灯从 LED1 移到 LED8            */
  LED_FX_PINGPONG,         /* 往复流水：LED1→LED8→LED1 来回扫描            */
  LED_FX_ACCUMULATE,       /* 逐位累积：1 颗、2 颗…全亮，然后整体熄灭      */
  LED_FX_ALTERNATE,        /* 奇偶交替：1357 / 2468 两组灯交替闪烁         */
  LED_FX_COUNT             /* 花样总数（不是花样，放在最后用于计数）        */
} LedEffect_t;

/* Exported functions --------------------------------------------------------*/
void    LedEffect_Init(void);                       /* 初始化并显示第一种花样 */
void    LedEffect_Task(void);                       /* 主循环中反复调用（非阻塞） */
void    LedEffect_Select(uint8_t fx);               /* 手动切换到第 fx 种花样   */
uint8_t LedEffect_Current(void);                    /* 当前花样序号             */
uint8_t LedEffect_Count(void);                      /* 花样总数                 */

#ifdef __cplusplus
}
#endif

#endif /* __LED_EFFECT_H */

/************************ (C) COPYRIGHT *****END OF FILE****/
