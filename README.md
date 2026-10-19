# playground-h-bridge-circuit

DCモーター用のHブリッジ回路を自作し、ATtiny1604 (tinyAVR 1-series) で制御しながら学ぶためのリポジトリです。
回路・ファームウェア・解説資料を段階的にまとめています。

> 状態: 作成中。以下のロードマップに沿って順次追加します。

## ロードマップ

| # | テーマ | 回路 | ファームウェア | 資料 |
|---|--------|------|----------------|------|
| 00 | 開発環境の確認 (LED点滅、UART) | - | [firmware/00-blink](firmware/00-blink)、[firmware/00-uart](firmware/00-uart) | [docs/00-setup.md](docs/00-setup.md) |
| 01 | Hブリッジの基礎と、PWMの練習 (PA3のLEDをフェード) | - | [firmware/01-pwm-led](firmware/01-pwm-led) | [docs/01-h-bridge-basics.md](docs/01-h-bridge-basics.md) |
| 02 | ハーフブリッジ (IRS2003 + 2SK4017) | 設計済み (未製作) | [firmware/02-half-bridge](firmware/02-half-bridge) (ビルドのみ確認) | [docs/02-half-bridge.md](docs/02-half-bridge.md) |
| 03 | Hブリッジ (ハーフブリッジ2組) と正転・逆転・ブレーキ。モーターを回す | 未着手 | 未着手 | 未着手 |
| 04 | PWMによる速度制御 (TCA0)。Sign-Magnitude と Lock-Antiphase の比較 | - | 未着手 | 未着手 |
| 05 | 電流検出・エンコーダによるフィードバック制御 (PID)。速度制御から位置制御へ | 未着手 | 未着手 | 未着手 |

回路構成の順序は予定であり、進めながら変更する可能性があります。

540 や 775 のような大きなモーターは、このハーフブリッジ (2SK4017、ブレッドボード) では電流が足りません。MOSFET の強化、基板化、電流保護 (05) の後の発展として扱う予定です。

## 使用部品

| 部品 | 型番 | 用途 |
|---|---|---|
| マイコン | ATtiny1604 (SOIC-14、変換基板に実装) | 制御 |
| ハーフブリッジゲートドライバ | IRS2003S (SOIC-8、変換基板を使用) | MOSFETの駆動 |
| Nch パワーMOSFET | 2SK4017(Q) (60V 5A、4V駆動) | スイッチ |
| ショットキーバリアダイオード | SBM245L (45V 2A) | ブートストラップ・還流 |
| USBシリアル変換 | FT232RQ モジュール | UPDI書き込み |
| 電源 | 安定化電源 (30V 12A) | モーター電源・ゲート電源 |
| モーター | 12V の 370 サイズのギヤードモーター (エンコーダ付き。想定、未購入) | 03〜05 で使用 |

IRS2003 は Infineon が新規設計に推奨していない品種で、2SK4017 も生産終了扱いの情報があります。
学習用として使っていますが、新規に作る場合は後継品 (IRS2007 など) の検討を勧めます。

## ディレクトリ構成

```
docker/     ビルド・書き込み用のDockerfile
firmware/   ATtiny1604のファームウェア (テーマごとにディレクトリ)
  common.mk   共通のビルド設定
  common/     共通の部品 (UART など)
docs/       解説資料
hardware/   回路図・基板データ (予定: KiCad)
tools/      資料の図を生成するスクリプト
```

## 使用環境

- マイコン: ATtiny1604 (UPDIで書き込み)
- 書き込み: SerialUPDI (USB-シリアル変換 + ダイオード + 470Ω)。avrdudeを使用
- ビルド: avr-gcc / avr-libc (2.1以降が必要)。Dockerイメージで用意しています

セットアップの詳細は [docs/00-setup.md](docs/00-setup.md) を参照してください。

## クイックスタート

```sh
docker build -t avr-builder --build-arg UID=$(id -u) --build-arg GID=$(id -g) docker/

docker run -dit --name avr-builder-dev \
  -v $(pwd)/firmware:/home/builder/firmware:Z \
  -v /dev:/host-dev \
  --device-cgroup-rule='c 188:* rmw' \
  --group-add $(getent group dialout | cut -d: -f3) \
  --restart unless-stopped \
  avr-builder

# 常駐したコンテナに入る
docker exec -it avr-builder-dev bash
# コンテナ内で
cd firmware/00-blink
make
make flash PORT=/host-dev/ttyUSB0
```

コンテナに入らずに、ホストから直接実行することもできます。

```sh
docker exec avr-builder-dev make -C firmware/00-blink flash PORT=/host-dev/ttyUSB0
```

ホストの `/dev` を `/host-dev` として見せているので、起動後にUSBアダプタを抜き差ししても使えます。
`:Z` はSELinux用の指定で、SELinuxのない環境では無視されます。
詳細は [docs/00-setup.md](docs/00-setup.md) を参照してください。

## 注意

モーターや電源を扱う回路です。通電前の配線確認、電源の電流制限、
上下アームの同時ON (シュートスルー) の防止に注意してください。
本リポジトリの内容は学習目的であり、動作や安全性は保証しません。

## ライセンス

[MIT License](LICENSE)。コード、資料、回路図のすべてに適用します。
