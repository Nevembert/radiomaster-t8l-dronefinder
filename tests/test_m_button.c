#include "m_button.h"
#include <assert.h>
#include <stdio.h>
static FinderGesture tick(FinderButton *b, bool down, uint32_t now) {
    return finder_button_tick(b,down,true,now);
}
static FinderGesture double_click(FinderButton *b, uint32_t now) {
    assert(tick(b,true,now)==FINDER_NO_CHANGE);
    assert(tick(b,false,now+80)==FINDER_NO_CHANGE);
    assert(tick(b,true,now+160)==FINDER_NO_CHANGE);
    return tick(b,false,now+240);
}
int main(void) {
    FinderButton b; finder_button_reset(&b);
    /* Held during startup is ignored until released. */
    assert(tick(&b,true,0)==FINDER_NO_CHANGE);
    assert(tick(&b,true,3000)==FINDER_NO_CHANGE);
    assert(tick(&b,false,4000)==FINDER_NO_CHANGE);
    assert(double_click(&b,4100)==FINDER_ENTERED && b.enabled);
    assert(double_click(&b,5000)==FINDER_EXITED && !b.enabled);
    /* Stock bind hold, or short followed by long, must never toggle search. */
    assert(tick(&b,true,6000)==FINDER_NO_CHANGE);
    assert(tick(&b,false,7100)==FINDER_NO_CHANGE && !b.enabled);
    tick(&b,true,8000); tick(&b,false,8080); tick(&b,true,8200);
    assert(tick(&b,false,9400)==FINDER_NO_CHANGE && !b.enabled);
    /* Two isolated clicks do not count. */
    tick(&b,true,10000); tick(&b,false,10080); tick(&b,true,11000);
    assert(tick(&b,false,11080)==FINDER_NO_CHANGE && !b.enabled);
    tick(&b,false,12000);
    assert(double_click(&b,13000)==FINDER_ENTERED);
    /* Power chord/arming/configuration cancels search and requires release. */
    assert(finder_button_tick(&b,true,false,14000)==FINDER_EXITED);
    assert(tick(&b,true,15000)==FINDER_NO_CHANGE && !b.enabled);
    assert(tick(&b,false,16000)==FINDER_NO_CHANGE);
    assert(double_click(&b,UINT32_MAX-100)==FINDER_ENTERED);
    /* Auto-exit does not allow a held M to become a new click on recovery. */
    assert(finder_button_tick(&b,true,false,500)==FINDER_EXITED);
    assert(tick(&b,true,600)==FINDER_NO_CHANGE);
    assert(tick(&b,false,700)==FINDER_NO_CHANGE);
    puts("PASS: double M, bind hold, startup/power chord, isolated clicks, auto-exit, timer wrap");
}
