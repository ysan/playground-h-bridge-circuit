/*
 * 01-pwm-led: PA3 に接続したLEDを、PWMで明るくしたり暗くしたりする (フェード)。
 *
 * PA3 は TCA0 の波形出力 WO3。WO3〜WO5 は TCA0 の分割モード (8bit×2) でのみ出る。
 * 配線: PA3 -> LEDのアノード、LEDのカソード -> 470Ω -> GND (Highで点灯)
 * PA2 のLEDは、普通の出力ピンとして、フェード1往復 (約2秒) ごとに点灯と消灯を切り替える。
 * (PA2 は TCA0 の波形出力を持たないので、PWMではフェードできない。)
 *
 * クロックは既定の 20MHz/6 = 3.33MHz のまま、分周なしで8bitカウンタ (256) を回すので、
 * PWM周波数は 3.33MHz / 256 = 約13kHz。目に見えるちらつきはない。
 */
#include <avr/io.h>
#include <util/delay.h>

/* 明るさは目に対して非線形なので、2乗して見た目をなめらかにする (0..255 -> 0..255) */
static uint8_t gamma_2(uint8_t x)
{
  return (uint16_t)x * x / 255;
}

int main(void)
{
  PORTA.DIRSET = PIN3_bm;

  PORTA.DIRSET = PIN2_bm;   /* PA2 のLED (フェード1往復ごとに反転) */

  TCA0.SPLIT.CTRLD  = TCA_SPLIT_SPLITM_bm;   /* 分割モード (最初に設定する) */
  TCA0.SPLIT.HPER   = 255;                   /* WO3〜5側のカウンタの周期 */
  TCA0.SPLIT.HCMP0  = 0;
  TCA0.SPLIT.CTRLB  = TCA_SPLIT_HCMP0EN_bm;  /* WO3 (PA3) に出力 */
  TCA0.SPLIT.CTRLA  = TCA_SPLIT_CLKSEL_DIV1_gc | TCA_SPLIT_ENABLE_bm;

  for (;;) {
    PORTA.OUTTGL = PIN2_bm;                  /* 1往復ごとに PA2 を反転 */
    for (int i = 0; i <= 255; i++) {         /* だんだん明るく */
      TCA0.SPLIT.HCMP0 = gamma_2(i);
      _delay_ms(4);
    }
    for (int i = 255; i >= 0; i--) {         /* だんだん暗く */
      TCA0.SPLIT.HCMP0 = gamma_2(i);
      _delay_ms(4);
    }
  }
}
