#ifndef T8L_ADAPTER_H
#define T8L_ADAPTER_H
#include "finder.h"
#include <stddef.h>

/* Integration contract, NOT an installed hook or GPIO driver. */
typedef struct {
    bool requested;
    bool controls_verified; /* false until the firmware state mapping is known */
    bool armed;
    bool battery_warning;
    bool system_busy;
} T8lFinderControls;

/* Feed only newly delivered complete CRSF frames from ELRS 3.6.2.
 * Never feed USB 0x67 snapshots or periodically poll the stock RAM copy.
 * Returns true for an accepted Link Statistics frame, even if RSSI invalid.
 * Invalid/other frames cannot extend the last-sample timestamp.
 */
bool t8l_finder_frame(Finder *f, const uint8_t *frame, size_t size,
                     uint32_t now_ms);

/* Compose into a caller-owned six-byte copy of the original LED states.
 * Does not touch GPIO, global firmware memory, or the last two trim LEDs.
 * All API calls must be serialized by the eventual firmware integration.
 */
FinderOutput t8l_finder_compose(Finder *f, T8lFinderControls controls,
                              uint32_t now_ms, uint8_t led_states[6]);
#endif
