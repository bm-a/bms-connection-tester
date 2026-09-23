# JBD / Xiaoxiang BMS protocol (as implemented here)

![JBD frame map](https://github.com/bm-a/bms-connection-tester/releases/download/v2.7/protocol.png)

UART **9600 8N1**, half-duplex over RS485.

## Request (host → BMS, 7 bytes, fixed for a basic-info read)

```
DD A5 03 00 FF FD 77
│  │  │  │  └────┴── checksum 0xFFFD = 0x10000 − (0x03 + 0x00)
│  │  │  └──────────── length 0 (no payload on a read)
│  │  └─────────────── register 0x03 = basic info
│  └────────────────── read marker (0x5A = write)
└───────────────────── start byte (0x77 = end byte)
```

## Response (BMS → host)

```
DD REG LEN_HI LEN_LO DATA… CK_HI CK_LO 77
```

- **Checksum rule (verified against 6 real captures):**
  `checksum = 0x10000 − sum(covered bytes)`, big-endian.
  - Requests cover `REG + LEN + DATA` (the `A5`/`5A` marker byte excluded).
  - Responses cover `LEN_HI + LEN_LO + DATA` — the echoed command byte excluded
    (including it misses by exactly `0x03`; an early spec note had this wrong).
- **Registers answered:** `0x03` basic info (voltage/current/capacity/SOC/
  temperature/cycles), `0x04` cell voltages, `0x05` device name.
- **Writes (`0x5A`) and unknown registers:** answered with silence, but still
  counted as live traffic for the link indicator (a tester must never confuse
  a host with a wrong-register reply).

## Canned frames shipped in firmware

| Register | Bytes | Content | Checksum |
|---|---|---|---|
| `0x03` | 34 | Byte-exact real capture: 52.0 V, 0 A, 100 Ah/100 Ah, 100 %, 14S, 2 × 25.0 °C | `FC DA` |
| `0x04` | 35 | 14 × 3714 mV (= 52.0 V, consistent with `0x03`) | `F8 04` |
| `0x05` | 19 | ASCII `TEST-14S100A` | `FC FD` |

All three are frozen literals (never recomputed at runtime) and verified
byte-present inside the shipped `firmware.bin`.

## Out of scope (seen on real buses, deliberately not implemented)

- Modbus RTU frames occasionally share the wire — different protocol, ignored.
- Charge/discharge captures whose checksums match no formula are treated as
  transcription errors and excluded from the test vectors (documented in code).

## v2.6 note: no protocol change

Daily meter counting adds no wire traffic and no new register: the JBD
protocol carries no meter ID, so "retry same meter" vs "next meter" is
unknowable from protocol data alone. The count is a software estimate from
link gaps (RED ≥ 3 s closed by GREEN = reseat = new meter), never a
per-unit identification. Link state (GREEN/RED) and relay actuations are
unchanged.
