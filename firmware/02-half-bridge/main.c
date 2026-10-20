/*
 * 02-half-bridge: IRS2003S + 2SK4017 のハーフブリッジを動作確認する。
 *
 * 配線 (docs/02-half-bridge.md 参照):
 *   PA4 (TCA0 WO4) -> IRS2003 HIN   (正論理: 1 で HO がON)
 *   PA5 (TCA0 WO5) -> IRS2003 /LIN  (負論理: 0 で LO がON)
 *   PB2 (TXD), PB3 (RXD) -> UART (115200bps)。picocom などからコマンドを送る
 *   PA2 -> LED: 0.5秒ごとに点滅 (ファームウェアが動いている目印)
 *   PA3 (TCA0 WO3) -> LED: ハーフブリッジの状態を明るさで示す
 *        全OFF: 消灯 / 低側のみON: 薄く点灯 / 高側ON: 最大 / PWM: デューティと同じ明るさ
 *
 * 待機状態は HIN=0, /LIN=1 で、上下とも OFF。起動直後は全OFFで、コマンドを待つ。
 *
 * コマンド (1文字):
 *   o, 0 : 全OFF
 *   l    : 低側のみON
 *   h    : 高側パルスを1回 (低側5msで C_boot を充電 -> 高側10ms)
 *   s    : 静的テストを1周 (全OFF -> 低側のみON -> 全OFF -> 高側パルス20回 -> 全OFF)
 *   1-9  : PWM。デューティを 10%〜90% で固定 (約19.5kHz)
 *   p    : PWM スイープを1往復 (10% -> 90% -> 10%、各1秒)
 *   ?    : コマンドの一覧と状態を表示
 * テスト (h, s, p) の実行中に何かキーを押すと、全OFFにして止まる
 * (そのキーはコマンドとして実行しない)。
 *
 * 時間の管理:
 *   TCB0 の 1ms ごとの割り込みで、時刻 (ms) を数え、PA2 の LED を点滅させる。
 *   テストは段階 (ステップ) に区切った状態機械で、main のループが時刻を見て
 *   次の段階に進める。main のループは待ち時間なしで、UART の受信も見ていられる。
 *
 * HINと/LINに同じPWM波形を出すと、/LINが反転入力なのでHOとLOは相補になる。
 * デッドタイムはIC内蔵 (標準520ns)。
 * WO3〜WO5 (PA3〜PA5) は TCA0 の分割モード (8bit×2) でのみ出力される。
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>
#include "uart.h"

#define HIN_bm  PIN4_bm  /* PA4 (WO4, HCMP1) */
#define LIN_bm  PIN5_bm  /* PA5 (WO5, HCMP2) */
#define LED_HEARTBEAT_bm  PIN2_bm  /* PA2 */
#define LED_STATE_bm      PIN3_bm  /* PA3 (WO3, HCMP0) */

#define PWM_PERIOD    256u   /* 20MHz / 4 / 256 = 約19.5kHz */
#define DUTY_MAX_PCT  90u    /* ブートストラップコンデンサを充電する時間を残す */

#define LED_LEVEL_LOW_ON  16   /* 低側のみON のときの PA3 の明るさ (0..255) */
#define LED_LEVEL_HIGH_ON 255  /* 高側ON のとき */

/* ---------------------------------------------------------------- 時刻 (1ms 割り込み) */

static volatile uint32_t ms_ticks;

ISR(TCB0_INT_vect)
{
  static uint16_t heartbeat;

  TCB0.INTFLAGS = TCB_CAPT_bm;    /* 割り込みフラグを消す (1を書く) */
  ms_ticks++;
  if (++heartbeat >= 500) {       /* 0.5秒ごとに PA2 を反転 */
    heartbeat = 0;
    PORTA.OUTTGL = LED_HEARTBEAT_bm;
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

static void clock_init(void)
{
  /* 主クロックを20MHzにする (既定は20MHz/6)。保護されたレジスタなのでCCPで解除する。 */
  CPU_CCP = CCP_IOREG_gc;
  CLKCTRL.MCLKCTRLB = 0;  /* PEN=0: プリスケーラなし */
}

static void timer_init(void)
{
  /* TCA0: 分割モードで PWM。WO3 (PA3, LED) は常に出力、WO4/WO5 (HIN, /LIN) は PWM のときだけ */
  TCA0.SPLIT.CTRLD = TCA_SPLIT_SPLITM_bm;   /* 分割モード (最初に設定する) */
  TCA0.SPLIT.LPER  = PWM_PERIOD - 1;
  TCA0.SPLIT.HPER  = PWM_PERIOD - 1;        /* WO3〜5側のカウンタの周期 */
  TCA0.SPLIT.HCMP0 = 0;                     /* PA3 の LED は消灯から */
  TCA0.SPLIT.CTRLB = TCA_SPLIT_HCMP0EN_bm;
  TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV4_gc | TCA_SPLIT_ENABLE_bm;

  /* TCB0: 周期割り込みモード。F_CPU / 1000 カウントで 1ms */
  TCB0.CCMP    = F_CPU / 1000 - 1;
  TCB0.CTRLB   = TCB_CNTMODE_INT_gc;
  TCB0.INTCTRL = TCB_CAPT_bm;
  TCB0.CTRLA   = TCB_CLKSEL_CLKDIV1_gc | TCB_ENABLE_bm;
}

/* ---------------------------------------------------------------- 出力の状態 */

static uint8_t  duty_now;      /* PWM 中のコンペア値 */
static bool     pwm_running;
static unsigned duty_pct_now;  /* 表示用 */

static void led_state(uint8_t level)
{
  TCA0.SPLIT.HCMP0 = level;
}

/* 全OFF: HIN/LIN のピンを GPIO に戻し、HIN=0, /LIN=1 にする (PA3 の LED の PWM は残す) */
static void bridge_off(void)
{
  TCA0.SPLIT.CTRLB = TCA_SPLIT_HCMP0EN_bm;
  pwm_running = false;
  PORTA.OUTCLR = HIN_bm;
  PORTA.OUTSET = LIN_bm;
  led_state(0);
}

/* 状態を切り替える前に、必ず全OFFを挟む */
static void go_off_with_deadtime(void)
{
  bridge_off();
  _delay_us(10);  /* デッドタイム (ICの内蔵分に加えて) */
}

static void bridge_low_on(void)  /* LOのみON: /LIN=0, HIN=0 */
{
  go_off_with_deadtime();
  PORTA.OUTCLR = HIN_bm | LIN_bm;
  led_state(LED_LEVEL_LOW_ON);
}

static void bridge_high_on(void) /* HOのみON: HIN=1, /LIN=1 */
{
  go_off_with_deadtime();
  PORTA.OUTSET = HIN_bm | LIN_bm;
  led_state(LED_LEVEL_HIGH_ON);
}

/* ---------------------------------------------------------------- PWM */

static uint8_t pct_to_cmp(unsigned pct)
{
  return (uint16_t)PWM_PERIOD * pct / 100;
}

/*
 * 分割モードのHCMPnはバッファがなく、書くとすぐ反映される。
 * HINのHighの時間が/LINのHighの時間より長いと、HOとLOの両方がONになる区間ができる
 * (ICの貫通防止ロジックが働くが、それに頼らない)。
 * そのため、デューティを上げるときは/LINを先に、下げるときはHINを先に更新する。
 */
static void pwm_set(uint8_t cmp)
{
  if (cmp >= duty_now) {
    TCA0.SPLIT.HCMP2 = cmp;   /* /LIN */
    TCA0.SPLIT.HCMP1 = cmp;   /* HIN */
  } else {
    TCA0.SPLIT.HCMP1 = cmp;
    TCA0.SPLIT.HCMP2 = cmp;
  }
  duty_now = cmp;
}

/* PWM を始める。最初に低側を5msONにして、C_boot を充電しておく (5ms だけ待つ) */
static void pwm_start(uint8_t cmp)
{
  bridge_low_on();
  _delay_ms(5);
  go_off_with_deadtime();

  duty_now = cmp;
  TCA0.SPLIT.HCMP1 = cmp;                            /* HIN */
  TCA0.SPLIT.HCMP2 = cmp;                            /* /LIN */
  TCA0.SPLIT.CTRLB = TCA_SPLIT_HCMP0EN_bm | TCA_SPLIT_HCMP1EN_bm | TCA_SPLIT_HCMP2EN_bm;
  pwm_running = true;
}

static void pwm_duty(unsigned duty_pct)
{
  if (duty_pct > DUTY_MAX_PCT)
    duty_pct = DUTY_MAX_PCT;
  uint8_t cmp = pct_to_cmp(duty_pct);
  if (pwm_running)
    pwm_set(cmp);
  else
    pwm_start(cmp);
  led_state(cmp);             /* PA3 の LED をデューティと同じ明るさに */
  duty_pct_now = duty_pct;
  printf("state: PWM %u%%\n", duty_pct);
}

/* ---------------------------------------------------------------- テスト (状態機械) */

typedef enum { SEQ_NONE, SEQ_STATIC, SEQ_SWEEP, SEQ_HPULSE } seq_t;

static seq_t    seq = SEQ_NONE;
static uint8_t  step;          /* 今の段階 */
static uint8_t  count;         /* 繰り返しの回数 */
static uint32_t t_next;        /* 次の段階に進む時刻 (ms) */

static void wait_until(uint16_t ms)
{
  t_next = millis() + ms;
}

/*
 * 静的テストの段階:
 *   0: 全OFF 1秒 / 1: 低側のみON 2秒 / 2: 全OFF 1秒
 *   3: 低側ON 5ms (C_boot を充電) / 4: 高側ON 10ms / 5: 全OFF 85ms  ... 3〜5 を20回
 *   6: 全OFF 1秒 / 7: 終了
 */
static void static_enter(uint8_t s)
{
  step = s;
  switch (s) {
  case 0: go_off_with_deadtime(); printf("  all OFF (1 s)\n");      wait_until(1000); break;
  case 1: bridge_low_on();        printf("  low side ON (2 s)\n");  wait_until(2000); break;
  case 2: go_off_with_deadtime(); printf("  all OFF (1 s)\n");      wait_until(1000); break;
  case 3:
    if (count == 0)
      printf("  high side pulse x20 (low 5 ms -> high 10 ms, every 100 ms)\n");
    bridge_low_on();  wait_until(5);  break;
  case 4: bridge_high_on(); wait_until(10); break;
  case 5: go_off_with_deadtime(); wait_until(85); break;
  case 6: go_off_with_deadtime(); printf("  all OFF (1 s)\n"); wait_until(1000); break;
  default:
    seq = SEQ_NONE;
    printf("static test: done\n");
    break;
  }
}

static void static_next(void)
{
  if (step == 5) {
    if (++count < 20) {
      static_enter(3);
      return;
    }
    static_enter(6);
    return;
  }
  static_enter(step + 1);
}

/* PWM スイープの段階: 0〜8 で 10%〜90%、9〜16 で 80%〜10%、17 で終了 */
static void sweep_enter(uint8_t s)
{
  step = s;
  if (s <= 16) {
    unsigned d = (s <= 8) ? (s + 1) * 10 : (17 - s) * 10;
    pwm_duty(d);
    wait_until(1000);
  } else {
    bridge_off();
    seq = SEQ_NONE;
    printf("PWM sweep: done (all OFF)\n");
  }
}

/* 高側パルスの段階: 0: 低側ON 5ms / 1: 高側ON 10ms / 2: 終了 */
static void hpulse_enter(uint8_t s)
{
  step = s;
  switch (s) {
  case 0: bridge_low_on();  wait_until(5);  break;
  case 1: bridge_high_on(); wait_until(10); break;
  default:
    bridge_off();
    seq = SEQ_NONE;
    printf("high side pulse x1 -> state: all OFF\n");
    break;
  }
}

static void seq_start(seq_t s)
{
  seq = s;
  count = 0;
  switch (s) {
  case SEQ_STATIC: printf("static test: start\n"); static_enter(0); break;
  case SEQ_SWEEP:  printf("PWM sweep: start\n"); sweep_enter(0);  break;
  case SEQ_HPULSE: hpulse_enter(0); break;
  default: break;
  }
}

/* 次の段階に進む時刻になったら進める */
static void seq_poll(void)
{
  if (seq == SEQ_NONE || (int32_t)(millis() - t_next) < 0)
    return;
  switch (seq) {
  case SEQ_STATIC: static_next();           break;
  case SEQ_SWEEP:  sweep_enter(step + 1);   break;
  case SEQ_HPULSE: hpulse_enter(step + 1);  break;
  default: break;
  }
}

static void seq_abort(void)
{
  seq = SEQ_NONE;
  bridge_off();
  printf("test aborted -> state: all OFF\n");
}

/* ---------------------------------------------------------------- コマンド */

static void print_help(void)
{
  printf("\n--- 02-half-bridge commands ---\n");
  printf(" o, 0 : all OFF\n");
  printf(" l    : low side ON\n");
  printf(" h    : one high side pulse (low 5 ms -> high 10 ms)\n");
  printf(" s    : run the static test once\n");
  printf(" 1-9  : PWM, fixed duty 10-90%%\n");
  printf(" p    : PWM sweep (10%% -> 90%% -> 10%%)\n");
  printf(" ?    : show this help\n");
  printf("Press any key during a test to stop it (all OFF)\n");
  printf("LED: PA2 blinks while running, PA3 shows the state (brightness = duty in PWM)\n");
  if (pwm_running)
    printf("state: PWM %u%%\n", duty_pct_now);
  else
    printf("state: not PWM (all OFF, low side ON, ...)\n");
}

static void handle(int c)
{
  switch (c) {
  case 'o': case '0':
    bridge_off();
    printf("state: all OFF\n");
    break;
  case 'l':
    bridge_low_on();
    printf("state: low side ON\n");
    break;
  case 'h': seq_start(SEQ_HPULSE); break;
  case 's': seq_start(SEQ_STATIC); break;
  case 'p': seq_start(SEQ_SWEEP);  break;
  case '?': print_help(); break;
  case '\r': case '\n': break;
  default:
    if (c >= '1' && c <= '9')
      pwm_duty((c - '0') * 10);
    else
      printf("unknown command '%c' (? for help)\n", c);
    break;
  }
}

int main(void)
{
  clock_init();

  PORTA.OUTCLR = HIN_bm;
  PORTA.OUTSET = LIN_bm;          /* 全OFF (HIN=0, /LIN=1) を先に決めてから出力にする */
  PORTA.DIRSET = HIN_bm | LIN_bm | LED_HEARTBEAT_bm | LED_STATE_bm;

  timer_init();
  bridge_off();
  uart_init(115200);
  sei();                          /* 割り込みを有効にする */

  printf("\n02-half-bridge: start (all OFF)\n");
  print_help();

  for (;;) {
    int c = uart_getc_nowait();
    if (c >= 0) {
      if (seq != SEQ_NONE)
        seq_abort();              /* テスト中のキーは中断だけ (コマンドとしては実行しない) */
      else
        handle(c);
    }
    seq_poll();
  }
}
