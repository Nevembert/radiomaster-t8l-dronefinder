#ifndef FINDER_AUDIO_H
#define FINDER_AUDIO_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { uint32_t last_ms; bool running; } FinderAudio;
uint32_t finder_audio_period(int16_t filtered_q4);
bool finder_audio_due(FinderAudio *a, bool active, int16_t filtered_q4, uint32_t now);
#endif
