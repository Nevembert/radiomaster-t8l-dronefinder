#include "finder_audio.h"
/* Smooth cadence from 1.6 s at -110 dBm to 0.2 s at -20 dBm.
 * This is a signal-strength cue, never a distance estimate. */
uint32_t finder_audio_period(int16_t q4) {
    int32_t strength=(int32_t)q4+110*16;
    if (strength<=0) return 1600;
    if (strength>=90*16) return 200;
    return 1600u-(uint32_t)(strength*1400/(90*16));
}
bool finder_audio_due(FinderAudio *a,bool active,int16_t q4,uint32_t now) {
    if (!active) { *a=(FinderAudio){0}; return false; }
    if (!a->running || (uint32_t)(now-a->last_ms)>=finder_audio_period(q4)) {
        a->last_ms=now; a->running=true; return true;
    }
    return false;
}
