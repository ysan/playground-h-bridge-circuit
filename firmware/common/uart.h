/*
 * USART0 によるシリアル通信 (デバッグ用の printf と、1文字の受信)。
 *
 * ピン (USART0 の既定のピン):
 *   PB2 (7番ピン) = TXD  -> USB-シリアル変換の RXD へ
 *   PB3 (6番ピン) = RXD  <- USB-シリアル変換の TXD から
 * 形式は 8N1 (データ8bit、パリティなし、ストップ1bit)。
 *
 * 使い方:
 *   uart_init(115200);
 *   printf("duty=%u\n", d);          // stdout を UART につなぐ
 *   int c = uart_getc_nowait();       // 受信した文字。なければ -1
 */
#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(uint32_t baud);
void uart_putc(char c);
int  uart_getc_nowait(void);

#endif
