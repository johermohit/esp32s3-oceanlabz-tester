# OceanLabz ESP32-S3 AI Expansion Board: hardware POC

Tests the parts on the board: SSD1306 OLED (I2C), INMP441 mic (I2S), MAX98357A amp (I2S), 1.8" ST7735 TFT (SPI).

## What's in here
| Path | Purpose |
|---|---|
| `docs/` | Browser flasher + serial test console (open via `serve.bat` / `serve.sh`) |
| `docs/firmware-merged.bin` | Prebuilt image for ESP32-S3, 16MB flash, flash at 0x0 |
| `firmware_source/` | PlatformIO project (`pio run`). Edit pins at the top of `src/main.cpp` |
| `serve.bat`, `serve.sh` | Start a local web server (needs Python) |

## Quick start (no tools to install except Python)
1. Plug the board in with a **data** USB-C cable, using the module's **UART** port (the one that shows up as CH340/CP210x).
2. Double-click `serve.bat` (Windows) or run `./serve.sh`.
3. Open http://localhost:8000 in **Chrome or Edge**. Web Serial only works there, and only on localhost or https.
4. Click **Flash test firmware**, pick the port, wait for it to finish. If it can't connect: hold BOOT, tap RESET, release BOOT, retry.
5. Under **Test**, click **Connect serial**, pick the port again, then use the buttons.

The flasher page loads esp-web-tools from unpkg, so it needs internet. Offline fallback: `python -m esptool --chip esp32s3 write_flash 0x0 docs/firmware-merged.bin`.

## Test procedure
| Button | Expected | If it fails |
|---|---|---|
| **I2C scan** | A line `FOUND 0x3C SDA=x SCL=y` (OLED). Takes about a minute. | No hits: OLED not seated, or it is not I2C on a free pin. Hits at other addresses on camera pins are the camera's SCCB bus, ignore them. |
| **Mic meter** | `rms` jumps when you clap or talk | Stuck at 0: wrong pins, mic not seated or backwards, L/R pin mismatch. Try swapping the mic WS/SCK defines. |
| **Speaker tone** | 2 s of 440 Hz | Silent: wrong pins, speaker not connected, MAX98357A SD pin held low. Keep the speaker away from your ear. |
| **TFT colors** | Red, green, blue, white, then "POC OK" | Blank: wrong pins or backlight. Wrong colors or offset: change `INITR_BLACKTAB` to `INITR_GREENTAB` or `INITR_REDTAB`. |

## Known uncertainty: the pin map
There is no published schematic for this board. The default pins in `main.cpp` come from OceanLabz's XiaoZhi tutorial wiring, which describes a hand-wired build and is **not confirmed** to match the expansion board's routing. TFT pins 19/20 are also the S3's native USB pins.
Use the I2C scan to find the OLED pins. For the rest, continuity-test from each module header pin to the S3 header with a multimeter, then edit the `#define`s and rebuild.

## Rebuild from source
```
cd firmware_source
pio run
```
On Windows keep the folder path short (under about 100 characters), or PlatformIO fails on long library paths.
To make a new merged image, run esptool `merge_bin` with bootloader at 0x0, partitions at 0x8000, boot_app0 at 0xe000, firmware at 0x10000.

## Hardware notes
- Module: ESP32-S3 N16R8 (16MB flash, 8MB octal PSRAM). GPIO 26-37 are used by flash and PSRAM, so do not use them.
- INMP441 and MAX98357A are I2S, not I2C. The amp needs 5V and drives a 4-8 ohm speaker.
