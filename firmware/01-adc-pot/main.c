/*
 * 01-adc-pot: 半固定ボリュームの電圧を ADC で読み、PA3 の LED の明るさを変える。
 *
 * - PA6 (4番ピン、AIN6): 半固定ボリューム (10kΩ) のワイパー。両端は +5V と GND
 * - PA3 (TCA0 WO3): LED。ボリュームの位置で明るさが変わる (PWM、約13kHz)
 * - PA2: LED。0.5秒ごとに点滅 (動作中の目印)
 * - UART (PB2=TXD、PB3=RXD、115200bps): 0.2秒ごとに ADC の値、電圧、PWM の割合を表示
 *
 * ADC は 10bit (0〜1023)。基準電圧は電源 (VDD = 5V) なので、0〜5V が 0〜1023 になる。
 * ハードウェアで8回分を足し合わせ (積算)、8で割って平均することで、値の揺れを抑える。
 * 時間の管理は TCB0 の 1ms 割り込み (01-pwm-led-intr と同じ形)。
 *
 * 配線: 00-blink と同じ LED (PA2、PA3) に、ボリュームを追加。UART は docs/00-uart.md。
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdio.h>
#include "uart.h"

#define VDD_MV  5000   /* 電源電圧 (mV)。電圧の表示にだけ使う */

/* ---------------------------------------------------------------- 時刻 (1ms 割り込み) */

static volatile uint32_t ms_ticks;

ISR(TCB0_INT_vect)
{
  static uint16_t heartbeat;

  TCB0.INTFLAGS = TCB_CAPT_bm;    /* 割り込みフラグを消す (1を書く) */
  ms_ticks++;
  if (++heartbeat >= 500) {       /* 0.5秒ごとに PA2 を反転 */
    heartbeat = 0;
    PORTA.OUTTGL = PIN2_bm;
  }
}

static uint32_t millis(void)
{
  uint32_t t;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {  /* 32bit を読む間だけ、割り込みを止める */
    t = ms_ticks;
  }
  return t;
}

/* ---------------------------------------------------------------- 初期化 */

static void tick_init(void)
{
  /* TCB0 を周期割り込みモードにする。F_CPU / 1000 カウントで 1ms */
  TCB0.CCMP    = F_CPU / 1000 - 1;
  TCB0.CTRLB   = TCB_CNTMODE_INT_gc;
  TCB0.INTCTRL = TCB_CAPT_bm;
  TCB0.CTRLA   = TCB_CLKSEL_CLKDIV1_gc | TCB_ENABLE_bm;
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

static void adc_init(void)
{
  /* PA6 はアナログ入力として使うので、デジタル入力の回路を切る (雑音と消費電流を減らす) */
  PORTA.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;

  /* 基準電圧は VDD。ADC のクロックは 3.33MHz / 4 = 約833kHz (推奨範囲の中)。
   * 基準電圧が 1V 以上のときは、サンプリング容量を小さくする設定 (SAMPCAP) が推奨されている */
  ADC0.CTRLC  = ADC_SAMPCAP_bm | ADC_REFSEL_VDDREF_gc | ADC_PRESC_DIV4_gc;
  ADC0.CTRLB  = ADC_SAMPNUM_ACC8_gc;          /* 8回分を足し合わせる */
  ADC0.MUXPOS = ADC_MUXPOS_AIN6_gc;           /* PA6 */
  ADC0.CTRLA  = ADC_RESSEL_10BIT_gc | ADC_ENABLE_bm;
}

/* 1回分 (8回の積算) を変換して、平均 (0〜1023) を返す。数十µs かかる */
static uint16_t adc_read(void)
{
  ADC0.COMMAND = ADC_STCONV_bm;
  while (!(ADC0.INTFLAGS & ADC_RESRDY_bm))
    ;
  return ADC0.RES / 8;    /* RES を読むとフラグも消える */
}

/* ---------------------------------------------------------------- メイン */

int main(void)
{
  PORTA.DIRSET = PIN2_bm;

  pwm_init();
  adc_init();
  uart_init(115200);
  tick_init();
  sei();

  printf("\n01-adc-pot: start (turn the trimmer on PA6)\n");

  uint32_t t_adc = 0, t_print = 0;
  uint16_t value = 0;

  for (;;) {
    uint32_t now = millis();

    if ((int32_t)(now - t_adc) >= 0) {        /* 20ms ごとに読む */
      t_adc = now + 20;
      value = adc_read();
      /* 10bit (0〜1023) を 8bit (0〜255) にして、LED の明るさにする */
      TCA0.SPLIT.HCMP0 = value >> 2;
    }

    if ((int32_t)(now - t_print) >= 0) {      /* 0.2秒ごとに表示 */
      t_print = now + 200;
      unsigned mv  = (uint32_t)value * VDD_MV / 1023;
      unsigned pct = (uint32_t)value * 100 / 1023;
      printf("adc=%4u  %u.%02u V  pwm=%3u%%\n", value, mv / 1000, (mv % 1000) / 10, pct);
    }
  }
}
