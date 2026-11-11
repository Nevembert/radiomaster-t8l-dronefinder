#!/usr/bin/env python3
"""Read-only verifier for one official T8L image. Never patches or flashes."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

BASE = 0x08006000
EXPECTED_SHA256 = "91c69ad34105bd12dc167f29cca538f84c474e18ec36902ce503b578ce03a958"
EXPECTED_SIZE = 94536


def decode_data(image):
    """Static reconstruction of the scatter-loader at 0x080066a0."""
    pos = 0x0801CEA8 - BASE
    out = bytearray()

    def take():
        nonlocal pos
        if pos >= len(image):
            raise ValueError("truncated compressed data")
        value = image[pos]
        pos += 1
        return value

    while len(out) < 0x4B0:
        control = take()
        literals = control & 7
        if not literals:
            literals = take()
        count = control >> 4
        if not count:
            count = take()
        if literals < 1:
            raise ValueError("invalid literal count")
        for _ in range(literals - 1):
            out.append(take())
        if control & 8:
            distance = take()
            if not distance or distance > len(out):
                raise ValueError("invalid back-reference")
            for _ in range(count + 2):
                out.append(out[-distance])
        else:
            out.extend(bytes(count))
        if len(out) > 0x4B0:
            raise ValueError("decompressed data overflow")
    return bytes(out), BASE + pos


def analyze(image):
    digest = hashlib.sha256(image).hexdigest()
    if len(image) != EXPECTED_SIZE or digest != EXPECTED_SHA256:
        raise ValueError("unsupported firmware: size/SHA-256 differ; no address map applied")
    msp, reset = struct.unpack_from("<II", image)
    entries = struct.unpack_from("<8I", image, 0x0801CE88 - BASE)
    expected = (0x0801CEA8,0x20000000,0x4B0,0x080066A0,
                0x0801D148,0x200004B0,0x4088,0x0801B9C6)
    if entries != expected:
        raise ValueError("scatter table differs")
    data, end = decode_data(image)
    pin_bytes = data[0x490:0x49C]
    if data[0x330] != 6 or pin_bytes != bytes.fromhex("01 05 01 04 01 03 00 0f 03 0f 03 0e"):
        raise ValueError("LED table differs")
    key_pointers = struct.unpack_from("<17I", data, 0x1A0)
    if key_pointers[:2] != (0x2000065E, 0x20001B01):
        raise ValueError("power/M input pointers differ")
    if data[0x18D:0x18F] != b"\x01\x01" or data[0x48E:0x490] != b"\x00\x05":
        raise ValueError("M GPIO or polarity differs")
    if data[0x49E:0x4A0] != b"\x02\x0f":
        raise ValueError("power GPIO differs")
    systick_vector = struct.unpack_from("<I", image, 0x3C)[0]
    timer_vector = struct.unpack_from("<I", image, 0xA4)[0]
    if systick_vector != 0x0800B821 or timer_vector != 0x0800B939:
        raise ValueError("interrupt vectors differ")
    # Evidence windows at the instructions traced in RESEARCH.md.
    locations = {
        "led_apply": (0x0800F994, 24),
        "digital_write": (0x0801076C, 24),
        "battery_render_call": (0x08016824, 4),
        "link_stats_parse": (0x08018B18, 16),
        "downlink_rssi_store": (0x08018B6A, 6),
        "link_stats_event_return": (0x08018B96, 6),
        "stale_rssi_decrement": (0x0801AB60, 14),
        "link_stats_age_reset": (0x080155AE, 12),
        "timer_calls_led_scheduler": (0x0800B95C, 4),
        "systick_reload_divide_4000": (0x0800B7E8, 32),
        "systick_counter_increment": (0x0800B82E, 14),
        "gpio_input_scan": (0x08019562, 20),
        "key_scan_call": (0x08014B92, 4),
        "stock_key_handler_call": (0x08014BA4, 4),
        "scanner_double_event": (0x08017B48, 8),
        "stock_m_hold_compare": (0x0801210A, 8),
        "settings_flash_write": (0x0801A514, 28),
        "calibration_flash_write": (0x0801A480, 28),
        "heap_lazy_initialize": (0x0801BA68, 24),
        "returning_loop_iteration": (0x0801725E, 4),
    }
    return {
        "status": "static-analysis-only; not an installable modification",
        "sha256": digest, "size":len(image), "base":hex(BASE),
        "msp":hex(msp), "reset":hex(reset),
        "initialized_data_size":len(data), "compressed_data_end":hex(end),
        "initialized_data_sha256":hashlib.sha256(data).hexdigest(),
        "led_pins_in_logical_order":[f"P{'ABCD'[pin_bytes[i]]}{pin_bytes[i+1]}" for i in range(0,12,2)],
        "led_states_address":"0x20001008", "link_stats_address":"0x20000dbd",
        "downlink_rssi_address":"0x20000dc4", "link_stats_age_counter":"0x20000b2c",
        "timebase":{"nominal_ticks_per_second":4000, "counter":"0x20000e1c",
                    "systick_vector":hex(systick_vector),
                    "note":"static configuration; oscillator timing not measured"},
        "keys":{"m":{"index":1,"raw_address":hex(key_pointers[1]),
                       "pin":"PA5","active_low":True},
                "power":{"index":0,"raw_address":hex(key_pointers[0]),
                           "pin":"PC15","active_low":True},
                "event_array":"0x2000066d", "double_event":255,
                "stock_handler":"0x08011d20", "candidate_call_hook":"0x08014ba4",
                "note":"inferred from input tables and bind-handler control flow"},
        "settings_flash_write_base":"0x0801f800",
        "calibration_flash_write_base":"0x0801f000",
        "heap":{"start":hex(struct.unpack_from("<I",image,0x0801BA88-BASE)[0]),
                "end":hex(struct.unpack_from("<I",image,0x0801BA8C-BASE)[0]),
                "end_literal":"0x0801ba8c",
                "note":"end reservation requires patching literal; headroom not measured"},
        "evidence": {k:{"address":hex(a),"bytes":image[a-BASE:a-BASE+n].hex(" ")}
                     for k,(a,n) in locations.items()},
        "not_proven":["physical unit flash identity", "flash capacity/reserved pages",
                      "heap headroom after RAM reservation", "hardware state mapping", "IRQ behavior on hardware",
                      "firmware update recovery", "hardware operation"],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path)
    args = parser.parse_args()
    try:
        report = analyze(args.firmware.read_bytes())
    except (OSError, ValueError) as exc:
        parser.exit(2, f"ERROR: {exc}\n")
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
