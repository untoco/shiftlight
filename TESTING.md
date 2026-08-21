# First hardware test

## Toolchain

The firmware is standard Arduino C++ for ESP32-S3. We use PlatformIO Core only
as a portable command-line compiler/uploader; no graphical IDE is required.

```bash
cd /path/to/shiftlight
python3 -m venv .tooling/platformio
.tooling/platformio/bin/python -m pip install platformio
.tooling/platformio/bin/pio run --target upload --upload-port /dev/cu.usbmodem101
.tooling/platformio/bin/pio device monitor --port /dev/cu.usbmodem101 --baud 115200
```

On Windows, use `.tooling\\platformio\\Scripts\\` in place of
`.tooling/platformio/bin/`. The exact serial device name is intentionally
passed to each command: it can change when the USB cable or port changes.

## Wiring for this test

1. Keep the Atomic CAN Base fitted to the bottom of AtomS3R. The test does not
   initialise or transmit on CAN.
2. Connect the AtomS3R HY2.0-4P Grove port to the `IN` port of one Chain RGB.
   The Chain RGB `OUT` port remains empty for this test.
3. Use the supplied Grove cable. It carries `GND`, `5V`, `GPIO2 TX`, and
   `GPIO1 RX`; do not swap its ends through an unlabelled adapter.

## Expected result

- AtomS3R shows `CHAIN RGB READY`, then alternates `CHAIN RGB ON` and
  `CHAIN RGB OFF`.
- The entire 8x8 Chain RGB matrix is green at 25% brightness for 750 ms, then
  fully off for 750 ms.
- The serial monitor reports the discovered Chain device and each on/off phase.

If it displays `NO CHAIN RGB`, confirm that the cable goes into the matrix
`IN` port, then power-cycle the AtomS3R. Do not connect the BMW PT-CAN until
this standalone light test passes.
