/*
 * 01-pwm-led-intr: 01-pwm-led と同じ動きを、タイマ割り込みで実現する。
 * あわせて、UART で受け取った文字を送り返す (エコー。英小文字は大文字にする)。
 *
 * - PA3: PWM (TCA0 WO3、分割モード) でフェード。約1秒で明るく、約1秒で暗く
 * - PA2: フェード1往復 (約2秒) ごとに、点灯と消灯を切り替える
 * - UART (PB2=TXD、PB3=RXD、115200bps): 受け取った文字を送り返す (エコー)。
 *   英小文字は大文字にして返すので、打った文字と見分けられる (a を打つと A が出る)
 *
 * 01-pwm-led は、for ループと _delay_ms で時間を待っていた。この版では、
 * TCB0 の 1ms ごとの割り込みの中でフェードを進めるので、main のループは
 * 待ち時間なしで UART の受信だけを見ていられる。
 *
 * 配線: 00-blink と同じ (PA2、PA3 に LED)。UART は docs/00-uart.md。
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "uart.h"

#define FADE_STEP_MS  4   /* 明るさを1段変える間隔。256段 × 4ms = 約1秒 */

/* 明るさは目に対して非線形なので、2乗して見た目をなめらかにする (0..255 -> 0..255) */
static uint8_t gamma_2(uint8_t x)
{
  return (uint16_t)x * x / 255;
}

/* 割り込みの中だけで使う状態 */
static uint8_t level;          /* 今の明るさ (0..255) */
static int8_t  dir = 1;        /* +1: 明るくしている、-1: 暗くしている */
static uint8_t ms_count;

/* 割り込みと main の両方で使う変数には volatile を付ける */
static volatile uint16_t cycles;  /* フェードを何往復したか */

/* TCB0: 1ms ごとの割り込み */
ISR(TCB0_INT_vect)
{
  TCB0.INTFLAGS = TCB_CAPT_bm;    /* 割り込みフラグを消す (1を書く) */

  if (++ms_count < FADE_STEP_MS)
    return;
  ms_count = 0;

  if (dir > 0) {
    if (level == 255) {
      dir = -1;                   /* いちばん明るい -> 暗くしていく */
    } else {
      level++;
    }
  } else {
    if (level == 0) {
      dir = 1;                    /* いちばん暗い -> 1往復終わり */
      PORTA.OUTTGL = PIN2_bm;     /* 1往復ごとに PA2 を反転 */
      cycles++;
    } else {
      level--;
    }
  }
  TCA0.SPLIT.HCMP0 = gamma_2(level);
}

static void pwm_init(void)
{
  PORTA.DIRSET = PIN3_bm;
  TCA0.SPLIT.CTRLD  = TCA_SPLIT_SPLITM_bm;   /* 分割モード (最初に設定する) */
  TCA0.SPLIT.HPER   = 255;                   /* 3.33MHz / 256 = 約13kHz */
  TCA0.SPLIT.HCMP0  = 0;
  TCA0.SPLIT.CTRLB  = TCA_SPLIT_HCMP0EN_bm;  /* WO3 (PA3) に出力 */
  TCA0.SPLIT.CTRLA  = TCA_SPLIT_CLKSEL_DIV1_gc | TCA_SPLIT_ENABLE_bm;
}

static void tick_init(void)
{
  /* TCB0 を周期割り込みモードにする。F_CPU / 1000 カウントで 1ms */
  TCB0.CCMP    = F_CPU / 1000 - 1;
  TCB0.CTRLB   = TCB_CNTMODE_INT_gc;
  TCB0.INTCTRL = TCB_CAPT_bm;
  TCB0.CTRLA   = TCB_CLKSEL_CLKDIV1_gc | TCB_ENABLE_bm;
}

int main(void)
{
  PORTA.OUTSET = PIN2_bm;        /* 最初の1往復は PA2 を点灯 */
  PORTA.DIRSET = PIN2_bm;

  pwm_init();
  uart_init(115200);
  tick_init();
  sei();                         /* 割り込みを有効にする */

  printf("\n01-pwm-led-intr: start\n");
  printf("Fade runs in a timer interrupt. Typed keys are echoed back (a-z -> A-Z)\n");

  for (;;) {
    int c = uart_getc_nowait();
    if (c < 0)
      continue;
    if (c == '\r') {             /* Enter キー: 改行して、往復回数を表示 */
      uart_putc('\r');
      uart_putc('\n');
      cli();                     /* 16bit の変数を読む間だけ、割り込みを止める */
      uint16_t n = cycles;
      sei();
      printf("(fade cycles: %u)\n", n);
    } else {
      if (c >= 'a' && c <= 'z')  /* 英小文字は大文字にして返す (エコーだとわかるように) */
        c -= 'a' - 'A';
      uart_putc(c);              /* エコー */
    }
  }
}
