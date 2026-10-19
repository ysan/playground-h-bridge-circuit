/*
 * 00-uart: USART0 (PB2=TXD, PB3=RXD) の動作確認。
 *
 * - 1秒ごとに、カウンタを printf で送る
 * - 文字を受信したら、その文字を送り返す
 *
 * FT232RQ とは、ピンヘッダのジャンパーピンを UART側にしてつなぐ (docs/00-setup.md)。
 * PC 側 (ホスト): picocom -b 115200 /dev/ttyUSB0  (終了は Ctrl-A Ctrl-X)
 */
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "uart.h"

int main(void)
{
  uart_init(115200);
  printf("\n00-uart: start (F_CPU=%lu)\n", (unsigned long)F_CPU);

  uint16_t count = 0;
  for (;;) {
    for (uint8_t i = 0; i < 100; i++) {   /* 10ms × 100 = 約1秒 */
      int c = uart_getc_nowait();
      if (c >= 0)
        printf("got '%c' (0x%02x)\n", c, c);
      _delay_ms(10);
    }
    printf("count=%u\n", count++);
  }
}
