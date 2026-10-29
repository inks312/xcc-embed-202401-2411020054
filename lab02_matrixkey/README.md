# 实验二：3×4 矩阵键盘与 BCD 显示（Keil5 优化版）

| 项目 | 内容 |
| --- | --- |
| 实验名称 | 3×4 矩阵键盘扫描 + 8421BCD 显示 |
| 姓名 / 学号 | 汪传贵 / 2411020054 |
| 班级 | 24 电信 1 班 |
| 开发板 | STM32F407ZETX |
| 开发工具 | Keil MDK 5（ARM Compiler 6），工程文件 `MDK-ARM/0602_MatrixKey.uvprojx`；原 STM32CubeIDE 工程同样保留可用 |

## 一、实验目的

1. 掌握矩阵键盘的行扫描、列读取原理；
2. 掌握用 `IDR` / `BSRR` 寄存器直接操作 GPIO 的方法，理解行线为什么必须用开漏输出；
3. 用"4 状态机 + 定时采样"实现非阻塞消抖，按下和释放各只产生一次事件；
4. 把键值用 8421BCD 码显示到 8 个 LED 上，并能在 Keil5 中建立、编译、下载工程。

## 二、实验原理

### 1. 硬件连接

| 端口 | 方向 | 功能 |
| --- | --- | --- |
| PE0～PE3 | 输入（上拉） | 4 根列线 COL0～COL3，无按键时为高电平 |
| PE4～PE6 | 输出（**开漏**） | 3 根行线 ROW0～ROW2 |
| PF0～PF3 | 输出 | 键值个位（8421BCD，低电平点亮） |
| PF4～PF7 | 输出 | 键值十位（8421BCD，低电平点亮） |

键值按行优先编号，与实验板丝印 K1～K12 一致：

| | PE0(COL0) | PE1(COL1) | PE2(COL2) | PE3(COL3) |
| --- | --- | --- | --- | --- |
| **PE4(ROW0)** | 1 (K1) | 2 (K2) | 3 (K3) | 4 (K4) |
| **PE5(ROW1)** | 5 (K5) | 6 (K6) | 7 (K7) | 8 (K8) |
| **PE6(ROW2)** | 9 (K9) | 10 (K10) | 11 (K11) | 12 (K12) |

### 2. 行扫描原理

逐行把一根行线拉低，其余行**释放为高阻**，然后读 4 根列线：

```
选通 ROW0（拉低）→ 等 20µs 稳定 → 读列 → 释放所有行
选通 ROW1（拉低）→ 等 20µs 稳定 → 读列 → 释放所有行
选通 ROW2（拉低）→ 等 20µs 稳定 → 读列 → 释放所有行
```

某列读到 **低电平**，就说明"当前这一行 × 这一列"的按键被按下。

> **为什么行线要用开漏输出？**
> 推挽输出写 1 是主动输出高电平；如果同时按下不同行的两个键，一个行输出高、另一个行输出低，
> 两个引脚会通过按键直接短接，形成大电流，长期可能损坏 IO。
> 开漏输出写 1 是高阻（相当于断开），只有写 0 才拉低，所以不会出现"输出对打"。

### 3. 非阻塞消抖（状态机）

按键抖动一般持续几毫秒。这里不用 `HAL_Delay` 卡住主循环，而是用 4 个状态推进：

```
          有键出现              连续 20ms 稳定
 ┌───────────────┐      ┌──────────────────┐
 │  ① 空闲        │─────>│  ② 按下消抖       │
 └───────────────┘<─────└──────────────────┘
      ↑  键没了/换键        │ 确认 → 返回 DOWN 事件
      │                     ▼
      │              ┌──────────────┐
      │              │  ③ 已按下     │ 等待松开
      │              └──────────────┘
      │                     │ 读到无键
      │                     ▼
      │              ┌──────────────┐
      └──────────────│ ④ 释放消抖    │ 20ms 稳定 → 返回 UP 事件
                     └──────────────┘
```

- 默认每 **1 ms** 扫描一次（`MATRIX_KEY_SCAN_PERIOD_MS`），单次约 60～80 µs，CPU 占用 < 10%；
- 只有"消抖后的按下"才产生 `MATRIX_KEY_EVT_DOWN`，按住不放不会反复触发；
- 松开后还会产生一次 `MATRIX_KEY_EVT_UP`，可以拿来做"松手熄灭"等扩展。

### 4. BCD 显示

键值 1～12 拆成十位和个位，按 8421 编码送到 PF 口：

```
PF7 PF6 PF5 PF4 | PF3 PF2 PF1 PF0
    十位 (0~9)     |    个位 (0~9)
```

例如键值 12 → 十位 1、个位 2 → 点亮 PF4 和 PF1（低电平点亮）。

## 三、实验步骤

1. 用 STM32CubeMX 生成 STM32F407ZETX 工程：PE0～PE3 上拉输入，PE4～PE6 输出，PF0～PF7 推挽输出；
2. **把 PE4～PE6 的输出模式改成开漏（Output Open Drain）**——这是本次优化的关键硬件改动；
3. 用 CubeMX 的 "MDK-ARM" 工具链生成 Keil 工程框架，补上启动文件，得到 `MDK-ARM/0602_MatrixKey.uvprojx`；
4. 新建 `matrix_key.c/h`，实现行扫描、查表解码、消抖状态机；
5. 在 `main.c` 中调用 `MatrixKey_GetEvent()`，收到 `DOWN` 事件就刷新 BCD 显示；
6. 用 Keil5 打开工程 → Options for Target → Debug 选 **ST-Link**、Port 选 **SW** → `F7` 编译 → `F8` 下载；
7. 逐个按键，核对 LED 显示的 BCD 码。

## 四、程序结构

| 文件 | 作用 |
| --- | --- |
| `Core/Src/main.c` | 初始化 + 主循环：取按键事件、刷新显示 |
| `Core/Src/matrix_key.c` | 键盘驱动：行扫描、列码查表、消抖状态机 |
| `Core/Inc/matrix_key.h` | 引脚宏、消抖/扫描/稳定时间、事件类型 |
| `Core/Src/gpio.c` | 引脚初始化（PE4～PE6 开漏、PE0～PE3 上拉输入） |
| `MDK-ARM/` | Keil5 工程（`.uvprojx` + ARM 启动文件 + 工程说明） |

主循环没有任何 `HAL_Delay`：

```c
while (1)
{
  uint8_t          key = MATRIX_KEY_NONE;
  MatrixKeyEvent_t ev  = MatrixKey_GetEvent(&key);   /* 非阻塞，无事件立即返回 */

  if (ev == MATRIX_KEY_EVT_DOWN)
  {
    KeyDisplay_Show(key);                            /* 只在按下瞬间刷新一次 */
  }
}
```

## 五、关键代码

### 1. 选行 + 读列（只动 PE4～PE6）

```c
static void MatrixKey_SelectRow(uint8_t row)
{
  uint32_t bit = (uint32_t)1U << (MATRIX_KEY_ROW_FIRST + row);

  /* 开漏输出：BSRR 置位 = 释放（高阻），复位 = 拉低
     一次写入同时完成"选中行拉低 + 其余行释放" */
  MATRIX_KEY_ROW_PORT->BSRR = ((uint32_t)MATRIX_KEY_ROW_MASK & ~bit) | (bit << 16);
}

uint8_t MatrixKey_Scan(void)
{
  uint8_t key = MATRIX_KEY_NONE;
  uint8_t row, col;

  for (row = 0U; row < MATRIX_KEY_ROW_NUM; row++)
  {
    MatrixKey_SelectRow(row);
    MatrixKey_SettleDelay();                       /* 等列线电平恢复，约 20µs */

    col = s_col_lut[MATRIX_KEY_COL_PORT->IDR & MATRIX_KEY_COL_MASK];

    if ((col >= 1U) && (col <= MATRIX_KEY_COL_NUM))
    {
      key = (uint8_t)(row * MATRIX_KEY_COL_NUM + col);   /* 1 ~ 12 */
      break;
    }
  }

  MatrixKey_ReleaseAllRows();
  return key;
}
```

### 2. 列码查表（16 项，多键同时为低直接判无效）

```c
static const uint8_t s_col_lut[16] =
{
  0xFFU, 0xFFU, 0xFFU, 0xFFU,   /* 0x00 ~ 0x03 无效 */
  0xFFU, 0xFFU, 0xFFU, 0x04U,   /* 0x07 = COL3 按下 */
  0xFFU, 0xFFU, 0xFFU, 0x03U,   /* 0x0B = COL2 按下 */
  0xFFU, 0x02U, 0x01U, 0x00U    /* 0x0D = COL1，0x0E = COL0，0x0F = 无键 */
};
```

### 3. 消抖状态机（节选）

```c
case KEY_ST_RELEASED:                       /* 空闲：等一个键出现 */
  if (raw != MATRIX_KEY_NONE)
  {
    s_candidate = raw;
    s_time_ms   = now;
    s_state     = KEY_ST_PRESS_DEBOUNCE;
  }
  break;

case KEY_ST_PRESS_DEBOUNCE:                 /* 按下消抖 */
  if (raw != s_candidate)
  {
    s_candidate = MATRIX_KEY_NONE;
    s_state     = KEY_ST_RELEASED;          /* 抖动或换键：重新计时 */
  }
  else if ((uint32_t)(now - s_time_ms) >= MATRIX_KEY_DEBOUNCE_MS)
  {
    s_pressed = s_candidate;
    s_state   = KEY_ST_PRESSED;
    *key      = s_pressed;
    return MATRIX_KEY_EVT_DOWN;             /* 只在这里返回一次 */
  }
  break;
```

### 4. BCD 显示

```c
static void KeyDisplay_Show(uint8_t key)
{
  uint16_t bcd = (uint16_t)(((key / 10U) << 4) | (key % 10U));
  uint16_t lit = (uint16_t)(bcd & 0x00FFU);      /* 1 = 点亮（输出低） */
  uint16_t off = (uint16_t)(~lit & 0x00FFU);     /* 1 = 熄灭（输出高） */

  GPIOF->BSRR = ((uint32_t)lit << 16) | (uint32_t)off;
}
```

## 六、实验结果

按下不同按键，LED 以 8421BCD 码显示对应键值（1～12）；按住不放显示保持稳定、不会乱跳；
松开再按才更新。上电时 8 颗灯全灭（显示 0）。

![按键现象 1](screenshots/matrixkey_01.jpg)
![按键现象 2](screenshots/matrixkey_02.jpg)
![按键现象 3](screenshots/matrixkey_03.jpg)
![按键现象 4](screenshots/matrixkey_04.jpg)
![按键现象 5](screenshots/matrixkey_05.jpg)

## 七、与原实现的对比

| 对比项 | 原实现 | 现实现 | 改进点 |
| --- | --- | --- | --- |
| 扫描顺序 | 先读列、后换行，靠 `PreLine[3]` 映射表对上号 | 选行 → 稳定 → 读列 → 释放 | 时序清楚，第一次扫描就是有效数据 |
| 行线驱动 | `GPIOE->ODR = ~(1<<(i+4))` 整端口写，推挽 | `GPIOE->BSRR` 原子写，**开漏** | 不误改其它引脚；双键按下不会输出对打 |
| 消抖方式 | `HAL_Delay(16)` 阻塞 | 1ms 采样 + 4 状态机 + 20ms 稳定判定 | 主循环不被卡住，可并发其他任务 |
| 按键判定 | 一直按住会持续刷新 | 按下/释放各只产生一次事件 | 按住不会重复触发 |
| 列判定 | `switch` 里写死 `0x0E/0x0D/0x0B/0x07` | 16 项查表 `s_col_lut[]` | 无分支；多键同时为低直接判无效 |
| 列线稳定 | 无（靠上一轮的长时间延时"顺带"恢复） | 换行后显式等 20µs | 消除"偶尔串键" |
| 代码组织 | 全部写在 main.c | 拆出 `matrix_key.c/h` | 可复用，主循环更干净 |
| 工程 | 仅 STM32CubeIDE | 增加 Keil5 工程 | 两套 IDE 都能直接编译 |

## 八、说明与注意点

- **列线是上拉输入**：没按键时为高电平，读到低电平才说明按下；PE0～PE3 不能配成 `GPIO_NOPULL`；
- **行线必须是开漏**：若用 CubeMX 重新生成代码，要在 Pinout 里把 PE4～PE6 的 GPIO mode 设为 `Output Open Drain`，否则会退回推挽；
- **换行后要等电平恢复**：列线上拉约 40 kΩ，加上排线电容，恢复需要十几微秒，`MATRIX_KEY_SETTLE_LOOPS` 调大可解决偶发串键；
- 键盘没有加二极管，属于"无防冲"接法，同时按 3 个以上键存在鬼键可能；本程序对"同一行多列同时为低"直接判为无效，避免误判；
- 键值 10～12 的个位是 0，对应个位 LED 全灭，这是 BCD 编码的正常现象，不是显示错误。

## 九、文件说明

| 路径 | 内容 |
| --- | --- |
| `Core/Src/main.c` | 主程序：时钟配置、初始化、主循环、BCD 显示 |
| `Core/Src/matrix_key.c` | 键盘驱动：扫描、查表解码、消抖状态机 |
| `Core/Inc/matrix_key.h` | 键盘引脚宏、时间参数、事件类型与接口声明 |
| `Core/Src/gpio.c` | GPIO 初始化（PE4～PE6 开漏输出、PE0～PE3 上拉输入、PF0～PF7 推挽输出） |
| `MDK-ARM/` | Keil5 工程：`0602_MatrixKey.uvprojx`、ARM 启动文件、工程说明 |
| `Drivers/` | CMSIS + STM32F4xx HAL 驱动库 |
| `Startup/` | GCC 启动文件 `startup_stm32f407zetx.s`（CubeIDE 用） |
| `docs/` | 时序图及生成脚本 |
| `screenshots/` | 现象截图 |
| `0602_MatrixKey.ioc` | CubeMX 工程配置文件 |
