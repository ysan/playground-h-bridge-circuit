# 各プロジェクトのMakefileからincludeして使う共通ビルド設定です。
#
# プロジェクト側のMakefileの例:
#   TARGET := blink
#   SRCS   := main.c
#   include ../common.mk
#   (共通の部品を使う場合: SRCS := main.c ../common/uart.c)
#
# 使い方: make              (ビルド)
#         make flash        (UPDIで書き込み)
#         make flash BAUD=57600          (ボーレートを指定して書き込み)
#         make MCU=attiny404 PART=t404   (チップを変える場合)
#
# 型番は用途ごとに別々に定義されているので、チップを変えるときは両方変える。
#   MCU  : avr-gcc用 (-mmcu)。 "attiny" + 型番 (例: attiny404, attiny1604)
#          gccのdevice-specs (/usr/lib/gcc/avr/*/device-specs/specs-<MCU>) が読まれ、
#          __AVR_ATtiny1604__ のようなマクロが自動定義される。
#          avr/io.h がそれを見て iotn1604.h (レジスタ定義) を読むので、
#          ソース側に型番を書く必要はない。 対応一覧: avr-size --mlist-devices
#   PART : avrdude用。 "t" + 型番 (例: t404, t1604)。 /etc/avrdude.conf のpart ID。
#          書き込み時のメモリ構成やUPDI設定に使われる。 対応一覧: avrdude -p ?
#   F_CPU: _delay_ms()等が使う周波数。 実際のクロック設定に合わせる。
#          (tinyAVR 0/1のデフォルトは20MHz内蔵OSC/6分周 = 3.33MHz)

ifndef TARGET
$(error TARGET is not set)
endif
ifndef SRCS
$(error SRCS is not set)
endif

MCU        ?= attiny1604
PART       ?= t1604
F_CPU      ?= 3333333UL
PROGRAMMER ?= serialupdi
PORT       ?= /dev/ttyUSB0
BAUD       ?=

# firmware/common/ (共通の部品。uart.c など) をインクルードパスに入れる
COMMON_DIR := $(dir $(lastword $(MAKEFILE_LIST)))common

CFLAGS  := -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -ffunction-sections -fdata-sections -I$(COMMON_DIR)
LDFLAGS := -mmcu=$(MCU) -Wl,--gc-sections

all: $(TARGET).hex size

$(TARGET).elf: $(SRCS)
	avr-gcc $(CFLAGS) $(LDFLAGS) -o $@ $^

$(TARGET).hex: $(TARGET).elf
	avr-objcopy -O ihex -R .eeprom $< $@

size: $(TARGET).elf
	avr-size --format=avr --mcu=$(MCU) $<

flash: $(TARGET).hex
	avrdude -p $(PART) -c $(PROGRAMMER) -P $(PORT) $(if $(BAUD),-b $(BAUD)) -U flash:w:$<:i

clean:
	rm -f *.elf *.hex

.PHONY: all size flash clean
