#!/usr/bin/env python3
"""Offline check of the byte templates in samsung_protocol.h.

No host C++ compiler is needed. The script re-implements the checksum exactly as
samsung_protocol.h does, reads the constant arrays back out of the header and
verifies that the frames IRremoteESP8266 transmits verbatim carry a checksum our
algorithm agrees with. If the port of the checksum or one of the templates ever
drifts, this fails.

Usage: python tests/check_protocol.py
"""

from pathlib import Path
import re
import sys

HEADER = Path(__file__).resolve().parent.parent / "components" / "samsung_climate_ir" / "samsung_protocol.h"

SECTION_LENGTH = 7


def read_array(source: str, name: str) -> list[int]:
    """Pull a `static constexpr uint8_t NAME[...] = {...};` array out of the header."""
    match = re.search(rf"{name}\s*\[[^\]]*\]\s*=\s*\{{(.*?)\}}\s*;", source, re.DOTALL)
    if match is None:
        raise SystemExit(f"could not find {name} in {HEADER}")
    return [int(value, 0) for value in match.group(1).replace("\n", " ").split(",") if value.strip()]


def calc_section_checksum(section: list[int]) -> int:
    total = bin(section[0]).count("1")
    total += bin(section[1] & 0x0F).count("1")
    total += bin((section[2] >> 4) & 0x0F).count("1")
    total += sum(bin(byte).count("1") for byte in section[3:7])
    return (total & 0xFF) ^ 0xFF


def get_section_checksum(section: list[int]) -> int:
    return ((section[2] & 0x0F) << 4) | ((section[1] >> 4) & 0x0F)


def check_frame(name: str, frame: list[int]) -> bool:
    ok = True
    for index in range(0, len(frame), SECTION_LENGTH):
        section = frame[index : index + SECTION_LENGTH]
        stored = get_section_checksum(section)
        computed = calc_section_checksum(section)
        status = "ok" if stored == computed else "MISMATCH"
        if stored != computed:
            ok = False
        print(
            f"  {name} section {index // SECTION_LENGTH + 1}: "
            f"stored 0x{stored:02X} computed 0x{computed:02X}  {status}"
        )
    return ok


def main() -> int:
    source = HEADER.read_text(encoding="utf-8")

    # Sent verbatim by the component, so its stored checksum must be correct.
    off_frame = read_array(source, "POWER_OFF_FRAME")
    if len(off_frame) != 21:
        raise SystemExit(f"POWER_OFF_FRAME should be 21 bytes, got {len(off_frame)}")

    print("POWER_OFF_FRAME (transmitted as-is, checksum must already be valid)")
    ok = check_frame("off", off_frame)

    # The reset state is only a starting point: IRremoteESP8266 recomputes its
    # checksum before every send, so it is only length-checked here.
    reset_state = read_array(source, "RESET_STATE")
    if len(reset_state) != 14:
        raise SystemExit(f"RESET_STATE should be 14 bytes, got {len(reset_state)}")
    print("RESET_STATE: 14 bytes, checksum recomputed at transmit time")

    middle = read_array(source, "EXTENDED_MIDDLE_SECTION")
    if len(middle) != SECTION_LENGTH:
        raise SystemExit(f"EXTENDED_MIDDLE_SECTION should be 7 bytes, got {len(middle)}")
    print("EXTENDED_MIDDLE_SECTION: 7 bytes")

    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
