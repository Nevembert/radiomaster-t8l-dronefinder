#include "t8l_clock.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    T8lClock c;
    t8l_clock_reset(&c, 123);
    for (uint32_t i=0; i<=8000; ++i)
        assert(t8l_clock_advance(&c, 123+i) == i/4);
    /* Ordinary division of the absolute raw counter fails at this boundary. */
    t8l_clock_reset(&c, UINT32_MAX-6);
    for (uint32_t i=0; i<=12000; ++i)
        assert(t8l_clock_advance(&c, UINT32_MAX-6+i) == i/4);
    t8l_clock_reset(&c, 0);
    assert(t8l_clock_advance(&c, 3)==0);
    assert(t8l_clock_advance(&c, 2)==1073741824u); /* max delta + fraction */
    assert(c.remainder==2);
    c.milliseconds=UINT32_MAX;
    assert(t8l_clock_advance(&c, 4)==0); /* independent millisecond wrap */
    /* Irregular sampling, multiple raw wraps, compared to a 64-bit oracle. */
    t8l_clock_reset(&c, 0);
    uint64_t total=0;
    uint32_t random=12345;
    for (unsigned i=0; i<10000; ++i) {
        random=random*1664525u+1013904223u;
        total+=random;
        assert(t8l_clock_advance(&c, (uint32_t)total)==(uint32_t)(total/4));
        assert(c.remainder==total%4);
    }
    puts("clock: elapsed time, raw/ms wrap, fractional and large deltas OK");
}
