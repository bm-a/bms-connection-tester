# captures/ — original recordings (ground truth)

![Meter segment layout (what the captures light up)](https://raw.githubusercontent.com/bm-a/bms-connection-tester/main/Actual%20Meter%20Image/1411bb7a-d99e-4885-8ea0-12714b984a5c.jpeg)

![JBD frame map](https://github.com/bm-a/bms-connection-tester/releases/download/v2.7/protocol.png)

- `SOC-DOCKLIGHT.xlsx` — raw Docklight captures from the real battery + real meter:
  the fixed `DD A5 03 00 FF FD 77` request, SOC 100 % / 50 % replies, a 90 %/45 °C
  long frame, two 0x2A variants, charge/discharge frames, and a Modbus RTU frame
  also seen on the bus (`01 03 00 00 00 1D 85 C3`, out of scope).

Used as regression vectors in `test/test_checksum/`. Frames whose checksums match
no formula (charge/discharge captures) are documented and excluded, not used.
