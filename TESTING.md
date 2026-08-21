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

## Wiring for the two-matrix visualisation test

1. Keep the Atomic CAN Base fitted to the bottom of AtomS3R. The test does not
   initialise or transmit on CAN.
2. Connect the AtomS3R HY2.0-4P Grove port to the `IN` port of Chain RGB #1.
   Connect its `OUT` port to the `IN` port of Chain RGB #2. This test requires
   exactly two detected matrices.
3. Use the supplied Grove cable. It carries `GND`, `5V`, `GPIO2 TX`, and
   `GPIO1 RX`; do not swap its ends through an unlabelled adapter.

## Expected result

- AtomS3R displays a large test RPM value cycling from 3000 to 7000 and back
  in 100 RPM steps, plus `SECTIONS n/5` below it.
- A green 2×2 section is lit in central rows 3–4 for every reached threshold:
  4000, 4500, 5000, 5500 and 6000 RPM. The five sections start at global
  horizontal positions 0, 3, 6, 9 and 12, so the gap between them is one pixel.
- The serial monitor reports the two discovered Chain devices and the current
  test RPM/section count.

If it displays `CHAIN RGB ERROR`, confirm that the cable goes into the first
matrix `IN` port and that the first matrix `OUT` reaches the second matrix
`IN`, then power-cycle the AtomS3R. Do not connect the BMW PT-CAN until this
standalone light test passes.
