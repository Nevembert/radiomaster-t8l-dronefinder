#ifndef T8L_M_BUTTON_H
#define T8L_M_BUTTON_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { FINDER_NO_CHANGE, FINDER_ENTERED, FINDER_EXITED } FinderGesture;
typedef struct {
    uint32_t pressed_ms, first_release_ms;
    bool enabled, down, pending_click, wait_release;
} FinderButton;
void finder_button_reset(FinderButton *button);
/* m_down must already be debounced. allowed requires normal runtime,
 * verified disarmed control state, and power button released.
 * Reports a double SHORT click only. Does not dispatch or consume stock bind.
 * Integration must preserve stock long-M and startup M+power handling.
 */
FinderGesture finder_button_tick(FinderButton *button, bool m_down,
                                 bool allowed, uint32_t now_ms);
#endif
