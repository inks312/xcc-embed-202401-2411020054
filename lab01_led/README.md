# 实验一：LED 流水灯

| 项目 | 内容 |
| --- | --- |
| 实验名称 | LED 流水灯 |
| 姓名 / 学号 | 汪传贵 / 2411020054 |
| 班级 | 24 电信 1 班 |
| 开发板 | STM32F407ZETX |
| 开发工具 | STM32CubeIDE |

## 一、实验目的

1. 掌握 STM32 GPIO 输出模式的配置方法与寄存器操作；
2. 熟悉 STM32CubeIDE 工程的建立、编译与下载流程；
3. 用 8 个 LED 实现流水灯效果，理解位操作控制端口输出的方法。

## 二、实验原理

- LED 接在 GPIOF 的 PF0～PF7 上，**低电平点亮**；
- 直接写 `GPIOF->ODR` 输出寄存器，一次改变 8 个引脚的电平；
- 让第 `i` 位为 0、其余位为 1（`~(1U << i)`），即只有第 `i` 个 LED 点亮；
- `i` 从 0 循环到 7，配合约 1 秒的软件延时，形成"流水"效果。

系统时钟由 HSI 经 PLL 倍频到 168 MHz（PLLM = 8、PLLN = 168、PLLP = 2），
因此在 168 MHz 下空循环 24 000 000 次约为 1 秒。

## 三、实验步骤

1. 用 STM32CubeMX / CubeIDE 新建 STM32F407ZETX 工程，配置时钟树到 168 MHz；
2. 将 PF0～PF7 配置为推挽输出；
3. 在 `main.c` 的 `while(1)` 中实现流水灯循环；
4. 编译、下载到开发板，观察 LED 流水效果。

## 四、关键代码

```c
/* LED1~LED8 逐位点亮，形成流水灯效果（LED 低电平点亮） */
for (i = 0; i < 8; i++)
{
  GPIOF->ODR = ~(1U << i);   /* 仅第 i 位为 0，其余位为 1 */
  delay(24000000);           /* 软件延时，约 1 秒 */
}
```

```c
/* 软件延时：空循环 + __NOP() */
void delay(uint32_t i)
{
  while (i--)
  {
    __NOP();
  }
}
```

## 五、实验结果

8 个 LED 依次点亮、循环流动，每个 LED 点亮约 1 秒。

![流水灯现象 1](screenshots/led_water_01.jpg)
![流水灯现象 2](screenshots/led_water_02.jpg)
![流水灯现象 3](screenshots/led_water_03.jpg)

PF0 引脚电平随时间变化的时序见 [`docs/pf0_led_timing.png`](docs/pf0_led_timing.png)
（由 [`docs/make_pf0_timing.py`](docs/make_pf0_timing.py) 生成）。

## 六、说明与注意点

- **低电平点亮**：`~(1U << i)` 取反后只有目标位为 0，写 `ODR` 时其余位保持 1，
  所以不会影响其他 LED；
- **软件延时不精确**：`delay()` 是空循环阻塞延时，实际时间受主频、编译优化等级、
  中断影响，只用于演示，需要精确定时时应改用 `HAL_Delay()` 或定时器；
- 直接操作 `ODR` 是整端口写入，若同一端口还有其他功能的引脚需改用 `BSRR` 置位/复位，
  避免误改其他位。

## 七、文件说明

| 路径 | 内容 |
| --- | --- |
| `Core/Src/main.c` | 主程序：时钟配置、流水灯循环、软件延时 |
| `Core/Src/gpio.c` | GPIO 初始化（PF0～PF7 推挽输出） |
| `Core/Inc/` | 头文件 |
| `Drivers/` | CMSIS + STM32F4xx HAL 驱动库 |
| `Startup/` | 启动文件 `startup_stm32f407zetx.s` |
| `docs/` | PF0 时序图及生成脚本 |
| `screenshots/` | 现象截图 |
| `0501_LEDWater.ioc` | CubeMX 工程配置文件 |
