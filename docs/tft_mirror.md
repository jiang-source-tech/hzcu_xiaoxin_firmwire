# TFT Mirror

TFT mirror streams the exact RGB565 pixels submitted to the
Waveshare ESP32-S3-Touch-LCD-1.46 panel. A PC viewer applies the same rectangle
updates to a local framebuffer and displays the reconstructed 412x412 image.

## Enable

Run `idf.py menuconfig` and enable:

- `Board Type -> Waveshare ESP32-S3-Touch-LCD-1.46`
- `Display Mirror -> Enable pixel-exact TFT mirror`
- `Display Mirror -> Use USB CDC transport for TFT mirror`
- `Display Mirror -> TFT mirror USB CDC baud hint = 2000000`
- `Display Mirror -> Full-frame checkpoint interval = 60`

Then build and flash the firmware:

```powershell
idf.py build flash monitor
```

The mirror data must use its own USB CDC interface. Do not point the viewer at
the ESP-IDF console port, because the binary mirror packets must not be mixed
with text logs.

## Run

Install the only Python dependency once:

```powershell
python -m pip install pyserial
```

Find the mirror CDC serial port in Device Manager, then run:

```powershell
python scripts\tft_mirror_viewer.py --serial COM7 --baud 2000000
```

Expected startup output:

```text
connected COM7
hello 412x412 RGB565
frame=<number> crc=<8 hex digits> rects=<count> ok
```

The viewer keeps the window open while it reconnects. If the board is unplugged,
reset, or the CDC endpoint disappears, the viewer reopens the same serial port
once per second until packets return.

## Protocol Guarantees

The protocol is a little-endian binary stream with a fixed packet header,
payload CRC32, and these packet types:

- `HELLO`: display width, height, RGB565 color format, and byte-order flags.
- `RECT`: one final panel rectangle plus its RGB565 pixel bytes.
- `FRAME_END`: frame id, rectangle count, and full framebuffer CRC32.
- `FULL_FRAME`: checkpoint pixels used to recover after loss or reconnect.

The viewer validates every packet payload CRC before applying it. On
`FRAME_END`, it computes CRC32 over the reconstructed raw RGB565 framebuffer and
prints the firmware-provided frame CRC. A matching line ends with `ok`; a
mismatch means the host framebuffer no longer matches the firmware framebuffer.

This guarantees the PC reconstructs the same RGB565 pixel values that firmware
submitted to the LCD panel. It does not reproduce the panel's physical
brightness, gamma, backlight, viewing angle, color temperature, or response
time.

## Throughput And Reconnect Limits

A full 412x412 RGB565 frame is 339488 bytes, before packet headers and USB CDC
overhead. Continuous full-frame streaming is slow and can affect UI smoothness,
so rectangle packets are required for normal operation. Full-frame packets are
checkpoints for initial sync, reconnect, and recovery.

The viewer can resynchronize after stray or lost bytes by scanning for the next
valid `TFTM` packet header. If a packet payload CRC fails, the viewer closes and
reopens the serial port so firmware can provide a fresh checkpoint. During heavy
UI updates, USB CDC throughput can still lag behind the physical display; the PC
image may be delayed even when CRCs match.
