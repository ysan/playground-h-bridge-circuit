#!/usr/bin/env python3
"""docs/02-half-bridge.md の配線図 (ASCII) を生成する。

使い方:
    python3 tools/gen_wiring_ascii.py stage1   # 段階 1: MOSFET なし (VS は GND に直結)
    python3 tools/gen_wiring_ascii.py stage2   # 段階 2 以降: MOSFET あり (全部入り)
    python3 tools/gen_wiring_ascii.py power    # 5.1 節の電源部の概略図

出力を docs のコードブロックに貼り付ける。図の中は ASCII だけにする
(日本語は表示幅が環境で変わり、列がずれるため。行末に置く注記は除く)。
"""
import sys

# ---------------------------------------------------------------- 共通の描画
H, W = 40, 130
g = [[" "] * W for _ in range(H)]


def put(r, c, t):
    for k, ch in enumerate(t):
        g[r][c + k] = ch


def hline(r, c0, c1):
    for c in range(c0, c1):
        g[r][c] = "-"


def vline(c, r0, r1):
    for r in range(r0, r1):
        g[r][c] = "|"


def dump():
    print("\n".join("".join(r).rstrip() for r in g).strip("\n"))


# ---------------------------------------------------------------- 電源部の概略図
def power():
    M, J, CB = 36, 25, 9   # MOSFET の D/S の列、R_gs の分岐の列、C_bus の列
    put(0, 1, "+12V"); hline(0, 6, M); g[0][CB] = "*"; g[0][M] = "+"
    vline(CB, 1, 7); put(7, CB - 1, "---"); put(8, CB - 1, "---"); vline(CB, 9, 16); put(7, CB + 3, "C_bus")
    # Q_H
    vline(M, 1, 2); put(2, M, "D"); put(3, M - 2, "G"); put(3, M + 3, "Q_H"); put(4, M, "S"); vline(M, 5, 8)
    hline(3, 14, M - 2); put(3, 12, "HO"); put(3, 16, "[Rg_H]"); g[3][J] = "*"
    vline(J, 4, 6); g[6][J] = "+"; hline(6, J + 1, M); put(6, J + 2, "[100k]"); g[6][M] = "*"
    put(5, J - 8, "R_gs_H")
    # 中点
    g[8][M] = "*"; put(8, M + 1, "---- VS (6番ピン)、負荷へ")
    # Q_L
    vline(M, 9, 10); put(10, M, "D"); put(11, M - 2, "G"); put(11, M + 3, "Q_L"); put(12, M, "S"); vline(M, 13, 16)
    hline(11, 14, M - 2); put(11, 12, "LO"); put(11, 16, "[Rg_L]"); g[11][J] = "*"
    vline(J, 12, 14); g[14][J] = "+"; hline(14, J + 1, M); put(14, J + 2, "[10k]"); g[14][M] = "*"
    put(13, J - 8, "R_gs_L")
    put(16, 1, "GND "); hline(16, 5, M); g[16][CB] = "*"; g[16][M] = "+"


# ---------------------------------------------------------------- 配線例 (ATtiny + IRS2003)
A = [("1 VCC", "GND 14"), ("2 PA4", "PA3 13"), ("3 PA5", "PA2 12"), ("4 PA6", "PA1 11"),
     ("5 PA7", "PA0 10"), ("6 PB3", "PB0  9"), ("7 PB2", "PB1  8")]
I = [("1 VCC", "VB 8"), ("2 HIN", "HO 7"), ("3 /LIN", "VS 6"), ("4 COM", "LO 5")]


def arow(r): return "|" + " " + A[r][0].ljust(9) + A[r][1].rjust(6) + " |"
def irow(r): return "|" + " " + I[r][0].ljust(9) + I[r][1].rjust(4) + " |"


def box_top(w): return "+" + "-" * ((w - 5) // 2) + "\\_/" + "-" * (w - 5 - (w - 5) // 2) + "+"
def box_bot(w): return "+" + "-" * (w - 2) + "+"


O = 6                       # IRS2003 の名前を書く行
AX, IX = 14, 44             # ATtiny、IRS2003 の左端の列
AW, IW = len(arow(0)), len(irow(0))
IR = IX + IW                # IRS2003 の右端の外側の列
C_LO, C_VB, C_VS = IR + 1, IR + 4, IR + 7


def logic_side():
    """IRS2003、ATtiny1604、その間の配線、プルダウンとプルアップ、C_vcc。"""
    o = O
    put(o, IX + 4, "IRS2003S"); put(o + 1, IX, box_top(IW))
    for r in range(4):
        put(o + 2 + r, IX, irow(r))
    put(o + 6, IX, box_bot(IW))
    put(o + 2, IX - 11, "+12V ------"); put(o + 5, IX - 7, "GND ---")
    # ATtiny1604
    t = o + 8
    put(t, AX + 4, "ATtiny1604"); put(t + 1, AX, box_top(AW))
    for r in range(7):
        put(t + 2 + r, AX, arow(r))
    put(t + 9, AX, box_bot(AW))
    put(t + 2, AX - 6, "+5V --")
    # PA4 -> HIN、PA5 -> /LIN (ATtiny の左側から回す)
    yA4, yA5, x4, x5 = t + 3, t + 4, 2, 5
    hline(yA4, x4, AX); hline(o + 3, x4, IX)
    hline(yA5, x5, AX); hline(o + 4, x5, IX)
    vline(x4, o + 3, yA4); vline(x5, o + 4, yA5)
    for (x, y) in ((x4, yA4), (x4, o + 3), (x5, yA5), (x5, o + 4)):
        g[y][x] = "+"
    g[yA4][x5] = "|"        # PA4 の横線と PA5 の縦線は交差するだけ (接続しない)
    # プルダウン (HIN)、プルアップ (/LIN)
    pd = 12; g[o + 3][pd] = "*"; g[o + 2][pd] = "|"; put(o + 1, pd - 2, "[10k]"); g[o][pd] = "|"; put(o - 1, pd - 1, "GND")
    pu = 33; g[o + 4][pu] = "*"; g[o + 5][pu] = "|"; put(o + 6, pu - 2, "[10k]"); g[o + 7][pu] = "|"; put(o + 8, pu - 1, "+5V")
    # C_vcc (VCC のスタブから上へ)
    cv = IX - 6
    vline(cv, o + 1, o + 2); g[o + 2][cv] = "*"
    put(o, cv - 1, "---"); put(o - 1, cv - 1, "---"); g[o - 2][cv] = "|"; put(o - 3, cv - 1, "GND")
    put(o - 1, cv - 17, "C_vcc 10u+0.1u")


def bootstrap():
    """D_boot (VB から上へ) と C_boot (VB から下へ。下側は呼び出し側で VS へつなぐ)。"""
    o = O
    put(o + 2, IR, "----")
    vline(C_VB, o, o + 2); g[o + 2][C_VB] = "*"
    put(o - 1, C_VB - 2, "-----"); g[o - 2][C_VB] = "v"; vline(C_VB, o - 4, o - 2)
    put(o - 5, C_VB - 2, "+12V"); put(o - 2, C_VB + 3, "D_boot")
    for r in range(o + 3, o + 7):
        g[r][C_VB] = "|"
    put(o + 7, C_VB - 1, "---"); put(o + 8, C_VB - 1, "---")
    g[o + 9][C_VB] = "+"; hline(o + 9, C_VB + 1, C_VS); g[o + 9][C_VS] = "*"
    put(o + 8, C_VS + 2, "C_boot 1u")


def stage1():
    o = O
    logic_side()
    bootstrap()
    # HO: オシロで観察
    hline(o + 3, IR, IR + 12); put(o + 3, IR + 12, "> HO (scope)")
    # VS: GND に直結
    hline(o + 4, IR, C_VS); g[o + 4][C_VS] = "+"
    vline(C_VS, o + 5, o + 9); vline(C_VS, o + 10, o + 11); put(o + 11, C_VS - 1, "GND")
    put(o + 5, C_VS + 3, "<- stage 1: VS = GND")
    # LO: 下へ回してオシロで観察
    hline(o + 5, IR, C_LO); g[o + 5][C_LO] = "+"
    vline(C_LO, o + 6, o + 13); g[o + 13][C_LO] = "+"; hline(o + 13, C_LO + 1, C_LO + 12)
    put(o + 13, C_LO + 12, "> LO (scope)")
    # C_boot の縦線が HO、VS の横線と交差する箇所は、縦線を優先して描く
    for r in (o + 3, o + 4):
        g[r][C_VB] = "|"


def stage2():
    o = O
    logic_side()
    bootstrap()
    QX = IR + 15; CX = QX + 6

    def qbox(top, name):
        put(top, QX, "+-----+-----+"); put(top + 1, QX, "| D         |")
        put(top + 2, QX, "| G   " + name + "   |"); put(top + 3, QX, "| S         |"); put(top + 4, QX, "+-----+-----+")
    qH, qL = o + 1, o + 9
    qbox(qH, "Q_H"); qbox(qL, "Q_L")
    put(o, CX - 2, "+12V")
    vline(CX, qH + 5, o + 7); g[o + 7][CX] = "*"; vline(CX, o + 8, qL)
    vline(CX, qL + 5, qL + 6); put(qL + 6, CX - 1, "GND")
    put(o + 7, CX + 1, "---- Load ---- GND")
    # HO -> Rg_H -> Q_H.G、R_gs_H (100k) を G と S の間に
    hline(o + 3, IR, QX); put(o + 3, QX - 9, "[Rg_H]"); g[o + 3][QX - 2] = "*"
    vline(QX - 2, o + 4, o + 6); g[o + 6][QX - 2] = "+"; put(o + 6, QX - 1, "-[100k]"); g[o + 6][CX] = "*"
    # VS -> 中点
    hline(o + 4, IR, C_VS); g[o + 4][C_VS] = "+"
    vline(C_VS, o + 5, o + 9); g[o + 7][C_VS] = "*"; hline(o + 7, C_VS + 1, CX)
    # LO -> Rg_L -> Q_L.G、R_gs_L (10k) を G と S の間に
    hline(o + 5, IR, C_LO); g[o + 5][C_LO] = "+"
    vline(C_LO, o + 6, qL + 2); g[qL + 2][C_LO] = "+"
    hline(qL + 2, C_LO + 1, QX); put(qL + 2, QX - 9, "[Rg_L]"); g[qL + 2][QX - 2] = "*"
    vline(QX - 2, qL + 3, qL + 5); g[qL + 5][QX - 2] = "+"; put(qL + 5, QX - 1, "-[10k]-"); g[qL + 5][CX] = "*"
    # C_bus (ネットラベルでつなぐ)
    cb = CX + 11
    put(o - 4, cb - 2, "+12V"); g[o - 3][cb] = "|"
    put(o - 2, cb - 1, "---"); put(o - 1, cb - 1, "---")
    g[o][cb] = "|"; put(o + 1, cb - 1, "GND"); put(o - 2, cb + 3, "C_bus 100u+0.1u")
    # C_boot の縦線が HO、VS の横線と交差する箇所は、縦線を優先して描く
    for r in (o + 3, o + 4):
        g[r][C_VB] = "|"


if __name__ == "__main__":
    {"stage1": stage1, "stage2": stage2, "power": power}[sys.argv[1]]()
    dump()
