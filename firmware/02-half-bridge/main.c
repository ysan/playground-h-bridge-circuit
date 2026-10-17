/*
 * 02-half-bridge: IRS2003S + 2SK4017 のハーフブリッジを動作確認する。
 *
 * 配線 (docs/02-half-bridge.md 参照):
 *   PA4 (TCA0 WO4) -> IRS2003 HIN   (正論理: 1 で HO がON)
 *   PA5 (TCA0 WO5) -> IRS2003 /LIN  (負論理: 0 で LO がON)
 *
 * 待機状態は HIN=0, /LIN=1 で、上下とも OFF。
 *
 * 動作:
 *   1. 静的テスト: OFF -> LOのみON -> OFF -> (LO 5ms ON -> HO 20ms ON) を20回 -> OFF
 *                  HOは長時間ONにできない (C_bootが空になる) ので、短いパルスにする。
 *   2. PWMテスト : 約19.5kHz、デューティを 10% から 90% まで往復。
 *                  HINと/LINに同じ波形を出すと、/LINが反転入力なので
 *                  HOとLOは相補になる。デッドタイムはIC内蔵 (標準520ns)。
 *   以降繰り返し。
 *
 * WO4, WO5 は TCA0 の分割モード (8bit×2) でのみ出力される。
 */
#include <avr/io.h>
#include <util/delay.h>

#define HIN_bm  PIN4_bm  /* PA4 (WO4, HCMP1) */
#define LIN_bm  PIN5_bm  /* PA5 (WO5, HCMP2) */

#define PWM_PERIOD    256u   /* 20MHz / 4 / 256 = 約19.5kHz */
#define DUTY_MIN_PCT  10u
#define DUTY_MAX_PCT  90u    /* ブートストラップコンデンサを充電する時間を残す */

static uint8_t duty_now;

static void clock_init(void)
{
  /* 主クロックを20MHzにする (既定は20MHz/6)。保護されたレジスタなのでCCPで解除する。 */
  CPU_CCP = CCP_IOREG_gc;
  CLKCTRL.MCLKCTRLB = 0;  /* PEN=0: プリスケーラなし */
}

static void timer_init(void)
{
  TCA0.SPLIT.CTRLD = TCA_SPLIT_SPLITM_bm;   /* 分割モード (最初に設定する) */
  TCA0.SPLIT.LPER  = PWM_PERIOD - 1;
  TCA0.SPLIT.HPER  = PWM_PERIOD - 1;        /* WO3〜5側のカウンタの周期 */
  TCA0.SPLIT.CTRLB = 0;                     /* 出力はまだピンにつながない */
  TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV4_gc | TCA_SPLIT_ENABLE_bm;
}

/* 全OFF: ピンをGPIOに戻し、HIN=0, /LIN=1 にする */
static void bridge_off(void)
{
  TCA0.SPLIT.CTRLB = 0;
  PORTA.OUTCLR = HIN_bm;
  PORTA.OUTSET = LIN_bm;
}

static void bridge_low_on(void)  /* LOのみON: /LIN=0, HIN=0 */
{
  PORTA.OUTCLR = HIN_bm | LIN_bm;
}

static void bridge_high_on(void) /* HOのみON: HIN=1, /LIN=1 */
{
  PORTA.OUTSET = HIN_bm | LIN_bm;
}

static uint8_t pct_to_cmp(unsigned pct)
{
  return (uint16_t)PWM_PERIOD * pct / 100;
}

static void pwm_start(unsigned duty_pct)
{
  duty_now = pct_to_cmp(duty_pct);
  TCA0.SPLIT.HCMP1 = duty_now;                       /* HIN */
  TCA0.SPLIT.HCMP2 = duty_now;                       /* /LIN */
  TCA0.SPLIT.CTRLB = TCA_SPLIT_HCMP1EN_bm | TCA_SPLIT_HCMP2EN_bm;
}

/*
 * 分割モードのHCMPnはバッファがなく、書くとすぐ反映される。
 * HINのHighの時間が/LINのHighの時間より長いと、HOとLOの両方がONになる区間ができる
 * (ICの貫通防止ロジックが働くが、それに頼らない)。
 * そのため、デューティを上げるときは/LINを先に、下げるときはHINを先に更新する。
 */
static void pwm_set(unsigned duty_pct)
{
  uint8_t cmp = pct_to_cmp(duty_pct);
  if (cmp >= duty_now) {
    TCA0.SPLIT.HCMP2 = cmp;   /* /LIN */
    TCA0.SPLIT.HCMP1 = cmp;   /* HIN */
  } else {
    TCA0.SPLIT.HCMP1 = cmp;
    TCA0.SPLIT.HCMP2 = cmp;
  }
  duty_now = cmp;
}

int main(void)
{
  clock_init();
  timer_init();

  bridge_off();
  PORTA.DIRSET = HIN_bm | LIN_bm;

  for (;;) {
    /* 1. 静的テスト。状態の切り替えは必ずOFFを挟む。 */
    bridge_off();      _delay_ms(1000);
    bridge_low_on();   _delay_ms(2000);
    bridge_off();      _delay_ms(1000);

    /* 高側は、C_bootに溜めた電荷で駆動する。IC自身の消費電流で数百msで空になるので、
     * 長くONにできない。低側で充電してから高側を短時間ONにする、を繰り返す。 */
    for (uint8_t i = 0; i < 20; i++) {
      bridge_low_on();   _delay_ms(5);    /* C_bootを充電 */
      bridge_off();      _delay_us(10);   /* デッドタイム (ICの内蔵分に加えて) */
      bridge_high_on();  _delay_ms(20);
      bridge_off();      _delay_ms(75);
    }
    _delay_ms(1000);

    /* 2. PWMテスト */
    pwm_start(DUTY_MIN_PCT);
    for (unsigned d = DUTY_MIN_PCT; d <= DUTY_MAX_PCT; d += 10) {
      pwm_set(d);
      _delay_ms(1000);
    }
    for (unsigned d = DUTY_MAX_PCT; d >= DUTY_MIN_PCT; d -= 10) {
      pwm_set(d);
      _delay_ms(1000);
    }
    bridge_off();
    _delay_ms(1000);
  }
}
