# Protocol — JBD UART over RS485 (9600 8N1, half-duplex)

## Frames
- Request: `DD A5 REG LEN DATA CK_HI CK_LO 77` (`A5` = read, `5A` = write).
  Reads are 7 bytes, e.g. `DD A5 03 00 FF FD 77`.
- Response: `DD REG LEN_HI LEN_LO DATA CK_HI CK_LO 77`.

## Checksum (verified on 6 vectors, do not re-derive)
`0x10000 − sum`, big-endian.
- Requests cover `REG+LEN+DATA` (`A5`/`5A` excluded).
- Responses cover `LEN_HI+LEN_LO+DATA` (echoed command byte excluded —
  including it gives FCD7 vs the recorded FCDA, off by exactly `0x03`).
- Vectors: `FFFD` (request), `FCDA` (100 %), `FCA8` (50 %), `FA86` (90 %/45 °C),
  `F65A` (2nd Docklight `0x2A` variant), `F658` (49 % frame).
- Ground truth: `captures/SOC-DOCKLIGHT.xlsx`. Modbus RTU seen on the bus
  (`01 03 …`) is out of scope and ignored.

## Canned replies (frozen literals, never recomputed)
| Reg | Bytes | Content | CK |
|---|---|---|---|
| `0x03` | 34 | byte-exact capture: 52.0 V, 0 A, 100/100 Ah, 100 %, 14S, 2×25.0 °C | `FC DA` |
| `0x04` | 35 | 14 × `0E 82` (3714 mV) = 52.0 V, consistent with `0x03` | `F8 04` |
| `0x05` | 19 | ASCII `TEST-14S100A` | `FC FD` |

Writes and unknown registers get silence (option A, user-confirmed) but still
refresh the green window. Parser: streaming FSM (`JST_*`), validates
start/cmd/len/data/CK/`0x77`, re-syncs on noise, rejects `>64`-byte payloads,
emits only on CK success. Tracker: EMA of poll intervals,
`threshold = clamp(2×EMA+500, 2 s, 10 s)`. Spec detail: `docs/PROTOCOL.md`.
