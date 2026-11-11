#include "t8l_clock.h"

void t8l_clock_reset(T8lClock *c, uint32_t ticks) {
    *c = (T8lClock){.last_ticks=ticks};
}

uint32_t t8l_clock_advance(T8lClock *c, uint32_t ticks) {
    uint32_t delta = ticks - c->last_ticks;
    c->last_ticks = ticks;
    /* Split BEFORE adding remainder: delta may itself be UINT32_MAX. */
    uint32_t fraction = (delta & 3u) + c->remainder;
    c->milliseconds += (delta >> 2) + (fraction >> 2);
    c->remainder = (uint8_t)(fraction & 3u);
    return c->milliseconds;
}
