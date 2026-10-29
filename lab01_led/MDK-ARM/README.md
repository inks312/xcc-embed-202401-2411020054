# 0501_LEDWater —— Keil5 工程说明

## 打开与下载

1. 双击 `0501_LEDWater.uvprojx`；
2. `Options for Target → Debug → ST-Link Debugger → Settings`，Port 选 **SW**；
3. `F7` 编译 → `F8` 下载 → 复位运行。

## 硬件

| 引脚 | 功能 | 说明 |
| --- | --- | --- |
| PF0 ~ PF7 | LED1 ~ LED8 (L1~L8) | 推挽输出，**低电平点亮** |

## 预期现象

上电后 8 颗灯轮播 4 种花样，每种循环 2 遍后自动切换：

1. 单向流水：L1 → L8，每颗 250ms；
2. 往复流水：L1 ↔ L8；
3. 逐位累积：1 颗、2 颗……全亮；
4. 奇偶交替：1357 / 2468 两组灯交替闪。

## 代码结构

| 文件 | 作用 |
| --- | --- |
| `Core/Src/main.c` | 初始化 + 主循环（只有一行 `LedEffect_Task()`） |
| `Core/Src/bsp_led.c` | LED 底层驱动，全部通过 `BSRR` 原子写 |
| `Core/Src/led_effect.c` | 表驱动的花样引擎，用 `HAL_GetTick()` 做节拍 |
| `Core/Inc/bsp_led.h` | 硬件参数宏（端口/起始引脚/数量/极性） |
| `Core/Inc/led_effect.h` | 花样序号 + 三个可配置宏 |

## 想改的地方

```c
/* led_effect.h */
#define LED_FX_AUTO_CYCLE     1    /* 0 = 只播一种花样            */
#define LED_FX_DWELL_CYCLES   2    /* 每种花样循环几遍后切换       */
#define LED_FX_DEFAULT_INDEX  0    /* 0 = 单向流水（经典实验效果） */

/* led_effect.c: s_fx_table[] */
{ s_fx_single_run, FX_FRAME_NUM(s_fx_single_run), 250U },  /* 250 = 每帧毫秒数 */
```

## 注意

- 如果你的板子是**高电平点亮**，把 `bsp_led.h` 里的 `LED_ACTIVE_LOW` 改成 `0`，其它代码不用动；
- `GPIOF->BSRR` 只改 PF0~PF7，PF8~PF15 可以放心接别的外设；
- 主循环没有阻塞延时，可以直接在后面追加按键、串口等任务。
