# 00. 開発環境のセットアップ

## 必要なもの

- ATtiny1604 (14pin) とブレッドボード等
- UPDI書き込み手段: SerialUPDI (USB-シリアル変換アダプタ + ダイオード + 470Ω。下記参照)
- Docker (または Podman)

## ATtiny1604 のピン配置 (SOIC-14)

上から見た図で、左上の `*` が1番ピンです。

```
                    ATtiny1604
                +------\_/------+
             VCC  1 |*          | 14  GND
 SS  WO4     PA4  2 |           | 13  PA3  WO3  SCK
     WO5     PA5  3 |           | 12  PA2       MISO
             PA6  4 |           | 11  PA1       MOSI
             PA7  5 |           | 10  PA0       UPDI
 RXD         PB3  6 |           |  9  PB0  WO0  SCL
 TXD WO2     PB2  7 |           |  8  PB1  WO1  SDA
                +---------------+
```

- `WO0`〜`WO5` は TCA0 の波形出力 (PWM) です。`WO3`〜`WO5` は TCA0 の分割モードでのみ出力されます。
- `SS`/`SCK`/`MISO`/`MOSI` は SPI、`SDA`/`SCL` は I2C (TWI)、`RXD`/`TXD` は USART0 の既定のピンです。
- PA2 には TCA0 の出力がありません。

参考: [Arduino フォーラムのピン配置図](https://europe1.discourse-cdn.com/arduino/optimized/4X/2/3/1/231ff543f32f67bfcaad4b65606ae69ff396c59e_2_690x243.png)。
TCA0 の出力ピンは [Microchip: TCA Pin Positions](https://onlinedocs.microchip.com/oxy/GUID-ACE76F82-8072-410B-AFA7-16B2BF7B4CBA-en-US-8/GUID-9E1155F0-F789-41FC-B240-9CAE98BBDC61.html) で確認しました。

### このリポジトリでのピンの使い方

| ピン | 用途 | 使うサンプル |
|---|---|---|
| PA0 (10番) | UPDI (書き込み) | 全て |
| PA2 (12番)、PA3 (13番) | LED (`00-blink`)。`01-pwm-led` は PA3 のみ | 00、01 |
| PA4 (2番) | IRS2003 の HIN (TCA0 WO4) | 02 |
| PA5 (3番) | IRS2003 の /LIN (TCA0 WO5) | 02 |
| PB2 (7番)、PB3 (6番) | UART (USART0 の TXD、RXD)。デバッグ用の printf | 00-uart、以降 |

## ビルド環境

tinyAVR 0/1/2 シリーズは avr-libc 2.1 以降が必要です。
Debian bookworm のパッケージでは古いため、`docker/Dockerfile` では trixie を使っています。

```sh
docker build -t avr-builder --build-arg UID=$(id -u) --build-arg GID=$(id -g) docker/
docker run -dit --name avr-builder-dev \
  -v $(pwd)/firmware:/home/builder/firmware:Z \
  -v /dev:/host-dev \
  --device-cgroup-rule='c 188:* rmw' \
  --group-add $(getent group dialout | cut -d: -f3) \
  --restart unless-stopped \
  avr-builder
```

コンテナは常駐させ、必要なときに `docker exec` で入ります。

```sh
docker exec -it avr-builder-dev bash                   # コンテナに入る
docker exec avr-builder-dev make -C firmware/00-blink  # 入らずに実行する
```

- `-dit` で常駐させます。`-d` だけだと、端末がないためbashがすぐ終了します。
- `-v` や `--device-cgroup-rule` は起動後に追加できません。変更する場合は `docker rm -f avr-builder-dev` で消して作り直します。
- `:Z` はSELinux用の指定です。SELinux環境でbind mountが拒否されるのを防ぎます。SELinuxのない環境では無視されます。
- ホストの `/dev` を `/host-dev` として見せています。`--device` と違い、コンテナ起動後にUSBアダプタを抜き差ししても使えます。`/dev` ではなく別の場所にマウントするのは、コンテナ自身の `/dev` (`/dev/pts` など) を上書きしないためです。
- `--device-cgroup-rule` はデバイスへのアクセス許可です。`188` はttyUSBのmajor番号で、ttyACMなら `166` です。
- `--group-add` にはホスト側の `dialout` グループのgidを渡します。
- Podman (rootless) ではcgroupルールが使えません。`--group-add keep-groups` で動くことが多いです。

## SerialUPDI の配線

[burariweb.info の解説](https://burariweb.info/wp-content/uploads/2023/06/arduino-updi-writing-method-37.png) の回路を参考にしています。
FT232RQ モジュール (FTDI の USB-シリアル変換) を使い、TXDとRXDを次のようにまとめてUPDIピンにつなぎます。

```
USB-TTL                                  ATtiny1604
  RXD -------------+------[ 470Ω ]------- UPDI (PA0)
                   |
  TXD ------|<------+                     (ダイオード: 線の側 = カソードがTXD)
  VCC ------------------------------------ VCC
  GND ------------------------------------ GND
```

UPDIは1本の線で送受信する半二重通信なので、TXDはダイオードを介して合流させ、RXDで自分の送信波形も読み戻します。
FT232RQ モジュールのVCC出力は5V固定で、ATtiny1604もこの5Vで動かします。
モジュールの切替スイッチは、信号線 (TXD/RXD) の電圧を選びます。ATtiny1604が5Vで動くため、信号線も5Vに合わせます (出荷時の設定から変更)。信号線が3.3Vのままだと、UPDIの入力がHighと認識される電圧 (電源の約70% = 3.5V) に届かず、書き込みが不安定になる可能性があります。
02 でクロックを20MHzにするためにも、電源5Vが前提です (tinyAVR 1-seriesは20MHz動作に4.5V以上が必要。データシートで要確認)。

## ビルドと書き込み

コンテナ内で実行します。

```sh
cd firmware/00-blink
make                              # blink.hex を生成
make flash PORT=/host-dev/ttyUSB0 # UPDIで書き込み
```

チップを変える場合は `MCU` と `PART` の両方を指定します (詳細は `firmware/common.mk` の冒頭)。

```sh
make MCU=attiny404 PART=t404
```

## サンプルの配線と動作

| サンプル | 配線 | 動作 |
|---|---|---|
| `00-blink` | PA2・PA3 に LED のアノード。カソードはまとめて 470Ω を通して GND へ | 2個のLEDを0.3秒ずつ交互に点灯 |
| `00-uart` | ジャンパーピンを UART側にする (次の節) | 1秒ごとにカウンタを送信し、受信した文字を送り返す |
| `01-pwm-led` | PA3 に LED のアノード、カソードは 470Ω を通して GND へ (00 と同じ配線でよい) | PA3 のLEDを PWM でフェード (約1秒で明るく、約1秒で暗く) |

- `00-blink` は同時に光るLEDが1個だけなので、抵抗は共通の1本で足ります。
- `01-pwm-led` の PWM は、TCA0 の WO3 (PA3) から出します。PA2 には TCA0 の出力がないため、PA2 のLEDはフェードできません。

## UART でのデバッグ (ピンヘッダで切り替え)

UPDI と UART で、FT232RQ モジュールを1つで共用します。3ピンのピンヘッダを2つ (TXD 用と RXD 用) 用意し、ジャンパーピン (ショートピン) を差す位置で、FT232RQ の TXD と RXD の行き先を切り替えます。

```
                PROG側        共通            UART側
 RXD 用ヘッダ:   [o]----------[o]------------[o]
                  |            |              |
                 (P1)    FT232RQ の RXD   ATtiny1604 の PB2 (7番, TXD)

 TXD 用ヘッダ:   [o]----------[o]------------[o]
                  |            |              |
                 (P2)    FT232RQ の TXD   ATtiny1604 の PB3 (6番, RXD)
```

PROG側のピン (P1、P2) の先は、上の SerialUPDI と同じ配線です。

```
 (P1) -------------+------[ 470Ω ]------- ATtiny1604 の UPDI (PA0, 10番)
                   |
 (P2) ------|<-----+   (ダイオード: 線の側 = カソードが P2)
```

接続をまとめると、次のとおりです。

| ヘッダ | PROG側のピン | 共通のピン | UART側のピン |
|---|---|---|---|
| RXD 用 | UPDI の接続点 (470Ω を通して PA0 へ) | FT232RQ の RXD | ATtiny1604 の PB2 (TXD) |
| TXD 用 | ダイオードを通して UPDI の接続点へ (カソードがこのピン側) | FT232RQ の TXD | ATtiny1604 の PB3 (RXD) |

- **PROG**: 2つのジャンパーピンを、どちらも PROG側と共通のピンに差します。上の SerialUPDI と同じ配線になり、`make flash` で書き込めます。
- **UART**: 2つとも、共通と UART側のピンに差し替えます。FT232RQ の RXD が ATtiny1604 の TXD (PB2) へ、FT232RQ の TXD が ATtiny1604 の RXD (PB3) へつながります。
- ジャンパーピンは隣り合う2本しかつなげないので、PROG側と UART側に同時につながることはありません。設定を間違えてもショートしません。
- 2×3 のピンヘッダにまとめて、2つのジャンパーピンが並ぶようにすると、向きがそろっているかを確かめやすくなります。
- PA0 (UPDI) は、マイコンの UART とは別の回路で、プログラムからは使えません。UART は PB2 と PB3 を使います。

使い方:

1. ジャンパーピンを PROG側にして、書き込む (`make flash`)。
2. ジャンパーピンを UART側に差し替えて、ターミナルソフトでつなぐ。

```sh
picocom -b 115200 /dev/ttyUSB0
# 終了は Ctrl-A のあとに Ctrl-X
```

- picocom は、ホストで実行します。多くの Linux ディストリビューションで、標準のパッケージマネージャーからインストールできます。
- ホストのユーザーが `/dev/ttyUSB0` を開けない場合は、`sudo picocom -b 115200 /dev/ttyUSB0` で実行するのが手っ取り早いです。普段から使うなら、`dialout` グループに追加すると、`sudo` なしで使えます: `sudo usermod -aG dialout $USER`。反映にはログインし直しが必要です (そのターミナルだけなら `newgrp dialout` ですぐに使えます)。

- 書き込みとターミナルは、同じ `/dev/ttyUSB0` を使うので、同時には使えません。書き込む前に、picocom を終了します。
- ジャンパーピンが UART側のまま `make flash` すると、書き込みに失敗します (壊れることはありません)。
- 動作確認用のサンプルは `firmware/00-uart` です。1秒ごとにカウンタを送り、受信した文字を送り返します。
- ファームウェアからは、`firmware/common/uart.c` の `uart_init(115200)` を呼べば、`printf` の出力が UART に出ます。

## 書き込みがうまくいかないとき

- まず接続だけ確認する: `avrdude -p t1604 -c serialupdi -P /host-dev/ttyUSB0 -v`。`device signature = 0x1e9425` と出れば、配線と書き込み環境は正常です。
- `/dev/ttyUSB0` が出ない場合は、**充電専用のUSBケーブル**(データ線が入っていないもの)の可能性があります。見た目では区別できません。FT232RQ モジュールをつないだ状態で確認します。

  ```sh
  lsusb | grep -i ftdi   # FTDIのデバイスが見えれば、データ線は生きている
  ls /dev/ttyUSB*        # 出れば、認識されている
  ```

  `lsusb` に出なければ、データ対応のケーブルに替えます。`lsusb` に出るのに `/dev/ttyUSB0` がなければ、ケーブルではなくドライバの問題です。長いケーブルやハブ経由では、認識が不安定になることもあります。
- ボーレートを変える (指定しない場合はavrdudeの既定値):

  ```sh
  # avrdudeを直接使う場合
  avrdude -p t1604 -c serialupdi -P /host-dev/ttyUSB0 -b 57600 -v

  # make flash の場合
  make flash PORT=/host-dev/ttyUSB0 BAUD=57600
  ```

  動作確認した範囲 (FT232RQ + ダイオード + 470Ω、`00-blink` の書き込みと照合まで):

  | ボーレート | 結果 |
  |---|---|
  | 9600, 19200, 38400, 57600, 115200, 153600, 230400, 345600 | 成功 |
  | 460800, 921600 | 失敗 (`UPDI link initialization failed`) |

  配線の長さやアダプタによって上限は変わるため、目安として扱ってください。
- `ttyUSB0` の番号が変わる場合は、`ls /dev/serial/by-id/` で固有のパスを調べ、`PORT=/host-dev/serial/by-id/...` を指定します。
- 開けない場合は、ホスト側で `ls -l /dev/ttyUSB0` を見て所有グループを確認し、`--group-add` に合わせます。
