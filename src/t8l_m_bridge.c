#include "t8l_m_bridge.h"

#define DEBOUNCE_MS 45u
#define STOCK_DOUBLE_EVENT 0xffu

void t8l_m_bridge_reset(T8lMBridge *b) {
    *b = (T8lMBridge){0};
    finder_button_reset(&b->button);
}

T8lMResult t8l_m_bridge_poll(T8lMBridge *b, bool down, uint8_t event,
                           T8lMContext context, uint32_t now) {
    if (!b->initialized) {
        b->initialized = true;
        b->raw_down = b->stable_down = down;
        b->raw_since_ms = now;
    }
    if (down != b->raw_down) {
        b->raw_down = down;
        b->raw_since_ms = now;
    }
    bool settled = (uint32_t)(now - b->raw_since_ms) >= DEBOUNCE_MS;
    if (settled) b->stable_down = down;

    bool allowed = context.mapping_verified && context.normal_runtime &&
                   context.disarmed_verified && !context.power_down &&
                   !context.stock_m_busy;
    if (!allowed) b->ready = false;
    /* Require a positively observed stable release before taking ownership.
     * A button held at boot or across a forbidden mode cannot be a first tap.
     */
    if (allowed && !down && settled && !b->stable_down) b->ready = true;
    bool owns_m = allowed && b->ready;
    FinderGesture gesture = finder_button_tick(&b->button, b->stable_down,
                                               owns_m, now);
    /* Stock scanner emits 0xff on a double press. It must not leak into
     * stock >=7 hold-event handling when Finder owns the gesture. Genuine
     * hold events and all events outside the verified context pass through.
     */
    if (owns_m && event == STOCK_DOUBLE_EVENT) event = 0;
    return (T8lMResult){gesture, event, b->button.enabled};
}
