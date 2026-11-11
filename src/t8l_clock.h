#ifndef T8L_CLOCK_H
#define T8L_CLOCK_H
#include <stdint.h>

/* v1.7 SysTick is nominally 4000 Hz, NOT milliseconds.
 * Single owner; advance at least once per 2^32 raw ticks (~12.4 days).
 * Reset on boot, never on a mode change. All Finder callers must share
 * this time domain; IRQ code consumes a published snapshot, not this object.
 */
typedef struct {
    uint32_t last_ticks, milliseconds;
    uint8_t remainder;
} T8lClock;
void t8l_clock_reset(T8lClock *clock, uint32_t ticks);
uint32_t t8l_clock_advance(T8lClock *clock, uint32_t ticks);
#endif
