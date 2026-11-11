#include "t8l_adapter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Independent polynomial long division rather than the production CRC loop. */
static uint8_t reference_crc(const uint8_t *p, size_t n) {
    unsigned r = 0;
    for (size_t i = 0; i < n; ++i) {
        r ^= (unsigned)p[i] << 8;
        for (int bit = 15; bit >= 8; --bit)
            if (r & (1u << bit)) r ^= 0x1d5u << (bit - 8);
        r <<= 8;
    }
    return (uint8_t)(r >> 8);
}
static void packet(uint8_t p[14], int dbm, unsigned lq) {
    const uint8_t base[14] = {0xea,12,0x14,60,61,100,4,0,1,2,0,0,3,0};
    memcpy(p, base, 14);
    p[10] = (uint8_t)dbm;
    p[11] = (uint8_t)lq;
    p[13] = reference_crc(p+2,11);
}
static const T8lFinderControls active = {true,true,false,false,false};
int main(void) {
    Finder f; uint8_t p[14], leds[6] = {0,0,0,0,7,9};
    finder_reset(&f);
    /* Exhaustive signed-wire decoding, including malformed positive RSSI. */
    for (unsigned raw=0; raw<256; ++raw) {
        finder_reset(&f); packet(p,(int)raw,0);
        assert(t8l_finder_frame(&f,p,14,10));
        assert(f.have_sample == (raw >= 128));
        if (raw >= 128) assert(f.filtered_q4 == ((int)raw-256)*16);
    }
    const int values[] = {-110,-80,-60,-25};
    const uint8_t masks[] = {1,3,7,15};
    for(unsigned i=0;i<4;i++) {
        finder_reset(&f); packet(p,values[i],0);
        assert(t8l_finder_frame(&f,p,14,100));
        FinderOutput o=t8l_finder_compose(&f,active,100,leds);
        assert(o.signal_valid && o.led_mask==masks[i]);
        for(unsigned j=0;j<4;j++) assert(leds[j]==((masks[i]>>j)&1));
        assert(leds[4]==7 && leds[5]==9);
    }
    /* LBT success does not gate RSSI; zero LQ still accepts real reports. */
    assert(f.have_sample && f.filtered_q4==-25*16);
    for(size_t n=0;n<14;n++) assert(!t8l_finder_frame(&f,p,n,500));
    assert(!t8l_finder_frame(&f,p,15,500));
    for(unsigned i=0;i<14;i++) for(unsigned b=0;b<8;b++) {
        uint8_t bad[14]; memcpy(bad,p,14); bad[i]^=(uint8_t)(1u<<b);
        assert(!t8l_finder_frame(&f,bad,14,500));
        assert(f.last_sample_ms==100);
    }
    assert(t8l_finder_compose(&f,active,1599,leds).signal_valid);
    assert(!t8l_finder_compose(&f,active,1600,leds).signal_valid);
    /* Unverified controls, ARM and stock warnings restore original ownership. */
    for(unsigned mode=0;mode<5;mode++) {
        T8lFinderControls c=active;
        if(mode==0)c.controls_verified=false;
        if(mode==1)c.requested=false;
        if(mode==2)c.armed=true;
        if(mode==3)c.battery_warning=true;
        if(mode==4)c.system_busy=true;
        const uint8_t original[6]={1,0,1,0,7,9};
        memcpy(leds,original,6); assert(t8l_finder_frame(&f,p,14,2000));
        assert(!t8l_finder_compose(&f,c,2001,leds).override_leds);
        assert(memcmp(leds,original,6)==0 && !f.have_sample);
        assert(!t8l_finder_compose(&f,active,2002,leds).signal_valid);
    }
    assert(!t8l_finder_frame(&f,NULL,14,0));
    assert(!t8l_finder_frame(NULL,p,14,0));
    assert(!t8l_finder_compose(NULL,active,0,leds).override_leds);
    puts("PASS: 256 RSSI encodings, all single-bit packet corruptions, lengths, CE_LBT, LED preservation, priorities, stale reports");
}
