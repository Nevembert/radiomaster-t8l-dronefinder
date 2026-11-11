#ifndef T8L_M_BRIDGE_H
#define T8L_M_BRIDGE_H
#include "m_button.h"

typedef struct {
    bool mapping_verified;
    bool normal_runtime;
    bool disarmed_verified;
    bool power_down;
    bool stock_m_busy; /* includes a pending stock M command, not only RF bind */
} T8lMContext;

typedef struct {
    FinderButton button;
    uint32_t raw_since_ms;
    bool initialized, raw_down, stable_down, ready;
} T8lMBridge;

typedef struct {
    FinderGesture gesture;
    uint8_t stock_event;
    bool enabled;
} T8lMResult;

void t8l_m_bridge_reset(T8lMBridge *bridge);
/* Serial foreground call after fresh input scan, before stock key handler.
 * Takes normalized raw M (v1.7 PA5 active low); time comes from T8lClock.
 * Does not write firmware memory. Caller passes result.stock_event to the
 * original handler in a COPY of its event array, preserving the other 16 keys
 * and original return value. No hook/trampoline is installed by this module.
 * Missing state mappings must leave mapping_verified/disarmed_verified false.
 */
T8lMResult t8l_m_bridge_poll(T8lMBridge *bridge, bool raw_m_down,
                           uint8_t stock_m_event, T8lMContext context,
                           uint32_t now_ms);
#endif
