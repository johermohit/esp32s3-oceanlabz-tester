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

## Pin map (OceanLabz AI ESP32-S3 Expansion Board v1.2)
Source: OceanLabz's own guide, https://www.oceanlabz.in/esp32-s3-ai-expansion/ (not their AI Explorer S3 GitHub repo, which is a different board).

| Part | Pins |
|---|---|
| TFT (ST7735 128x160, 3.3V) | BL 42, DIN/MOSI 43, CLK 44, D/C 38, CS 45, RST 1 |
| INMP441 mic | WS 39, SCK 40, SD 41 |
| MAX98357A amp | LRC 21, BCLK 48, DIN 47 |

**Serial console:** TFT DIN/CLK use GPIO43/44, which are also UART0. This firmware therefore builds with USB CDC on boot, so open the console on the board's *native* USB-C port. If nothing shows, try the other USB-C port. The TFT test will not work while a UART console owns 43/44.

If the screen stays blank, use **Try presets**, or type `T bl clk mosi dc cs rst` (for example `T42 44 43 38 45 1`) then **TFT colors**. A TFT module plugged one or two header positions off will also look blank: check the seating.

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
