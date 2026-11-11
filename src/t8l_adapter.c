#include "t8l_adapter.h"

static uint8_t crc8_d5(const uint8_t *p, size_t n) {
    uint8_t crc = 0;
    while (n--) {
        crc ^= *p++;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (uint8_t)((crc << 1) ^ ((crc & 0x80u) ? 0xd5u : 0u));
    }
    return crc;
}

bool t8l_finder_frame(Finder *f, const uint8_t *p, size_t n, uint32_t now) {
    if (!f || !p || n != 14 || p[0] != 0xea || p[1] != 12 || p[2] != 0x14)
        return false;
    if (crc8_d5(p + 2, 11) != p[13]) return false;
    /* ELRS 3.6.2 tx_main.cpp assigns LastPacketRSSI directly to uint8_t.
     * Portable two's-complement decode; not 255-raw and not -raw.
     * Payload+7 = frame+10. USB 0x67 uses a DIFFERENT offset (frame+9).
     */
    int16_t dbm = p[10] >= 128 ? (int16_t)p[10] - 256 : (int16_t)p[10];
    /* In CE_LBT, downlink LQ is LBT success, not receiver connectivity.
     * Actual disconnect stops ELRS Link Statistics reports; timeout expires.
     */
    finder_sample(f, dbm, true, now);
    return true;
}

FinderOutput t8l_finder_compose(Finder *f, T8lFinderControls c,
                              uint32_t now, uint8_t states[6]) {
    if (!f || !states) return (FinderOutput){false, false, 0};
    FinderOutput o = finder_output(f, c.requested && c.controls_verified,
                                  c.armed, c.battery_warning, c.system_busy, now);
    if (o.override_leds)
        for (unsigned i = 0; i < 4; ++i)
            states[i] = (uint8_t)((o.led_mask >> i) & 1u);
    return o;
}
