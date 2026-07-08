# PD-Pioneer IR Protocol

Reverse-engineered protocol for Pioneer ductless mini-split HVAC units controlled via
infrared remote. Derived from ESPIR1 captures and the companion
[`pronto_decoder`](https://github.com/martinsah/pronto_decoder) tooling.

## Overview

Each button press transmits **two sequential IR bursts** (called **odd** and **even** here,
matching the decode tooling). Both bursts carry a 14-byte frame (13 data bytes + checksum).

| Property | Value |
|----------|-------|
| Carrier frequency | 38 kHz |
| Bit order | LSB first within each byte |
| Framing | header + 112 data bits (14 bytes) + footer (no start/stop bits) |
| Word size | 8 bits |
| Header | ~3120 µs mark, ~1560 µs space (Pronto `0078`/`003C`) |
| Bit mark | ~520 µs (Pronto `0014`) |
| Bit 1 space | ~1066 µs (Pronto `0029`) |
| Bit 0 space | ~312 µs (Pronto `000C`) |
| Footer | ~520 µs mark, ~10010 µs space (Pronto `0014`/`0181`) |

Decode captured Pronto logs with:

```bash
cd /path/to/pronto_decoder
python3 test_parse_log.py --start-bits 0 --stop-bits 0 --show-checksum < capture.log
```

## Frame Layout

Both bursts share a common header. Byte index 3 identifies the burst type.

```
Byte:  0    1    2    3    4    5    6    7    8    9   10   11   12   13
       +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
Odd:   | 23 | CB | 26 | 02 | 00 | .. | .. | .. | C3 | 00 | 00 | 00 | 00 | CS |
Even:  | 23 | CB | 26 | 01 | 00 | .. | .. | .. | .. | 00 | 00 | 00 | .. | CS |
       +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
```

| Field | Odd burst (byte 3 = `0x02`) | Even burst (byte 3 = `0x01`) |
|-------|-----------------------------|------------------------------|
| Bytes 0–2 | Fixed header `23 CB 26` | Fixed header `23 CB 26` |
| Byte 4 | Usually `00` | Usually `00` |
| Byte 5 | Fan encoding (see below) | Status / power (see below) |
| Byte 6 | Fan speed level | **Operating mode** |
| Byte 7 | Swing / aux flags | **Temperature encoding** |
| Byte 8 | Fixed `C3` in most captures | Fan sub-code |
| Byte 12 | Usually `00` | Half-degree flag (`0x80` / `0x84`) |
| Byte 13 | Checksum | Checksum |

### Checksums

Let `S = sum(bytes 0–12) mod 256`.

| Burst | Checksum (byte 13) |
|-------|-------------------|
| Odd (`0x02`) | `S + 15` |
| Even (`0x01`) | `S` |

## Operating Mode (even byte 6)

Confirmed from mode-cycle captures:

| Mode | Byte 6 |
|------|--------|
| Heat | `0x01` |
| Dry | `0x02` |
| Cool | `0x03` |
| Fan only | `0x07` |
| Auto | `0x08` |

## Power (even byte 5)

| State | Byte 5 | Notes |
|-------|--------|-------|
| On (normal) | `0x24` | Default running state |
| On (ECO) | `0x25` or `0xA4` | ECO modifies high nibble; needs more captures |
| Off | `0xA0` | Seen in `turn_off_unit` capture |

## Temperature (even byte 7, byte 12)

Encoding is based on **Celsius**, not Fahrenheit. From
`pronto_decoder/temperature_decoding_spreadsheet.ods`:

```
code = 31.75 - temp_C
byte[7]  = floor(code)
byte[12] = 0x80 | (frac(code) < 0.5 ? 0x04 : 0x00)
```

ESPHome stores target temperature in Celsius internally, so no °F conversion is needed
for encoding. Valid range observed: 16–31 °C (61–88 °F).

| °F | °C (approx) | byte 7 | byte 12 |
|----|-------------|--------|---------|
| 88 | 31.11 | `0x00` | `0x80` |
| 76 | 24.44 | `0x07` | `0x84` |
| 73 | 22.78 | `0x08` | `0x80` |
| 72 | 22.22 | `0x09` | `0x80` |
| 71 | 21.67 | `0x0A` | `0x84` |
| 68 | 20.00 | `0x0B` | `0x80` |

Heat mode may use `0x88`/`0x8C` instead of `0x80`/`0x84`; treat as mode-specific
variants of the same half-degree bit.

## Fan Speed

Fan is split across the odd burst (primary) and even burst byte 8 (secondary).

### Odd burst fan encoding

| Fan setting | byte 5 | byte 6 |
|-------------|--------|--------|
| Speed 1 (lowest) | `0x60` | `0x40` |
| Speed 2 | `0x40` | `0x40` |
| Speed 3 | `0x40` | `0x60` |
| Speed 4 | `0x40` | `0x80` |
| Speed 5 | `0x40` | `0xA0` |
| Speed 6 / High | `0x40` | `0xC0` |
| Oscillate | `0x40` | `0x20` |

### Even burst byte 8 (secondary fan code)

| Fan setting | byte 8 |
|-------------|--------|
| Oscillate | `0x00` |
| Speed 1–2 | `0x02` |
| Speed 3–4 | `0x03` |
| Speed 5–6 / High | `0x05` |

### ESPHome mapping (proposed)

| `ClimateFanMode` | Pioneer equivalent |
|------------------|-------------------|
| `CLIMATE_FAN_LOW` | Speed 1–2 |
| `CLIMATE_FAN_MEDIUM` | Speed 3–4 |
| `CLIMATE_FAN_HIGH` | Speed 6 |
| `CLIMATE_FAN_AUTO` | Speed 6 / high (`0xC0`) |

## Swing (odd byte 7)

Partially mapped; all swing-off captures use odd byte 7 = `0x00`.

| Swing state | odd byte 7 | even byte 8 |
|-------------|------------|-------------|
| Off | `0x00` | normal fan code |
| Vertical on | `0x08` | often `0x3B` or `0x3D` |
| Horizontal on | `0x90` | unchanged fan code |

Swing toggles may be sent as a standalone command that only modifies the odd burst.

## Default Templates

Idle cool / 68 °F / fan high starting point:

```
Odd:  23 CB 26 02 00 40 C0 00 C3 00 00 00 00  E8
Even: 23 CB 26 01 00 24 03 0B 05 00 00 00 80  C8
```

Generate transmittable Pronto hex from these bytes:

```bash
cd /path/to/pronto_decoder
python3 create_pronto_from_hex.py \
  --even-message "23 CB 26 02 00 40 C0 00 C3 00 00 00 00" \
  --odd-message  "23 CB 26 01 00 24 03 0B 05 00 00 00 80"
```

Note: `create_pronto_from_hex.py` labels `--even-message` / `--odd-message` opposite
to burst order (first transmitted burst = `--even-message` arg = odd burst `0x02`).

## Decode Pipeline

```
Pronto hex log
  → parse_pronto_log()        strip timestamps, extract 16-bit words
  → decode_pronto_hex()       split into messages, convert counts to µs
  → timing_histogram()        bucket pulse widths
  → reduce_histogram(10%)     merge similar timings (~10% tolerance)
  → meanify_messages()        normalize each pulse to canonical width
  → convert_to_binary(0, 0)   mark<space → 1, else 0; no start/stop bits to skip
  → convert_to_hex(lsbfirst=8)  group into bytes, verify checksum
```

## Known Gaps

- **Light** on/off encoding not isolated (may share swing/aux bits).
- **ECO / preset** byte 5 high-nibble patterns need more captures.
- **Turbo / sleep** presets not mapped to PD-Pioneer frames.
- **Follow-me** and **special** Midea-style message types from early component
  skeleton do not apply to this protocol.

## References

| Resource | Location |
|----------|----------|
| Decode tooling | `/path/to/pronto_decoder` |
| Field-change notes | `pronto_decoder/notes on decoding.txt` |
| ESPIR1 captures | `espir1_device/captured_data.txt` |
| Mode / fan / temp logs | `pronto_decoder/test_log_files/` |
