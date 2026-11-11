/* Experimental v1.7 integration. Hardware/bootloader validation outstanding.
 * Foreground owns Finder, M and clock. IRQ sees ONLY the protected publication.
 * Address map is usable only with the exact image checked by the patch planner.
 */
#include "platform.h"
#include "t8l_clock.h"
#include "t8l_m_bridge.h"
#include "t8l_adapter.h"
#include "finder_audio.h"

#define TICKS_ADDR 0x20000e1cu
#define CONFIG_POINTER 0x20000e00u
#define HEAP_START 0x20001b3cu
#define HEAP_LIMIT 0x20002fb8u
#define MAGIC 0x54463834u

typedef struct {
    uint32_t magic;
    T8lClock clock;
    T8lMBridge m;
    Finder finder;
    uint32_t last_render_ms;
    volatile uint32_t published_ticks;
    volatile uint32_t published_flags; /* bit 4 valid; bits 0..3 LED mask */
    FinderAudio audio;
} Runtime;
static Runtime runtime;
_Static_assert(sizeof(Runtime)<=128, "reserved heap tail too small");

static uint32_t now_ms(void) {
    return t8l_clock_advance(&runtime.clock,board_u32(TICKS_ADDR));
}

static bool base_context(void) {
    uint32_t config=board_u32(CONFIG_POINTER);
    /* The 432-byte config is allocated by the original allocator. */
    if (config<HEAP_START+4 || config>HEAP_LIMIT-432 || (config&3u)) return false;
    if ((int16_t)board_u16(0x200002b0u)!=3 || !(board_u8(0x200002b4u)&1u)) return false;
    if (board_u8(0x2000065eu)!=1 || board_u8(0x2000065fu)!=0) return false;
    if (board_u8(config+7)<5 || board_u8(config+7)>16) return false;
    /* Final output CH5 before CRSF conversion, not the raw physical switch.
     * <=1500 us corresponds to ELRS AUX1 disarmed. This is NOT FC motor proof.
     */
    uint32_t ch5=board_u32(0x20000690u);
    return ch5>=988 && ch5<=1500;
}

static bool battery_low(void) {
    /* Preserve stock battery indication at/below its zero-percent threshold;
     * stock severe battery shutdown compares an even lower threshold. */
    uint16_t low=board_u16(0x2000032cu);
    uint16_t high=board_u16(0x2000032eu);
    uint16_t measured=board_u16(0x20001af4u);
    return low==0 || high<=low || measured<=low;
}

static bool can_display(void) {
    return runtime.magic==MAGIC && base_context() &&
           board_u8(0x2000066cu)==0 && !battery_low();
}

static void publish(uint32_t flags) {
    uint32_t saved=board_irq_save();
    runtime.published_ticks=board_u32(TICKS_ADDR);
    runtime.published_flags=flags;
    board_irq_restore(saved);
}

/* The original beep allocates two 16-byte nodes (24 bytes each with allocator
 * header/alignment), without checking malloc failure. Preflight a bounded,
 * monotonically ordered free list while IRQs are masked. Fail closed. */
static bool audio_heap_room(void) {
    uint32_t p=board_u32(0x20000000u), slots=0;
    if (board_u32(0x20000004u)!=1) return false;
    for (unsigned n=0;p && n<64;++n) {
        if (p<HEAP_START || p>HEAP_LIMIT-8 || ((p-HEAP_START)&7u)) return false;
        uint32_t size=board_u32(p), next=board_u32(p+4);
        if (size<8 || (size&7u) || size>HEAP_LIMIT-p) return false;
        if (next && (next<p+size || next>HEAP_LIMIT-8 || ((next-HEAP_START)&7u))) return false;
        slots+=size/24u;
        if (slots>=2) return true;
        p=next;
    }
    return false;
}

static void try_search_beep(void) {
    uint32_t saved=board_irq_save();
    uint32_t q=board_u32(0x20000e20u);
    /* Queue header: head, tail, count, capacity. e24/e28 are the currently
     * scheduled duration/callback; 0182=1 means the stock PWM is off.
     * Never clear/reorder native alerts or overwrite their active sound. */
    bool idle=q>=HEAP_START+4 && q<=HEAP_LIMIT-16 && !(q&3u);
    if (idle) idle=board_u32(q)==0 && board_u32(q+4)==0 &&
                   board_u32(q+8)==0 && board_u32(q+12)>=2 &&
                   board_u16(0x20000e24u)==0 && board_u32(0x20000e28u)==0 &&
                   board_u8(0x20000182u)==1;
    if (idle && audio_heap_room()) (void)stock_beep(1,50);
    board_irq_restore(saved);
}

uint32_t hook_init(void) {
    runtime=(Runtime){0};
    t8l_clock_reset(&runtime.clock,board_u32(TICKS_ADDR));
    t8l_m_bridge_reset(&runtime.m);
    finder_reset(&runtime.finder);
    runtime.magic=MAGIC;
    return stock_init();
}

uint32_t hook_keys(uint8_t *events) {
    if (runtime.magic!=MAGIC) return stock_keys(events);
    bool was_enabled=runtime.m.button.enabled;
    bool permitted=base_context();
    T8lMContext context={true,permitted,permitted,
                        board_u8(0x2000065eu)!=1,board_u8(0x2000066cu)!=0};
    T8lMResult r=t8l_m_bridge_poll(&runtime.m,board_u8(0x20001b01u)==0,
                                  events[1],context,now_ms());
    /* Temporarily replace only M in the ORIGINAL array. All other original
     * handler writes remain observable; restore M only if it was untouched.
     */
    uint8_t original=events[1];
    events[1]=r.stock_event;
    uint32_t result=stock_keys(events);
    if (events[1]==r.stock_event) events[1]=original;
    if (!r.enabled) publish(0);
    if (was_enabled && !r.enabled) {
        uint8_t stock[6];
        for (unsigned i=0;i<6;++i) stock[i]=board_u8(0x20001008u+i);
        hook_leds(stock);
    }
    return result;
}

uint32_t hook_crsf(const uint8_t *frame,uint32_t size) {
    uint8_t snapshot[14];
    /* Stock caller passes the occupied size of its 128-byte receive buffer,
     * not necessarily the first frame's length. Snapshot only a complete
     * Link Statistics frame; trailing buffered bytes belong to the stock code.
     */
    bool candidate=frame && size>=sizeof(snapshot) && size<=128u &&
                   frame[0]==0xeau && frame[1]==12u && frame[2]==0x14u;
    if (candidate) for (unsigned i=0;i<sizeof(snapshot);++i) snapshot[i]=frame[i];
    uint32_t result=stock_crsf(frame,size);
    if (runtime.magic==MAGIC && result==13 && candidate)
        t8l_finder_frame(&runtime.finder,snapshot,sizeof(snapshot),now_ms());
    return result;
}

uint32_t hook_leds(const uint8_t *leds) {
    uint8_t output[6];
    uint32_t saved=board_irq_save();
    uint32_t flags=runtime.published_flags;
    uint32_t stamp=runtime.published_ticks;
    uint32_t ticks=board_u32(TICKS_ADDR);
    board_irq_restore(saved);
    /* A stalled foreground must not leave a trusted signal bar indefinitely.
     * This read-only hook never updates Finder, clock or button state. */
    if (!(flags&16u) || (uint32_t)(ticks-stamp)>1000u || !can_display())
        return stock_leds(leds);
    for (unsigned i=0;i<6;++i) output[i]=leds[i];
    for (unsigned i=0;i<4;++i) output[i]=(uint8_t)((flags>>i)&1u);
    return stock_leds(output);
}

uint32_t hook_loop(void) {
    uint32_t result=stock_loop();
    if (runtime.magic!=MAGIC) return result;
    uint32_t now=now_ms();
    bool allowed=can_display();
    uint8_t unused[6]={0};
    T8lFinderControls controls={runtime.m.button.enabled,true,false,!allowed,false};
    FinderOutput out=t8l_finder_compose(&runtime.finder,controls,now,unused);
    if (finder_audio_due(&runtime.audio,out.override_leds && out.signal_valid,
                         runtime.finder.filtered_q4,now)) try_search_beep();
    uint32_t previous=runtime.published_flags;
    uint32_t flags=out.override_leds ? (16u|out.led_mask) : 0;
    publish(flags);
    if ((previous || flags) &&
        ((bool)previous!=(bool)flags || (uint32_t)(now-runtime.last_render_ms)>=50u)) {
        uint8_t stock[6];
        for (unsigned i=0;i<6;++i) stock[i]=board_u8(0x20001008u+i);
        hook_leds(stock);
        runtime.last_render_ms=now;
    }
    return result;
}
