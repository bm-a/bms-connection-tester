# Ready-to-flash binaries (built 2026-09-18 from this exact source)

## Flash with esptool (any PC, no IDE needed)
1. `pip install esptool`
2. Plug in the S3, find the port (COMx / /dev/ttyACM0), then:

```
esptool.py --chip esp32s3 --port <PORT> --baud 460800 write-flash \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

## Or browser flash (no installs)
Open https://esphome.github.io/esp-web-tools/ style flasher
(e.g. https://www.espthings.io/tools/esp32-flasher/),
load the three files at the addresses above, flash, done.

firmware.bin (276,768 bytes) contains the verified golden SOC100%
reply — checked byte-for-byte after the build.
