#!/usr/bin/env python3
"""RIGOL DS1000Z 系のオシロ (DS1102Z-E など) の画面を、LAN 経由で PNG として保存する。

使い方:
    python3 tools/scope_capture.py docs/images/02/1-4-deadtime.png
    python3 tools/scope_capture.py out.png --host 192.168.50.220

オシロの IP アドレスは、--host か、環境変数 SCOPE_HOST で指定する
(どちらもなければ 192.168.50.220)。オシロは SCPI の命令を TCP の 5555 番で受け付ける。
"""
import argparse
import os
import socket
import sys

DEFAULT_HOST = "192.168.50.220"
PORT = 5555


def recv_exact(sock, n):
    """n バイトを受け取るまで読む。"""
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("接続が途中で切れました")
        buf += chunk
    return bytes(buf)


def capture(host, timeout=10.0):
    """画面の画像 (PNG) を取り込んで、バイト列で返す。"""
    with socket.create_connection((host, PORT), timeout=timeout) as s:
        # 画面のデータを PNG で要求する (色はそのまま、反転なし)
        s.sendall(b":DISP:DATA? ON,OFF,PNG\n")
        # 返事は「#」+ 桁数 (1文字) + データの長さ (桁数ぶん) + データ本体 + 改行
        if recv_exact(s, 1) != b"#":
            raise ValueError("想定しない返事です (先頭が # ではない)")
        ndigits = int(recv_exact(s, 1))
        length = int(recv_exact(s, ndigits))
        data = recv_exact(s, length)
    if not data.startswith(b"\x89PNG"):
        raise ValueError("PNG のデータではありません")
    return data


def main():
    ap = argparse.ArgumentParser(description="オシロの画面を PNG で保存する")
    ap.add_argument("output", help="保存するファイル名 (.png)")
    ap.add_argument("--host", default=os.environ.get("SCOPE_HOST", DEFAULT_HOST),
                    help="オシロの IP アドレス")
    args = ap.parse_args()

    data = capture(args.host)
    out_dir = os.path.dirname(args.output)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(args.output, "wb") as f:
        f.write(data)
    print(f"saved: {args.output} ({len(data)} bytes)")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:  # 失敗の理由を短く表示する
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)
