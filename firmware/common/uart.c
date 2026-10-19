#include <avr/io.h>
#include <stdio.h>
#include "uart.h"

static int stdout_putchar(char c, FILE *stream)
{
  (void)stream;
  if (c == '\n')
    uart_putc('\r');  /* ターミナルで改行が崩れないよう、CR を先に送る */
  uart_putc(c);
  return 0;
}

static FILE uart_stdout = FDEV_SETUP_STREAM(stdout_putchar, NULL, _FDEV_SETUP_WRITE);

void uart_init(uint32_t baud)
{
  /*
   * BAUD レジスタの値 = 64 * F_CPU / (16 * baud)  (通常速度モード)
   * 内蔵発振器の誤差を、工場で測った補正値 (SIGROW.OSC20ERR5V、5V 動作時) で補正する。
   * (Microchip の TB3216 "Getting Started with USART" の方法)
   */
  int8_t err = SIGROW.OSC20ERR5V;
  int32_t reg = (int32_t)((64UL * F_CPU) / (16UL * baud));
  reg = reg * (1024 + err) / 1024;
  USART0.BAUD = (uint16_t)reg;

  PORTB.OUTSET = PIN2_bm;   /* TXD は待機中 High */
  PORTB.DIRSET = PIN2_bm;   /* PB2 = TXD (出力) */
  PORTB.DIRCLR = PIN3_bm;   /* PB3 = RXD (入力) */
  PORTB.PIN3CTRL = PORT_PULLUPEN_bm;  /* 何もつながっていないときに浮かないように */

  USART0.CTRLB = USART_TXEN_bm | USART_RXEN_bm;

  stdout = &uart_stdout;
}

void uart_putc(char c)
{
  while (!(USART0.STATUS & USART_DREIF_bm))
    ;
  USART0.TXDATAL = c;
}

int uart_getc_nowait(void)
{
  if (!(USART0.STATUS & USART_RXCIF_bm))
    return -1;
  return USART0.RXDATAL;
}
