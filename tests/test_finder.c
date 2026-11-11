#include "finder.h"
#include <assert.h>
#include <stdio.h>
static FinderOutput output(Finder *f,uint32_t t) {return finder_output(f,true,false,false,false,t);}
int main(void) {
    Finder f; finder_reset(&f);
    assert(!output(&f,0).signal_valid);
    const int16_t samples[]={-110,-80,-60,-25};
    const uint8_t masks[]={1,3,7,15};
    for(unsigned i=0;i<4;i++) {finder_reset(&f);finder_sample(&f,samples[i],true,100);assert(output(&f,100).led_mask==masks[i]);}
    assert(output(&f,1599).signal_valid);assert(!output(&f,1600).signal_valid);
    finder_sample(&f,-25,true,1700);finder_sample(&f,-25,false,1701);assert(!output(&f,1701).signal_valid);
    finder_sample(&f,0,true,1800);assert(!output(&f,1800).signal_valid);
    finder_sample(&f,-131,true,1801);assert(!output(&f,1801).signal_valid);
    finder_sample(&f,-25,true,1900);assert(!finder_output(&f,true,true,false,false,1901).override_leds);
    assert(!output(&f,1902).signal_valid);
    finder_sample(&f,-25,true,2000);assert(!finder_output(&f,true,false,true,false,2001).override_leds);
    finder_sample(&f,-25,true,2100);assert(!finder_output(&f,true,false,false,true,2101).override_leds);
    finder_sample(&f,-25,true,2200);assert(!finder_output(&f,false,false,false,false,2201).override_leds);
    finder_sample(&f,-80,true,UINT32_MAX-100);assert(output(&f,100).signal_valid);assert(!output(&f,1500).signal_valid);
    finder_reset(&f);finder_sample(&f,-91,true,0);
    for(uint32_t t=1;t<100;t++){finder_sample(&f,(t&1)?-89:-91,true,t);assert(output(&f,t).led_mask==1);}
    for(uint32_t t=100;t<200;t++)finder_sample(&f,-25,true,t);
    assert(output(&f,200).led_mask==15);
    for(uint32_t t=201;t<301;t++)finder_sample(&f,-110,true,t);
    assert(output(&f,301).led_mask==1);
    puts("PASS: levels, timeout, disconnect, invalid RSSI, priorities, timer wrap, hysteresis, convergence");
}
