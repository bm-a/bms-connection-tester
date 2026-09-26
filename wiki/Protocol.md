# Protocol — JBD UART over RS485 (9600 8N1, half-duplex)

![JBD frame map](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/docs/img/protocol.png)

*Request + reply byte roles and the checksum coverage rule — the whole
protocol on one card.*

The tester is the **responder** side: the meter asks, the box answers with
canned golden values (or the active spoof stage's values on `0x03`).

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
| `0x03` | 34 | byte-exact capture: **52.0 V, 0 A**, 100/100 Ah, **100 %**, 14S, 2×25.0 °C | `FC DA` |
| `0x04` | 35 | 14 × `0E 82` (**3714 mV**) = 52.0 V, consistent with `0x03` | `F8 04` |
| `0x05` | 19 | ASCII **`TEST-14S100A`** | `FC FD` |

What the meter shows for `0x03` — VOL/CUR/TEMP + status flags + battery bar:

![Meter segment layout](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/Actual%20Meter%20Image/1411bb7a-d99e-4885-8ea0-12714b984a5c.jpeg)

*STBY/CHG/DISCH/ERROR flags, VOL/CUR/TEMP digits, battery bar. Golden reply
lights it as 52.0 V, 100 %, full bar; spoof stage 2 drives 88.8/188.*

Writes and unknown registers get **silence** (option A, user-confirmed) but
still refresh the green window. Parser: streaming FSM (`JST_*`), validates
start/cmd/len/data/CK/`0x77`, re-syncs on noise, rejects `>64`-byte payloads,
emits only on CK success (proven: 10 M-byte fuzz, zero bad emits).
Tracker: EMA of poll intervals, `threshold = clamp(2×EMA+500, 2 s, 10 s)` —
self-adjusts to any meter's poll speed.

## Spoofing on `0x03`
While a spoof plan fires, `0x03` replies carry the stage's V/A/°C/SOC instead
of golden (stage 1 defaults 100.0/100.0/100.0/100 %; stage 2 defaults
88.8/88.8/88.8/188 %). `0x04` and `0x05` never change. After the plan,
replies revert to golden automatically.

## USB `STATUS?`
Send `STATUS?` + newline on the USB serial (115200): replies
`GREEN 2.8` (traffic seen within the window) or `RED 2.8` (bus silent) —
the HIL/automation hook. First token (`GREEN`/`RED`) is stable for parsing.

Spec detail: `docs/PROTOCOL.md`.
