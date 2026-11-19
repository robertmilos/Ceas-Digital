#ifndef MH_FMD_H
#define MH_FMD_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/regs/addressmap.h"
#include "hardware/regs/resets.h"
#include "hardware/regs/io_bank0.h"
#include "hardware/regs/pads_bank0.h"
#include "hardware/regs/sio.h"

#define REG32(a)       (*(volatile uint32_t *)(a))
#define ATOMIC_CLR     0x3000u

#define BUZ_PIN        10u
#define BUZ_MASK       (1u << BUZ_PIN)
#define FUNC_SIO       5u

#define RESETS_CLR     REG32(RESETS_BASE + ATOMIC_CLR + RESETS_RESET_OFFSET)
#define RESETS_DONE    REG32(RESETS_BASE + RESETS_RESET_DONE_OFFSET)
#define RESET_MASK     (RESETS_RESET_IO_BANK0_BITS | RESETS_RESET_PADS_BANK0_BITS)

#define PAD_GPIO10     REG32(PADS_BANK0_BASE + PADS_BANK0_GPIO10_OFFSET)
#define IO_GPIO10_CTRL REG32(IO_BANK0_BASE + IO_BANK0_GPIO10_CTRL_OFFSET)

#define SIO_GPIO_OUT_SET REG32(SIO_BASE + SIO_GPIO_OUT_SET_OFFSET)
#define SIO_GPIO_OUT_CLR REG32(SIO_BASE + SIO_GPIO_OUT_CLR_OFFSET)
#define SIO_GPIO_OE_SET  REG32(SIO_BASE + SIO_GPIO_OE_SET_OFFSET)

#define MHFMD_SELFTEST 1 

void buzzer_init(void);
void beep(uint32_t now_us);
void tone(bool enable, uint32_t now_us);
void buzzer_update(uint32_t now_us);

#endif