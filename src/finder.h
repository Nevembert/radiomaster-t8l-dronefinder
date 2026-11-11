#ifndef T8L_FINDER_H
#define T8L_FINDER_H
#include <stdbool.h>
#include <stdint.h>
/* Hardware-independent logic. No firmware hooks or GPIO writes. */
typedef struct {
    uint32_t last_sample_ms;
    int16_t filtered_q4;
    uint8_t level;
    bool have_sample;
} Finder;
typedef struct {
    bool override_leds; /* false: original firmware owns LEDs */
    bool signal_valid;
    uint8_t led_mask; /* logical first four LEDs; polarity/order NOT GPIO */
} FinderOutput;
void finder_reset(Finder *f);
void finder_sample(Finder *f, int16_t rssi_dbm, bool connected, uint32_t now_ms);
FinderOutput finder_output(Finder *f, bool enabled, bool armed,
                           bool battery_warning, bool system_busy, uint32_t now_ms);
#endif
