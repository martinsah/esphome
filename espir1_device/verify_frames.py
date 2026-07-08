#!/usr/bin/env python3
"""Compare PD-Pioneer frame bytes against captured reference values."""

from __future__ import annotations

# Reference frames from captured_data.txt / pronto_decoder (complete decodes)
REFERENCES = {
    "68F cool fan high odd": "23 CB 26 02 00 40 C0 00 C3 00 00 00 00 E8",
    "68F cool fan high even": "23 CB 26 01 00 24 03 0B 05 00 00 00 80 CC",
    "off odd": "23 CB 26 02 00 40 60 00 C3 00 00 00 00 88",
    "off even": "23 CB 26 01 00 A0 01 0C 03 00 00 00 80 45",
}


def parse_hex(line: str) -> list[int]:
    return [int(x, 16) for x in line.split()]


def build_cool_68f_high() -> tuple[list[int], list[int]]:
    odd = parse_hex("23 CB 26 02 00 40 C0 00 C3 00 00 00 00")
    even = parse_hex("23 CB 26 01 00 24 03 0B 05 00 00 00 80")
    odd.append((sum(odd) + 15) & 0xFF)
    even.append(sum(even) & 0xFF)
    return odd, even


def compare(name: str, got: list[int], expected_line: str) -> None:
    expected = parse_hex(expected_line)
    ok = got == expected
    status = "OK" if ok else "MISMATCH"
    print(f"{name}: {status}")
    if not ok:
        print(f"  expected: {' '.join(f'{b:02X}' for b in expected)}")
        print(f"  got:      {' '.join(f'{b:02X}' for b in got)}")


def main() -> None:
    odd, even = build_cool_68f_high()
    compare("68F cool fan high odd", odd, REFERENCES["68F cool fan high odd"])
    compare("68F cool fan high even", even, REFERENCES["68F cool fan high even"])
    print()
    print(
        "Compare TX log lines from ESPIR1 (TX odd / TX even) to the expected values above."
    )


if __name__ == "__main__":
    main()
