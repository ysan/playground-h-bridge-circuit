# 00. UART でのデバッグ (printf)

書き込んだプログラムと PC の間で、**文字をやりとりする**方法です。書き込み (UPDI) の方法は [00-setup.md](00-setup.md) にあります。ATtiny1604 の UART (USART0) を使い、`printf` の出力を PC で見たり、PC からキーでコマンドを送ったりできます。

## 配線

UART のピンは、PB2 (7番ピン、TXD) と PB3 (6番ピン、RXD) です。PA0 (UPDI) は書き込み専用の回路につながっていて、プログラムからは使えません。

FT232RQ モジュールは、書き込みと UART で1つを共用します。**FT232RQ の RXD と TXD に差すケーブルを、用途に合わせて差し替えます。** VCC と GND は、どちらのときもそのままです。

| FT232RQ のピン | 書き込み (UPDI) のとき | UART のとき |
|---|---|---|
| RXD | [SerialUPDI の配線](00-setup.md#serialupdi-の配線) (470Ω を通して PA0 へ) | ATtiny1604 の PB2 (7番、TXD) |
| TXD | SerialUPDI の配線 (ダイオードを通して PA0 側へ) | ATtiny1604 の PB3 (6番、RXD) |
| VCC、GND | そのまま | そのまま |

**TXD と RXD は交差させてつなぎます。** ATtiny1604 の TXD (送信) は FT232RQ の RXD (受信) へ、ATtiny1604 の RXD は FT232RQ の TXD へつなぎます。TXD どうしをつなぐと、1文字も届きません (ボーレートが違うときは、化けた文字が届きます)。モジュールによっては、ピンの印字が相手側から見た向きのこともあるので、何も表示されないときは2本を入れ替えて試します (5V どうしなので、入れ替えても壊れません)。

## 使い方

1. 書き込みの配線にして、書き込む (`make flash`)。
2. FT232RQ の RXD と TXD のケーブルを、UART の配線に差し替える。
3. ホストでターミナルソフトを起動する。

```sh
picocom -b 115200 /dev/ttyUSB0
# 終了は Ctrl-A のあとに Ctrl-X
```

- picocom は、ホストで実行します。多くの Linux ディストリビューションで、標準のパッケージマネージャーからインストールできます。
- ホストのユーザーが `/dev/ttyUSB0` を開けない場合は、`sudo picocom -b 115200 /dev/ttyUSB0` で実行するのが手っ取り早いです。普段から使うなら、`dialout` グループに追加すると、`sudo` なしで使えます: `sudo usermod -aG dialout $USER`。反映にはログインし直しが必要です (そのターミナルだけなら `newgrp dialout` ですぐに使えます)。
- picocom は、打ったキーを表示しません (ローカルエコーなし)。マイコンが返した文字だけが表示されます。
- 書き込みと picocom は、同じ `/dev/ttyUSB0` を使うので、同時には使えません。書き込む前に、picocom を終了します。
- UART の配線のまま `make flash` すると、書き込みに失敗します (壊れることはありません)。

## サンプル

- 動作確認用のサンプルは `firmware/00-uart` です。1秒ごとに `count=…` を送り、受信した文字を `got 'a' (0x61)` のように送り返します。
- 自分のファームウェアでは、`firmware/common/uart.c` を `SRCS` に加え、`uart_init(115200)` を呼べば、`printf` の出力が UART に出ます。`uart_getc_nowait()` で、受信した1文字を読めます。
