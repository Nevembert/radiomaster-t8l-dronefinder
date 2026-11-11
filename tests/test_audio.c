#include "finder_audio.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    FinderAudio a={0};
    assert(finder_audio_period(-130*16)==1600);
    assert(finder_audio_period(-20*16)==200);
    assert(finder_audio_period(-1*16)==200);
    uint32_t previous=1600;
    for (int q=-130*16;q<0;++q) {
        uint32_t p=finder_audio_period((int16_t)q);
        assert(p>=200 && p<=1600 && p<=previous); previous=p;
    }
    assert(!finder_audio_due(&a,false,-30*16,0));
    assert(finder_audio_due(&a,true,-20*16,UINT32_MAX-99));
    assert(!finder_audio_due(&a,true,-20*16,99));
    assert(finder_audio_due(&a,true,-20*16,100));
    assert(!finder_audio_due(&a,false,-20*16,101));
    assert(finder_audio_due(&a,true,-110*16,102));
    assert(!finder_audio_due(&a,true,-110*16,1701));
    assert(finder_audio_due(&a,true,-110*16,1702));
    /* No bursts to catch up after a delayed foreground loop. */
    assert(finder_audio_due(&a,true,-20*16,100000));
    assert(!finder_audio_due(&a,true,-20*16,100000));
    puts("audio: monotonic bounded cadence, wrap, disable/re-entry, no catch-up bursts OK");
}
