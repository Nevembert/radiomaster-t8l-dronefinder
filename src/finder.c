#include "finder.h"
/* Provisional thresholds: top boundary adjusted using the first USB bench
 * trace to distinguish nearby/attenuated samples. Not distance calibration. */
static const int16_t threshold[3] = {-90, -70, -35};
void finder_reset(Finder *f) { *f = (Finder){0}; }
void finder_sample(Finder *f, int16_t rssi_dbm, bool connected, uint32_t now_ms) {
    if (!connected || rssi_dbm < -130 || rssi_dbm >= 0) {
        finder_reset(f);
        return;
    }
    bool first = !f->have_sample || (uint32_t)(now_ms-f->last_sample_ms) >= 1500u;
    int16_t q4 = (int16_t)(rssi_dbm * 16);
    if (first) f->filtered_q4=q4;
    else f->filtered_q4=(int16_t)(f->filtered_q4 + (q4-f->filtered_q4)/4);
    f->last_sample_ms=now_ms;
    f->have_sample=true;
    if (first) {
        f->level=1;
        while (f->level<4 && f->filtered_q4>=threshold[f->level-1]*16) ++f->level;
    } else {
        while (f->level<4 && f->filtered_q4>=(threshold[f->level-1]+3)*16) ++f->level;
        while (f->level>1 && f->filtered_q4<(threshold[f->level-2]-3)*16) --f->level;
    }
}
FinderOutput finder_output(Finder *f, bool enabled, bool armed,
                           bool battery_warning, bool system_busy, uint32_t now_ms) {
    FinderOutput out={false,false,0};
    if (!enabled || armed || battery_warning || system_busy) {
        /* On re-entry, require a new sample: never reuse a signal from flight. */
        finder_reset(f);
        return out;
    }
    out.override_leds=true;
    if (!f->have_sample || (uint32_t)(now_ms-f->last_sample_ms)>=1500u) {
        finder_reset(f);
        /* Alternating outer/inner pairs distinguish no link from full signal. */
        out.led_mask=((now_ms/250u)&1u) ? 0x06 : 0x09;
        return out;
    }
    out.signal_valid=true;
    out.led_mask=(uint8_t)((1u<<f->level)-1u);
    return out;
}
