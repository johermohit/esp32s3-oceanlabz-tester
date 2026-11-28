# TFT screen test (ESP32-S3 CAM DevKit wired straight to the 1.8" module)

Purpose: prove the screen itself works, with no expansion board in the way. Nothing else is tested here.

The module is a **QDTech 1.8" 128x160 with a Samsung S6D02A1 controller** (pins printed on the module: 1 RST, 2 CS, 3 D/C, 4 DIN, 5 CLK, 6 VCC, 7 BL, 8 GND).
The driver is a port of [zigwart/Adafruit_QDTech](https://github.com/zigwart/Adafruit_QDTech) (MIT, based on Adafruit's ST7735 library): same init sequence, ESP32 SPI instead of AVR registers. It also lets you try the plain Adafruit ST7735 init for comparison.

## FIRST: check solder jumper J1 on the module's back
The module has a small regulator and a solder jumper **J1** (printed box: `VCC=5V + J1 OPEN`, `VCC=3.3V + J1 SHORT`).
- **J1 open** (two separate pads, the usual state): feed VCC with **5 V**. With only 3.3 V the regulator starves the screen: backlight on, nothing on the display.
- **J1 shorted** (solder joins the pads): feed VCC with **3.3 V** and never 5 V.
Signal pins stay 3.3 V logic either way. Check J1 with a multimeter in continuity mode if you are not sure.

## Wiring: 8 female-to-female jumpers, USB unplugged

![wiring](wiring.png)

| Module pin | Wire to ESP32-S3 CAM DevKit |
|---|---|
| 1 RST | GPIO1 (right header, J3) |
| 2 CS | GPIO2 (J3) |
| 3 D/C | GPIO42 (J3) |
| 4 DIN | GPIO41 (J3) |
| 5 CLK | GPIO40 (J3) |
| 6 VCC | **5V** (left header, J1) if module jumper J1 is open; 3V3 if shorted |
| 7 BL | GPIO39 (J3). A 3V3 pin also works. |
| 8 GND | G (last pin of the right header, J3) |

The six GPIO wires go to six neighbouring pins on the right header (GPIO1, 2, 42, 41, 40, 39, just after TX and RX). The CAM DevKit's camera uses GPIO4 to 13 and 15 to 18, so none of those are used and the camera can stay plugged in. Match by the names printed on the DevKit.

## Flash
```bash
cd screen_test
pio run -t upload
pio device monitor -b 115200
```
Use the USB-C port that shows up as the UART bridge (`usbmodem...`, VID 1A86). No pin here uses GPIO43/44, so the serial log stays clean.

## What you should see
The screen fills red, green, blue, white, black (1.2 s each), then draws bars and the words **SCREEN OK**. The serial log prints each step.

Keys in the monitor: `1` QDTech init (default), `2` `3` `4` plain ST7735 init (black, green, red tab), `k` clock demo, `i` invert colours, `h` help.

## If the screen shows coloured noise ("snow")
That means the module is powered and the controller is alive, but our commands and pixels are not arriving. In the monitor:
1. Press `c` (command ping). It sends only DISPLAY OFF / ON, four times, 1.8 s apart. Watch the screen.
   - It goes blank and back on cue: CLK, DIN, CS and D/C are fine. The problem is the init or the pixel data. Try `1`, then `2` `3` `4`.
   - Nothing changes: one of CLK (5), DIN (4), CS (2) or D/C (3) is loose or on the wrong pin. Reseat each jumper, check the pin names on the DevKit, and check that the pin on the module is really touching.
2. Press `B` (bit-bang mode, no SPI peripheral), then `1`. If that works but normal mode does not, it is an SPI pin problem on the ESP32 side.

## Reading the result
| What happens | Meaning |
|---|---|
| Colours and SCREEN OK appear | The screen and wiring are fine. Only the expansion-board wiring remains. |
| Works with `1` but not `2` to `4` | It is the S6D02A1 controller, so use the QDTech driver. |
| Works with `2` but not `1` | It is an ST7735, so use the Adafruit ST7735 driver. |
| Colours wrong (red shows blue) | Controller is right, only colour order differs. Tell us. |
| Backlight only, nothing else, on `1` to `4` | Check VCC first (J1 rule above), then wires 1 to 5 one by one. |
| Nothing lights at all | Check VCC, BL and GND wires, and the 3V3 pins. |
