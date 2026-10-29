# 实验一：LED 往返流水灯

| 项目 | 内容 |
| --- | --- |
| 实验名称 | LED 往返流水灯 |
| 姓名 / 学号 | 汪传贵 / 2411020054 |
| 班级 | 24 电信 1 班 |
| 开发板 | STM32F407ZETX |
| 开发工具 | STM32CubeIDE |
| 主频 | 168 MHz（HSI 经 PLL 倍频） |

## 一、实验目的

1. 掌握 STM32 GPIO 输出模式的配置方法；
2. 理解 `ODR` 与 `BSRR` 两种输出方式的区别，掌握用 `BSRR` 做原子位操作；
3. 掌握基于 `HAL_GetTick()` 的时间片调度写法，写出不阻塞主循环的任务；
4. 用 8 个 LED 实现往返流水灯效果。

## 二、实验原理

### 1. 硬件连接

8 个 LED 接在 **PF0～PF7**，**低电平点亮**（输出低电平 → 灯亮，输出高电平 → 灯灭）。

### 2. 为什么用 BSRR 而不是 ODR

| 方式 | 写法 | 特点 |
| --- | --- | --- |
| `ODR` | `GPIOF->ODR = ~(1 << i)` | 整端口**覆盖**写入，PF8～PF15 也被一起写成 1 |
| `BSRR` | `GPIOF->BSRR = ...` | 位**原子**置位/复位，只影响写 1 的位 |

`BSRR` 低 16 位写 1 表示"输出高电平"（灯灭），高 16 位写 1 表示"输出低电平"（灯亮）。
因此"点亮一个、熄灭其余"可以**一次写完成**：

```c
uint32_t lit = (1u << index) & LED_ALL_MASK;   /* 要亮的位 */
uint32_t off = LED_ALL_MASK & ~lit;            /* 要灭的位 */
LED_PORT->BSRR = off | (lit << 16u);
```

这样不会出现"先全灭、再点亮"的中间态，肉眼看不到闪一下。

### 3. 时间片与往返效果

- 每 **120 ms** 前进一个灯位；
- 走到 PF7 后**掉头**往回走，到 PF0 再掉头，形成 0→7→0 的往返流水；
- 用 `HAL_GetTick()`（SysTick 中断累加的毫秒计数）判断是否到时间，
  没到就直接返回，**主循环不会被卡住**。

## 三、实验步骤

1. 用 STM32CubeMX / CubeIDE 新建 STM32F407ZETX 工程，时钟树配置到 168 MHz；
2. 把 PF0～PF7 配置为推挽输出、默认输出高电平（上电全灭）；
3. 新建 `led_effect.c / led_effect.h`，实现流水灯效果；
4. 在 `main.c` 的 `while(1)` 中周期调用 `LED_Effect_Task()`；
5. 编译下载，观察 8 个 LED 的往返流动效果。

## 四、程序结构

| 文件 | 作用 |
| --- | --- |
| `Core/Src/main.c` | 初始化 + 主循环，只负责调用流水灯任务 |
| `Core/Src/led_effect.c` | 流水灯算法：方向控制、位操作、时间片 |
| `Core/Inc/led_effect.h` | 对外接口：`LED_Effect_Init()` / `LED_Effect_Task()` |
| `Core/Src/gpio.c` | 引脚初始化（PF0～PF7 推挽输出） |

主循环非常干净，后续要加按键、串口等功能，直接往下加任务即可：

```c
while (1)
{
  LED_Effect_Task();     /* 非阻塞：没到时间片立即返回 */
  /* 以后可以在这里加别的任务 */
}
```

## 五、关键代码

### 1. 一次 BSRR 完成"点亮一个 + 熄灭其余"

```c
static void led_show_only(uint8_t index)
{
  uint32_t lit = (uint32_t)(1u << index) & LED_ALL_MASK;   /* 要亮的位 */
  uint32_t off = LED_ALL_MASK & ~lit;                      /* 要灭的位 */

  LED_PORT->BSRR = off | (lit << 16u);
}
```

### 2. 非阻塞任务 + 往返方向控制

```c
void LED_Effect_Task(void)
{
  uint32_t now = HAL_GetTick();

  if ((now - s_last_tick) < LED_STEP_MS)   /* 还没到时间片 */
  {
    return;                                 /* 立即返回，不阻塞主循环 */
  }
  s_last_tick = now;

  led_show_only(s_index);

  if (s_step > 0)                           /* 向右走 */
  {
    if (s_index >= (uint8_t)(LED_COUNT - 1u))
    {
      s_step  = -1;                         /* 到头了，掉头 */
      s_index = (uint8_t)(LED_COUNT - 2u);
    }
    else { s_index++; }
  }
  else                                      /* 向左走 */
  {
    if (s_index == 0u)
    {
      s_step  = 1;                          /* 到头了，掉头 */
      s_index = 1u;
    }
    else { s_index--; }
  }
}
```

> 到端点时"掉头并立即向内走一步"，是为了避免端点灯被连续点亮两个时间片（那样看起来会卡一下）。

## 六、实验结果

8 个 LED 以 120 ms 的间隔依次点亮，在 PF0 与 PF7 之间往返流动，视觉上是一个连续的循环。

![往返流水灯现象 1](screenshots/led_water_01.jpg)
![往返流水灯现象 2](screenshots/led_water_02.jpg)
![往返流水灯现象 3](screenshots/led_water_03.jpg)

PF0 引脚电平随时间变化的时序见 [`docs/pf0_led_timing.png`](docs/pf0_led_timing.png)
（由 [`docs/make_pf0_timing.py`](docs/make_pf0_timing.py) 生成）。

## 七、与原实现的对比

| 对比项 | 原实现 | 现实现 | 改进点 |
| --- | --- | --- | --- |
| 输出方式 | `GPIOF->ODR = ~(1<<i)` 整端口覆盖 | `GPIOF->BSRR` 原子位操作 | 不误伤 PF8～PF15，无中间态闪烁 |
| 延时方式 | `delay(24000000)` 空循环 | `HAL_GetTick()` 时间片 | 不受编译优化等级/主频影响，且不阻塞 |
| 任务结构 | 主循环里 for + 阻塞延时 | `LED_Effect_Task()` 非阻塞任务 | 主循环可以继续加其他任务 |
| 视觉效果 | 0→7 后直接跳回 0 | 0→7→0 往返 | 循环连续，没有突兀的"回跳" |
| 代码组织 | 全部写在 main.c | 拆出 `led_effect.c/h` | 职责清晰，便于复用 |

## 八、说明与注意点

- **低电平点亮**：写 `BSRR` 时要注意用高 16 位（复位位）点亮、低 16 位置位熄灭，
  写反了现象就是"该亮的灭、该灭的亮"；
- **`LED_ALL_MASK` 限定在 0x00FF**：只操作 PF0～PF7，同端口其他引脚不会被改；
- 时间片是"软定时"，`HAL_GetTick()` 由 SysTick 中断驱动，中断被长时间关闭时会有累积误差，
  需要更精确的节拍可以改成 TIM 定时器中断。

## 九、文件说明

| 路径 | 内容 |
| --- | --- |
| `Core/Src/main.c` | 主程序：时钟配置、初始化、主循环 |
| `Core/Src/led_effect.c` | 流水灯效果实现（BSRR 位操作 + 时间片 + 往返控制） |
| `Core/Inc/led_effect.h` | 流水灯接口声明 |
| `Core/Src/gpio.c` | GPIO 初始化（PF0～PF7 推挽输出） |
| `Core/Inc/` | 头文件 |
| `Drivers/` | CMSIS + STM32F4xx HAL 驱动库 |
| `Startup/` | 启动文件 `startup_stm32f407zetx.s` |
| `docs/` | PF0 时序图及生成脚本 |
| `screenshots/` | 现象截图 |
| `0501_LEDWater.ioc` | CubeMX 工程配置文件 |
