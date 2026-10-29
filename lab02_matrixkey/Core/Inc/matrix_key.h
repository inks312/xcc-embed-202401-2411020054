/**
  ******************************************************************************
  * @file    matrix_key.h
  * @brief   3×4 矩阵键盘驱动（行线开漏输出 + 列线上拉输入）
  ******************************************************************************
  * 硬件连接（与实验板一致）：
  *   行  ROW0/ROW1/ROW2 = PE4 / PE5 / PE6   —— 开漏输出
  *   列  COL0~COL3      = PE0 / PE1 / PE2 / PE3 —— 内部上拉输入
  *   键值 K1~K12 = 行号 × 4 + 列号 + 1
  *
  * 设计要点：
  *   1) 一次只把一行拉低，其余行 "释放"（开漏输出写 1 即高阻）。这样即使
  *      同时按下两个不同行的键，也不会出现 "一个输出拉高、另一个输出拉低"
  *      被按键直接短接的情况（推挽输出则会有大电流，长期可能损坏引脚）。
  *   2) 选行和释放行都用 BSRR 一次写入完成，不碰 PE 口其它引脚。
  *   3) 消抖做成状态机：每次调用只做一次扫描 + 一次时间比较，不调用
  *      HAL_Delay()，所以按键扫描不会阻塞主循环。
  ******************************************************************************
  */
#ifndef __MATRIX_KEY_H
#define __MATRIX_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "stm32f4xx_hal.h"

/* 硬件参数 ------------------------------------------------------------------*/
#define MATRIX_KEY_ROW_NUM      3U            /* 行数                        */
#define MATRIX_KEY_COL_NUM      4U            /* 列数                        */
#define MATRIX_KEY_COUNT        (MATRIX_KEY_ROW_NUM * MATRIX_KEY_COL_NUM)

#define MATRIX_KEY_NONE         0U            /* 没有按键                    */

#define MATRIX_KEY_ROW_PORT     GPIOE         /* 行线端口                    */
#define MATRIX_KEY_ROW_FIRST    4U            /* 行线起始引脚：PE4           */
#define MATRIX_KEY_ROW_MASK     0x0070U       /* PE4 | PE5 | PE6             */

#define MATRIX_KEY_COL_PORT     GPIOE         /* 列线端口                    */
#define MATRIX_KEY_COL_MASK     0x000FU       /* PE0 | PE1 | PE2 | PE3       */

/* 可调参数 ------------------------------------------------------------------*/
/* 消抖时间：机械按键抖动一般 5~20ms，20ms 比较稳妥 */
#ifndef MATRIX_KEY_DEBOUNCE_MS
#define MATRIX_KEY_DEBOUNCE_MS   20U
#endif

/* 扫描周期：至少隔这么久再扫下一次（0 = 不限速，主循环有多快就扫多快）。
   一次完整扫描要驱动 3 行并等待稳定，约 60~80µs；限制到 1ms 扫一次，
   CPU 占用只有 6%~8%，剩下的时间全部留给其它任务。 */
#ifndef MATRIX_KEY_SCAN_PERIOD_MS
#define MATRIX_KEY_SCAN_PERIOD_MS 1U
#endif

/* 换行后等待列线电平稳定的空循环次数，168MHz 下约 20~25µs。
   列线上拉电阻约 40k，加上键盘排线的分布电容，电平恢复需要几 µs~十几 µs，
   直接读可能读到上一行的残留电平——这是很多矩阵键盘程序"偶尔串键"的原因。 */
#ifndef MATRIX_KEY_SETTLE_LOOPS
#define MATRIX_KEY_SETTLE_LOOPS  500U
#endif

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  MATRIX_KEY_EVT_NONE = 0,    /* 无事件                    */
  MATRIX_KEY_EVT_DOWN,        /* 有键按下（消抖后，只报一次）*/
  MATRIX_KEY_EVT_UP           /* 按键释放（消抖后，只报一次）*/
} MatrixKeyEvent_t;

/* Exported functions --------------------------------------------------------*/
/* 初始化：释放所有行线（引脚方向由 MX_GPIO_Init() 配置；消抖状态机由
   静态变量自动初始化为"空闲"状态） */
void            MatrixKey_Init(void);

/* 单次扫描（不含消抖），返回 1~12；无键或无效（多键同列）返回 0 */
uint8_t         MatrixKey_Scan(void);

/* 带消抖的事件获取：主循环中反复调用，没有事件时立即返回 */
MatrixKeyEvent_t MatrixKey_GetEvent(uint8_t *key);

#ifdef __cplusplus
}
#endif

#endif /* __MATRIX_KEY_H */

/************************ (C) COPYRIGHT *****END OF FILE****/
