/**
  ******************************************************************************
  * @file    keypad.h
  * @brief   实验二：3×4 矩阵键盘扫描（非阻塞 + 软件消抖）
  ******************************************************************************
  */

#ifndef __KEYPAD_H
#define __KEYPAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** 没有新按键时 KEYPAD_GetKey() 的返回值 */
#define KEYPAD_KEY_NONE   0u

/** 键值范围 1~12，对应 3 行 × 4 列 */
#define KEYPAD_KEY_MIN    1u
#define KEYPAD_KEY_MAX    12u

/**
  * @brief  键盘状态复位，行线全部拉高
  */
void KEYPAD_Init(void);

/**
  * @brief  取一次键值（非阻塞）
  * @retval 1~12：本次新按下的键；0：无新按键
  * @note   内部按 2ms 周期扫描、10ms 稳定判定，函数立即返回，不阻塞主循环
  */
uint8_t KEYPAD_GetKey(void);

#ifdef __cplusplus
}
#endif

#endif /* __KEYPAD_H */
