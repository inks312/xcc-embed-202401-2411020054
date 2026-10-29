# 0602_MatrixKey —— Keil5 工程说明

## 打开与下载

1. 双击 `0602_MatrixKey.uvprojx`；
2. `Options for Target → Debug → ST-Link Debugger → Settings`，Port 选 **SW**；
3. `F7` 编译 → `F8` 下载 → 复位运行。

## 硬件

| 引脚 | 功能 | 模式 |
| --- | --- | --- |
| PE4 / PE5 / PE6 | 行线 ROW0 / ROW1 / ROW2 | **开漏输出 + 上拉** |
| PE0 / PE1 / PE2 / PE3 | 列线 COL0 ~ COL3 | 输入 + 内部上拉 |
| PF0 ~ PF7 | 键值显示（8421BCD） | 推挽输出，低电平点亮 |

## 键值

| | PE0(COL0) | PE1(COL1) | PE2(COL2) | PE3(COL3) |
| --- | --- | --- | --- | --- |
| **PE4(ROW0)** | 1 (K1) | 2 (K2) | 3 (K3) | 4 (K4) |
| **PE5(ROW1)** | 5 (K5) | 6 (K6) | 7 (K7) | 8 (K8) |
| **PE6(ROW2)** | 9 (K9) | 10 (K10) | 11 (K11) | 12 (K12) |

显示：PF4~PF7 = 十位，PF0~PF3 = 个位。按 K12 → PF4 亮 + PF1 亮。

## 预期现象

- 上电 8 颗灯全灭；
- 每按一个键，灯按 BCD 码点亮并**保持**到下一次按键；
- 按住不放不会重复刷新，松开再按才更新；
- 同时按不同行的两个键，只认先扫描到的那个（不做多键组合）。

## 代码结构

| 文件 | 作用 |
| --- | --- |
| `Core/Src/matrix_key.c` | 扫描（选行→稳定→读列→释放）+ 4 状态消抖状态机 |
| `Core/Inc/matrix_key.h` | 引脚宏、消抖时间、稳定时间、事件类型 |
| `Core/Src/main.c` | 只处理 `MATRIX_KEY_EVT_DOWN` 事件，并用 `KeyDisplay_Show()` 显示 |
| `Core/Src/gpio.c` | PE4~PE6 改为 `GPIO_MODE_OUTPUT_OD`（关键改动） |

## 想改的地方

```c
/* matrix_key.h */
#define MATRIX_KEY_DEBOUNCE_MS   20U    /* 消抖时间，手感偏硬可改 10~30 */
#define MATRIX_KEY_SCAN_PERIOD_MS 1U    /* 扫描周期，0 = 全速扫描        */
#define MATRIX_KEY_SETTLE_LOOPS  500U   /* 换行稳定等待，串键时调到 1000 */

/* matrix_key.c: MatrixKey_Scan() 里的键值计算 */
key = (uint8_t)(row * MATRIX_KEY_COL_NUM + col);   /* 1~12，行优先 */

/* main.c: KeyDisplay_Show() 里的显示编码 */
```

## 扩展提示

`MatrixKey_GetEvent()` 还会返回 `MATRIX_KEY_EVT_UP`，可以直接拿来写"松手熄灭"：

```c
MatrixKeyEvent_t ev = MatrixKey_GetEvent(&key);
if (ev == MATRIX_KEY_EVT_DOWN) { KeyDisplay_Show(key); }
if (ev == MATRIX_KEY_EVT_UP)   { KeyDisplay_Show(0);   }
```

## 注意

- 行线**必须是开漏输出**。若用 CubeMX 重新生成代码，请在 Pinout 里把 PE4~PE6 的 GPIO mode 设为 `Output Open Drain`；
- 列线上拉是读键的前提，PE0~PE3 不能配成 `GPIO_NOPULL`；
- 键盘没有二极管，属于"无防冲"接法，同时按 3 个以上键存在鬼键可能，本程序对"同一行多列同时为低"直接判为无效。
