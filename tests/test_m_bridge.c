#include "t8l_m_bridge.h"
#include "t8l_clock.h"
#include <assert.h>
#include <stdio.h>

static T8lMBridge bridge;
static T8lClock clock_state;
static uint32_t ticks;
static T8lMContext context;
static unsigned enters, exits;
static T8lMResult poll(bool down, uint8_t stock, unsigned elapsed_ms) {
    ticks+=elapsed_ms*4u;
    T8lMResult r=t8l_m_bridge_poll(&bridge, down, stock, context,
                                  t8l_clock_advance(&clock_state, ticks));
    enters+=r.gesture==FINDER_ENTERED;
    exits+=r.gesture==FINDER_EXITED;
    return r;
}
static void reset(bool held) {
    ticks=UINT32_MAX-1000; /* Every gesture test crosses the real raw wrap. */
    t8l_clock_reset(&clock_state,ticks);
    t8l_m_bridge_reset(&bridge);
    context=(T8lMContext){true,true,true,false,false};
    enters=exits=0;
    poll(held,0,0);
    poll(held,0,45);
}
static void tap(void) {
    poll(true,1,1);
    poll(true,1,45);
    poll(false,2,55);
    poll(false,0,45);
}
int main(void) {
    reset(false);
    tap();
    assert(enters==0);
    assert(poll(true,255,50).stock_event==0);
    assert(enters==0); /* Toggle only on release, not stock double event. */
    poll(true,1,45);
    poll(false,2,55);
    assert(poll(false,0,45).enabled);
    assert(enters==1);
    tap(); tap();
    assert(!bridge.button.enabled && exits==1);

    reset(false);
    for (unsigned i=0;i<12;++i) poll((i&1)==0,0,5);
    poll(false,0,50);
    tap();
    assert(enters==0); /* Bounce does not become a first click. */

    reset(true);
    poll(false,0,100);
    poll(false,0,45);
    tap();
    assert(enters==0); /* Initial held/released M did not count. */
    tap();
    assert(enters==1);

    reset(false);
    tap();
    poll(true,1,50);
    poll(true,1,45);
    assert(poll(true,3,1000).stock_event==3);
    assert(poll(true,7,4000).stock_event==7);
    poll(false,0,1);
    poll(false,0,45);
    assert(enters==0); /* A short + long press is not a double tap. */

    /* Every guard cancels search and forwards ALL stock events unchanged. */
    for (unsigned guard=0;guard<5;++guard) {
        reset(false); tap(); tap();
        assert(bridge.button.enabled);
        switch(guard) {
        case 0: context.mapping_verified=false; break;
        case 1: context.normal_runtime=false; break;
        case 2: context.disarmed_verified=false; break;
        case 3: context.power_down=true; break;
        case 4: context.stock_m_busy=true; break;
        }
        for (unsigned e=0;e<256;++e) {
            T8lMResult r=poll(true,(uint8_t)e,1);
            assert(r.stock_event==e && !r.enabled);
        }
        assert(exits==1);
        context=(T8lMContext){true,true,true,false,false};
        poll(true,255,100); /* Still held across mode change: no ownership. */
        assert(poll(true,255,50).stock_event==255);
        poll(false,0,1); poll(false,0,45);
        tap(); assert(enters==1);
        tap(); assert(enters==2);
    }
    reset(false);
    for (unsigned e=0;e<255;++e)
        assert(poll(false,(uint8_t)e,1).stock_event==e);
    puts("M bridge: double enter/exit, bounce, startup, long bind, all guards OK");
}
