#include <avr/io.h>
#include <util/delay.h>

/*
 * ATtiny1604 (14pin版): PA2, PA3 のLEDを交互に点灯する。
 * 各LEDのアノードをピンに、カソードをまとめて470Ωを通してGNDにつなぐ (Highで点灯)。
 * 同時に光るのは1個だけなので、抵抗は1本でよい。
 * (同時点灯させる場合は、LEDごとに抵抗を入れる)
 * PA0はUPDIなので使わない。PA4とPA5は02-half-bridgeでHIN、/LINに使うため空けておく。
 */
#define LED_MASK (PIN2_bm | PIN3_bm)

int main(void)
{
  PORTA.DIRSET = LED_MASK;
  for (;;) {
    static const uint8_t leds[] = {PIN2_bm, PIN3_bm};
    for (uint8_t i = 0; i < sizeof(leds); i++) {
      PORTA.OUTCLR = LED_MASK;
      PORTA.OUTSET = leds[i];
      _delay_ms(300);
    }
  }
}
