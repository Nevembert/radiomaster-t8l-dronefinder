#include "platform.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static uint8_t ram[0x4538], output[6], seen_m, events[17];
static uint32_t irq_mask, irq_saves, irq_restores, parser_result;
static bool mutate_keys, interrupt_on_restore, in_irq;
static unsigned led_calls;
static unsigned beep_calls;
static const uint8_t normal_leds[6]={0,1,0,1,1,0};
static unsigned offset(uint32_t address,unsigned n) {
    assert(address>=0x20000000u && address+n<=0x20004538u);
    return address-0x20000000u;
}
uint8_t board_u8(uint32_t a) {return ram[offset(a,1)];}
uint16_t board_u16(uint32_t a) {
    unsigned o=offset(a,2); return ram[o]|((uint16_t)ram[o+1]<<8);
}
uint32_t board_u32(uint32_t a) {
    unsigned o=offset(a,4);
    return ram[o]|((uint32_t)ram[o+1]<<8)|((uint32_t)ram[o+2]<<16)|((uint32_t)ram[o+3]<<24);
}
static void put(uint32_t a,uint32_t v,unsigned n) {
    unsigned o=offset(a,n); for(unsigned i=0;i<n;++i) ram[o+i]=(uint8_t)(v>>(8*i));
}
uint32_t board_irq_save(void) {uint32_t old=irq_mask; irq_mask=1; ++irq_saves; return old;}
void board_irq_restore(uint32_t old) {
    assert(irq_mask==1); irq_mask=old; ++irq_restores;
    if (!old && interrupt_on_restore && !in_irq) {
        in_irq=true;
        assert(hook_leds(normal_leds)==0x1234);
        in_irq=false;
    }
}
uint32_t stock_init(void) {return 0x12345678;}
uint32_t stock_loop(void) {return 0x87654321;}
uint32_t stock_keys(uint8_t *e) {
    seen_m=e[1];
    if (mutate_keys) {e[3]=99; e[1]=42;}
    return 0xabc;
}
uint32_t stock_crsf(const uint8_t *f,uint32_t n) {(void)f;(void)n; return parser_result;}
uint32_t stock_leds(const uint8_t *l) {memcpy(output,l,6); ++led_calls; return 0x1234;}
uint32_t stock_beep(uint8_t count,uint16_t duration) {
    assert(count==1 && duration==50 && irq_mask==1);
    ++beep_calls; return 0;
}

static void setup(void) {
    memset(ram,0,sizeof(ram)); memset(events,0,sizeof(events));
    irq_mask=irq_saves=irq_restores=0;
    mutate_keys=interrupt_on_restore=in_irq=false;
    led_calls=beep_calls=0; parser_result=13;
    put(0x20000e1c,UINT32_MAX-1000,4);
    put(0x20000e00,0x20001b40,4);
    put(0x20001b47,10,1);
    put(0x200002b0,3,2); put(0x200002b4,1,1);
    put(0x2000065e,1,1); put(0x20000690,988,4);
    put(0x2000032c,660,2); put(0x2000032e,840,2); put(0x20001af4,800,2);
    put(0x20001b01,1,1);
    put(0x20000e20,0x20001d00,4); put(0x20001d0c,32,4);
    put(0x20000182,1,1); put(0x20000004,1,4);
    put(0x20000000,0x20001e04,4); put(0x20001e04,0x400,4);
    for(unsigned i=0;i<6;++i) put(0x20001008+i,normal_leds[i],1);
    assert(hook_init()==0x12345678);
    assert(hook_keys(events)==0xabc);
    assert(hook_leds(normal_leds)==0x1234);
}
static void step(bool down,unsigned ms,uint8_t event) {
    put(0x20000e1c,board_u32(0x20000e1c)+ms*4u,4);
    put(0x20001b01,!down,1); events[1]=event;
    assert(hook_keys(events)==0xabc);
    assert(hook_loop()==0x87654321);
}
static void tap(void) {
    step(true,1,1); step(true,45,1); step(false,55,2); step(false,45,0);
}
static void enter(void) {step(false,45,0); tap(); tap();}
static uint8_t crc(const uint8_t *p,unsigned n) {
    uint8_t v=0;
    while(n--) {v^=*p++; for(unsigned i=0;i<8;++i) v=(uint8_t)((v<<1)^((v&128)?0xd5:0));}
    return v;
}
static void packet(int rssi) {
    uint8_t f[14]={0xea,12,0x14,80,80,100,0,0,2,1,0,0,0,0};
    f[10]=(uint8_t)rssi; f[13]=crc(f+2,11);
    assert(hook_crsf(f,14)==parser_result);
}
static bool equals_stock(void) {return memcmp(output,normal_leds,6)==0;}
int main(void) {
    setup(); enter(); assert(beep_calls==0); /* missing link is silent */
    packet(-20); step(false,1,0); assert(beep_calls==1);
    step(false,199,0); assert(beep_calls==1);
    step(false,1,0); assert(beep_calls==2);
    step(false,1500,0); assert(beep_calls==2); /* stale data is silent */
    packet(-20); step(false,1,0); assert(beep_calls==3);
    tap(); tap(); unsigned stopped=beep_calls;
    packet(-20); step(false,500,0); assert(beep_calls==stopped);
    /* Busy native sound, invalid pointers, depleted/fragmented/malformed heap. */
    const uint32_t sound_gates[]={0x20000e20,0x20001d00,0x20001d04,0x20001d08,
        0x20001d0c,0x20000e24,0x20000e28,0x20000182,0x20000004,
        0x20000000,0x20001e04,0x20001e08};
    const uint32_t sound_values[]={0,1,1,1,1,1,1,0,0,0,40,0x20001e04};
    const unsigned sound_sizes[]={4,4,4,4,4,2,4,1,4,4,4,4};
    for(unsigned i=0;i<sizeof(sound_gates)/sizeof(sound_gates[0]);++i) {
        setup(); enter(); put(sound_gates[i],sound_values[i],sound_sizes[i]);
        packet(-20); step(false,1,0); assert(beep_calls==0);
    }
    setup(); enter(); /* Two separate valid 24-byte blocks also suffice. */
    put(0x20001e04,24,4); put(0x20001e08,0x20001f04,4);
    put(0x20001f04,24,4); packet(-20); step(false,1,0); assert(beep_calls==1);
    setup(); enter();
    assert(!equals_stock());
    packet(-25); step(false,50,0);
    for(unsigned i=0;i<4;++i) assert(output[i]==1);
    assert(output[4]==1 && output[5]==0);
    /* Caller buffers remain unchanged, and trim LEDs remain caller-owned. */
    uint8_t custom[6]={1,0,1,0,7,9}, before[6]; memcpy(before,custom,6);
    hook_leds(custom); assert(!memcmp(custom,before,6));
    assert(output[4]==7 && output[5]==9);
    tap(); tap(); assert(equals_stock()); /* immediate exit restores battery */

    /* The real caller supplies occupied buffer bytes, not a frame slice. */
    setup(); enter();
    uint8_t buffered[128]={0xea,12,0x14,80,80,100,0,0,2,1,231,0,0,0};
    buffered[13]=crc(buffered+2,11);
    assert(hook_crsf(buffered,sizeof(buffered))==13);
    step(false,50,0);
    assert(output[0] && output[1] && output[2] && output[3]);
    setup(); enter();
    assert(hook_crsf(buffered,13)==13); /* No read beyond short buffer. */
    step(false,50,0);
    assert(!(output[0] && output[1] && output[2] && output[3]));

    setup(); enter(); packet(-25); step(false,50,0);
    parser_result=255; packet(-25);
    step(false,1500,0); /* Last invalid frame did not keep signal alive. */
    assert(!(output[0] && output[1] && output[2] && output[3]));
    put(0x20000e1c,board_u32(0x20000e1c)+1001,4);
    hook_leds(normal_leds); assert(equals_stock()); /* stopped foreground */

    /* IRQ-state restoration: no unconditional enable while already masked. */
    setup(); enter(); packet(-25);
    irq_mask=1; step(false,50,0); hook_leds(normal_leds); assert(irq_mask==1);
    irq_mask=0; interrupt_on_restore=true;
    step(false,50,0); assert(irq_mask==0 && irq_saves==irq_restores);
    assert(output[0] && output[1] && output[2] && output[3]);

    const uint32_t gates[]={0x200002b0,0x200002b4,0x2000065e,0x2000065f,
                           0x20000690,0x2000066c,0x20001af4,0x20000e00,
                           0x20001b47,0x2000032e};
    const uint32_t values[]={1,0,0,1,2012,1,640,0,4,600};
    const unsigned sizes[]={2,1,1,1,4,1,2,4,1,2};
    for(unsigned i=0;i<sizeof(gates)/sizeof(gates[0]);++i) {
        setup(); enter(); packet(-25); step(false,50,0);
        put(gates[i],values[i],sizes[i]);
        hook_leds(normal_leds); assert(equals_stock()); /* immediate IRQ gate */
        step(false,50,0); assert(equals_stock());
        unsigned before=beep_calls;
        packet(-20); step(false,500,0); assert(beep_calls==before);
    }
    setup(); step(false,45,0); events[1]=255;
    assert(hook_keys(events)==0xabc && seen_m==0 && events[1]==255);
    mutate_keys=true; assert(hook_keys(events)==0xabc && events[1]==42 && events[3]==99);
    assert(irq_saves==irq_restores);
    puts("hooks: M -> CRSF -> LEDs, guards, IRQ snapshot, return values, stock side effects OK");
}
