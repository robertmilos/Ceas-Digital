#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "mh-fmd.h"


static bool     buzzer_active    = false;
static bool     alarm_pattern_on = false;
static uint32_t buzzer_end_us    = 0u;
static uint32_t alarm_start_us   = 0u;

static inline void buz_on(void)  { SIO_GPIO_OUT_CLR = BUZ_MASK; } 
static inline void buz_off(void) { SIO_GPIO_OUT_SET = BUZ_MASK; }  

void buzzer_init(void)
{
    RESETS_CLR = RESET_MASK;
    while ((RESETS_DONE & RESET_MASK) != RESET_MASK) { }

    PAD_GPIO10 = 0u;

    buz_off();

    IO_GPIO10_CTRL = FUNC_SIO;
    SIO_GPIO_OE_SET = BUZ_MASK;

#if MHFMD_SELFTEST
    for (int i = 0; i < 2; i++) {
        buz_on();
        busy_wait_ms(100);
        buz_off();
        busy_wait_ms(100);
    }
#endif
}

void beep(uint32_t now_us)
{
    if (alarm_pattern_on) return;
    buz_on();
    buzzer_active = true;
    buzzer_end_us = now_us + 80000u;
}

void tone(bool enable, uint32_t now_us)
{
    if (enable) {
        if (!alarm_pattern_on) {
            alarm_pattern_on = true;
            alarm_start_us   = now_us;
        }
    } else {
        alarm_pattern_on = false;
        buzzer_active    = false;
        buz_off();
    }
}

void buzzer_update(uint32_t now_us)
{
    if (alarm_pattern_on) {
        uint32_t phase_us = (now_us - alarm_start_us) % 1000000u;

        bool should_sound = (phase_us <  60000u) ||
                            (phase_us >= 120000u && phase_us < 180000u) ||
                            (phase_us >= 240000u && phase_us < 300000u) ||
                            (phase_us >= 360000u && phase_us < 420000u);

        if (should_sound && !buzzer_active) {
            buz_on();
            buzzer_active = true;
        } else if (!should_sound && buzzer_active) {
            buz_off();
            buzzer_active = false;
        }
        return;
    }

    if (buzzer_active && (int32_t)(now_us - buzzer_end_us) >= 0) {
        buz_off();
        buzzer_active = false;
    }
}