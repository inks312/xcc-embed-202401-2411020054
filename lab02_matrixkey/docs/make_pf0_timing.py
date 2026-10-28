# -*- coding: utf-8 -*-
"""Demo: 用 Pillow 程序化生成技术时序图（非 AI 文生图）"""
import os
from PIL import Image, ImageDraw, ImageFont

OUT_DIR = r"C:\Users\29470\Documents\STM32F407ZET6\docs"
os.makedirs(OUT_DIR, exist_ok=True)
OUT = os.path.join(OUT_DIR, "pf0_led_timing.png")

# ---- 找一个能显示中文的字体 ----
CJK_CANDIDATES = [
    (r"C:\Windows\Fonts\msyh.ttc", 0),      # 微软雅黑
    (r"C:\Windows\Fonts\msyhbd.ttc", 0),
    (r"C:\Windows\Fonts\simhei.ttf", 0),    # 黑体
    (r"C:\Windows\Fonts\simsun.ttc", 0),    # 宋体
    (r"C:\Windows\Fonts\Deng.ttf", 0),      # 等线
]
FONT_PATH, FONT_IDX = None, 0
for p, i in CJK_CANDIDATES:
    if os.path.exists(p):
        FONT_PATH, FONT_IDX = p, i
        break

def font(size):
    if FONT_PATH:
        try:
            return ImageFont.truetype(FONT_PATH, size, index=FONT_IDX)
        except Exception:
            pass
    return ImageFont.load_default()

F_TITLE = font(30)
F_LABEL = font(19)
F_SMALL = font(15)
F_MONO  = font(16)

W, H = 1200, 640
BG, INK, GRID, ACC = "#ffffff", "#1a1a1a", "#d8dee6", "#0a66c2"
HI_C, LO_C = "#137333", "#b3261e"

img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)

# ---- 布局 ----
X0, X1 = 130, 1130
Y_HI, Y_LO = 190, 370
Y_AXIS = 430
T_MAX = 2.0                       # 秒
SX = (X1 - X0) / T_MAX            # 像素/秒
def tx(t): return X0 + t * SX

# ---- 标题 ----
d.text((X0, 34), "STM32F407ZET6  PF0 (L1) 闪烁时序", font=F_TITLE, fill=INK)
d.text((X0, 74), "HAL_GPIO_TogglePin(L1_GPIO_Port, L1_Pin);  +  HAL_Delay(500);",
       font=F_MONO, fill="#5f6368")

# ---- 背景网格 ----
for i in range(0, 9):             # 每 0.25 s
    t = i * 0.25
    x = tx(t)
    major = (i % 2 == 0)
    d.line([(x, Y_HI - 40), (x, Y_AXIS)], fill=GRID if not major else "#c2ccd8", width=1)
    if major:
        d.line([(x, Y_AXIS), (x, Y_AXIS + 6)], fill=INK, width=2)
        d.text((x - 16, Y_AXIS + 12), f"{t:.2f}", font=F_SMALL, fill="#5f6368")
d.text((X1 - 40, Y_AXIS + 34), "t / s", font=F_SMALL, fill="#5f6368")

# ---- 电平辅助线与标签 ----
d.line([(X0 - 14, Y_HI), (X1, Y_HI)], fill="#e8edf3", width=1)
d.line([(X0 - 14, Y_LO), (X1, Y_LO)], fill="#e8edf3", width=1)
d.text((X0 - 74, Y_HI - 12), "HIGH", font=F_LABEL, fill=HI_C)
d.text((X0 - 62, Y_LO - 12), "LOW",  font=F_LABEL, fill=LO_C)

# ---- 波形：0-0.5 高, 0.5-1.0 低, 1.0-1.5 高, 1.5-2.0 低 ----
segs = [(0.0, 0.5, Y_HI), (0.5, 1.0, Y_LO), (1.0, 1.5, Y_HI), (1.5, 2.0, Y_LO)]
for a, b, y in segs:
    d.line([(tx(a), y), (tx(b), y)], fill=ACC, width=4)
for t in (0.5, 1.0, 1.5):                       # 跳变沿
    ja = Y_HI if (t - 0.01) % 1.0 < 0.5 else Y_LO
    jb = Y_LO if ja == Y_HI else Y_HI
    d.line([(tx(t), ja), (tx(t), jb)], fill=ACC, width=4)
# 端点
d.line([(X0, Y_HI), (tx(0.0), Y_HI)], fill=ACC, width=4)

# ---- 跳变沿箭头 + 标注 ----
for t in (0.5, 1.0, 1.5):
    x = tx(t)
    d.polygon([(x, Y_AXIS - 4), (x - 6, Y_AXIS - 16), (x + 6, Y_AXIS - 16)], fill="#e8710a")
# 只标注第一个跳变沿；文字右对齐放在箭头左侧，避开 HAL_Delay 括号标注
lab = "TogglePin()"
lab_w = d.textlength(lab, font=F_SMALL)
d.text((tx(0.5) - 14 - lab_w, Y_AXIS - 30), lab, font=F_SMALL, fill="#e8710a")

# ---- HAL_Delay(500) 括号标注 ----
def bracket(x_a, x_b, y, text, color):
    d.line([(x_a, y), (x_a, y - 12)], fill=color, width=2)
    d.line([(x_b, y), (x_b, y - 12)], fill=color, width=2)
    d.line([(x_a, y - 12), (x_b, y - 12)], fill=color, width=2)
    tw = d.textlength(text, font=F_SMALL)
    d.text(((x_a + x_b) / 2 - tw / 2, y - 34), text, font=F_SMALL, fill=color)

bracket(tx(0.0), tx(0.5), Y_HI - 26, "HAL_Delay(500) 阻塞", "#9334e6")
bracket(tx(0.5), tx(1.0), Y_LO + 62, "HAL_Delay(500) 阻塞", "#9334e6")

# ---- 周期标注 ----
d.line([(tx(0.0), Y_LO + 110), (tx(1.0), Y_LO + 110)], fill="#5f6368", width=2)
for x in (tx(0.0), tx(1.0)):
    d.line([(x, Y_LO + 102), (x, Y_LO + 118)], fill="#5f6368", width=2)
d.text((tx(0.5) - 78, Y_LO + 116), "周期 T = 1 s   →  f ≈ 1 Hz",
       font=F_LABEL, fill="#5f6368")

# ---- 底部说明 ----
d.line([(X0, 578), (X1, 578)], fill=GRID, width=1)
d.text((X0, 592), "占空比 50%  ·  亮 500 ms / 灭 500 ms  ·  高电平点亮（GPIO_PIN_SET 驱动 PF0）",
       font=F_SMALL, fill="#5f6368")

img.save(OUT)
print("saved:", OUT, img.size)
print("font used:", FONT_PATH or "PIL default (no CJK)")
