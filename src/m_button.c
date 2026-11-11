#include "m_button.h"
#define SHORT_PRESS_MS 250u
#define DOUBLE_GAP_MS 350u
void finder_button_reset(FinderButton *b) {
    *b = (FinderButton){.wait_release=true};
}
FinderGesture finder_button_tick(FinderButton *b, bool down, bool allowed, uint32_t now) {
    if (!allowed) {
        bool was_enabled=b->enabled;
        finder_button_reset(b);
        return was_enabled ? FINDER_EXITED : FINDER_NO_CHANGE;
    }
    if (b->wait_release) {
        if (!down) b->wait_release=false;
        return FINDER_NO_CHANGE;
    }
    if (down && !b->down) {
        if (b->pending_click && (uint32_t)(now-b->first_release_ms)>DOUBLE_GAP_MS)
            b->pending_click=false;
        b->pressed_ms=now;
    }
    FinderGesture result=FINDER_NO_CHANGE;
    if (!down && b->down) {
        if ((uint32_t)(now-b->pressed_ms)>SHORT_PRESS_MS) {
            b->pending_click=false;
        } else if (b->pending_click) {
            b->pending_click=false;
            b->enabled=!b->enabled;
            result=b->enabled ? FINDER_ENTERED : FINDER_EXITED;
        } else {
            b->pending_click=true;
            b->first_release_ms=now;
        }
    }
    if (!down && b->pending_click && (uint32_t)(now-b->first_release_ms)>DOUBLE_GAP_MS)
        b->pending_click=false;
    b->down=down;
    return result;
}
