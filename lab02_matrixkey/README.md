# 实验二：3×4 矩阵键盘与 BCD 显示

| 项目 | 内容 |
| --- | --- |
| 实验名称 | 3×4 矩阵键盘扫描 + 8421BCD 显示 |
| 姓名 / 学号 | 汪传贵 / 2411020054 |
| 班级 | 24 电信 1 班 |
| 开发板 | STM32F407ZETX |
| 开发工具 | STM32CubeIDE |

## 一、实验目的

1. 掌握矩阵键盘的行扫描、列读取原理；
2. 掌握用 `IDR` / `BSRR` 寄存器直接操作 GPIO 的方法；
3. 学会用"定周期采样 + 状态稳定判定"做非阻塞消抖；
4. 实现按键按下沿检测，并把键值用 8421BCD 码显示到 LED 上。

## 二、实验原理

### 1. 硬件连接

| 端口 | 方向 | 功能 |
| --- | --- | --- |
| PE0～PE3 | 输入（上拉） | 4 根列线 COL0～COL3 ，无按键时为高电平 |
| PE4～PE6 | 输出 | 3 根行线 ROW0～ROW2 |
| PF0～PF3 | 输出 | 键值个位（8421BCD，低电平点亮） |
| PF4～PF7 | 输出 | 键值十位（8421BCD，低电平点亮） |

### 2. 行扫描原理

逐行把一根行线拉低，其余行保持高电平，然后读 4 根列线：

- 某列读到 **低电平** → 该行该列的按键被按下；
- 三行扫完，得到 12 个按键的完整状态（用一个 12 位位图表示）。

```
行0 拉低 → 读列 → 记录 bit0~bit3
行1 拉低 → 读列 → 记录 bit4~bit7
行2 拉低 → 读列 → 记录 bit8~bit11
```

### 3. 非阻塞消抖

按键机械抖动一般持续几毫秒。这里不用 `HAL_Delay` 卡住主循环，而是：

1. 每 **2 ms** 采样一次 12 位位图；
2. 采样值一旦变化，就记下"变化时刻"；
3. 如果采样值**连续 10 ms 保持不变**，才认为状态真正稳定，把它作为有效状态；
4. 只有"由 0 变 1"的位（按下沿）才产生键值 —— 按住不放不会反复触发。

这样整个过程中主循环始终可以响应其他任务。

### 4. BCD 显示

键值 1～12 拆成十位和个位，按 8421 编码送到 PF 口：

```
PF7 PF6 PF5 PF4 | PF3 PF2 PF1 PF0
   十位 (0~9)     |    个位 (0~9)
```

例如键值 12 → 十位 1、个位 2 → 点亮 PF4 和 PF1（低电平点亮）。

## 三、实验步骤

1. 配置 PE0～PE3 为上拉输入，PE4～PE6、PF0～PF7 为推挽输出；
2. 新建 `keypad.c / keypad.h`，实现行扫描、消抖、按下沿检测；
3. 在 `main.c` 中周期调用 `KEYPAD_GetKey()`，有键值就调用 `display_bcd()` 刷新显示；
4. 编译下载，逐个按键，核对 LED 显示的 BCD 码。

## 四、程序结构

| 文件 | 作用 |
| --- | --- |
| `Core/Src/main.c` | 初始化 + 主循环：取键值、刷新显示 |
| `Core/Src/keypad.c` | 键盘驱动：行扫描、消抖、按下沿检测、键值解码 |
| `Core/Inc/keypad.h` | 对外接口：`KEYPAD_Init()` / `KEYPAD_GetKey()` |
| `Core/Src/gpio.c` | 引脚初始化 |

主循环没有任何 `HAL_Delay`：

```c
while (1)
{
  key = KEYPAD_GetKey();          /* 非阻塞，没按键立即返回 0 */
  if (key != KEYPAD_KEY_NONE)
  {
    display_bcd(key);             /* 只在按下瞬间刷新一次 */
  }
}
```

## 五、关键代码

### 1. 行扫描：只动 PE4～PE6

```c
static void row_drive_low(uint8_t row)
{
  uint32_t pin = (uint32_t)ROW_FIRST_PIN << row;

  ROW_PORT->BSRR = (uint32_t)ROW_MASK;    /* 先三行全部拉高 */
  ROW_PORT->BSRR = pin << 16u;            /* 再拉低选中行（BR 位写 1 → 输出低） */
}

static uint16_t scan_raw(void)
{
  uint16_t bitmap = 0u;

  for (row = 0u; row < ROW_COUNT; row++)
  {
    row_drive_low(row);
    for (volatile uint32_t wait = 0u; wait < SETTLE_LOOPS; wait++)
    {
      __NOP();                            /* 等列线电平稳定 */
    }
    cols = COL_PORT->IDR & (uint32_t)COL_MASK;

    for (col = 0u; col < COL_COUNT; col++)
    {
      if ((cols & (uint32_t)(1u << col)) == 0u)     /* 列线被拉低 = 按下 */
      {
        bitmap |= (uint16_t)(1u << (row * COL_COUNT + col));
      }
    }
  }

  row_release_all();
  return bitmap;
}
```

### 2. 消抖与按下沿检测

```c
if ((now - s_scan_tick) >= SCAN_PERIOD_MS)      /* 每 2ms 采样一次 */
{
  raw = scan_raw();
  s_scan_tick = now;

  if (raw != s_raw_prev)                        /* 还在抖，重新计时 */
  {
    s_raw_prev = raw;
    s_raw_tick = now;
  }
  else if ((raw != s_stable_mask) && ((now - s_raw_tick) >= DEBOUNCE_MS))
  {
    /* 连续 10ms 稳定 → 正式生效，只取按下沿 */
    pressed_edge  = (uint16_t)(raw & (uint16_t)(~s_stable_mask));
    s_stable_mask = raw;

    if (pressed_edge != 0u)
    {
      s_pending = decode_key(pressed_edge);
    }
  }
}
```

### 3. BCD 显示

```c
static void display_bcd(uint8_t value)
{
  uint32_t bcd = ((uint32_t)(value / 10u) << 4u) | (uint32_t)(value % 10u);
  uint32_t lit = bcd & DISPLAY_MASK;

  DISPLAY_PORT->BSRR = (DISPLAY_MASK & ~lit) | (lit << 16u);
}
```

## 六、实验结果

按下不同按键，LED 以 8421BCD 码显示对应键值（1～12），按住不放显示保持稳定、不会乱跳。

![按键现象 1](screenshots/matrixkey_01.jpg)
![按键现象 2](screenshots/matrixkey_02.jpg)
![按键现象 3](screenshots/matrixkey_03.jpg)
![按键现象 4](screenshots/matrixkey_04.jpg)
![按键现象 5](screenshots/matrixkey_05.jpg)

## 七、与原实现的对比

| 对比项 | 原实现 | 现实现 | 改进点 |
| --- | --- | --- | --- |
| 消抖方式 | `HAL_Delay(16)` 阻塞 | 2 ms 采样 + 10 ms 稳定判定 | 主循环不被卡住，可并发其他任务 |
| 按键判定 | 一直按住会持续刷新键值 | 只在按下沿产生一次 | 按住不会重复触发 |
| 行线驱动 | `GPIOE->ODR = ~(1<<(i+4))` 整端口写 | `GPIOE->BSRR` 原子位操作 | 不会把输入脚 PE0～PE3 一起写掉 |
| 列判定 | `switch` 里写死 `0x0E/0x0D/0x0B/0x07` | 按位循环判断 | 换键盘接线只需改宏 |
| 行序映射 | `PreLine[3]` 数组塞在 main 里 | `s_row_map[]` 放在驱动里并注释 | 语义清楚，换板只改一张表 |
| 代码组织 | 全部写在 main.c | 拆出 `keypad.c/h` | 可复用，主循环更干净 |

## 八、说明与注意点

- **列线是上拉输入**：没按键时为高电平，读到低电平才说明按下；
  换行后要给列线一点时间恢复高电平（`SETTLE_LOOPS` 空转），否则会误判；
- **行扫描顺序 ≠ 键盘行号**：开发板接线可能是反的，所以用 `s_row_map[]` 做一层映射，
  如果发现键值对不上，先改这张表；
- 同时按下多个键时只取**编号最小**的那个（`decode_key` 返回第一个置位的 bit）；
- 12 个键的 BCD 显示中，键值 10～12 的个位是 0，对应个位 LED 全灭，这是硬件决定的，
  不是显示错误。

## 九、文件说明

| 路径 | 内容 |
| --- | --- |
| `Core/Src/main.c` | 主程序：时钟配置、初始化、主循环、BCD 显示 |
| `Core/Src/keypad.c` | 键盘驱动：扫描、消抖、按下沿检测、键值解码 |
| `Core/Inc/keypad.h` | 键盘接口声明 |
| `Core/Src/gpio.c` | GPIO 初始化（PE/PF 端口方向配置） |
| `Core/Inc/` | 头文件 |
| `Drivers/` | CMSIS + STM32F4xx HAL 驱动库 |
| `Startup/` | 启动文件 `startup_stm32f407zetx.s` |
| `docs/` | 时序图及生成脚本 |
| `screenshots/` | 现象截图 |
| `0602_MatrixKey.ioc` | CubeMX 工程配置文件 |
