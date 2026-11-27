# Test steps on a Mac (about 10 minutes)
1. Plug the board into the Mac with a data USB-C cable. Use the port on the ESP32 module marked UART/COM. If nothing shows up later, try the other port.
2. Open Terminal and paste these two lines:
   ```
   git clone https://github.com/johermohit/esp32s3-oceanlabz-tester.git
   cd esp32s3-oceanlabz-tester && bash run_mac.sh
   ```
   (If macOS asks to install developer tools, click Install, wait, then run the lines again.)
3. It builds, flashes, then opens a serial monitor. Type one letter and press Enter:
   - `s` I2C scan (wait about 1 minute)
   - `m` mic meter (clap or talk for 5 seconds)
   - `t` speaker tone (keep the speaker away from your ear)
   - `d` TFT colors (note what colors you actually see)
4. Select all the Terminal text, copy it and send it back. Also tell us: did the OLED light up, did you hear the tone, what did the TFT show. A photo of the board helps.

If flashing fails: hold BOOT, tap RESET, release BOOT, run `bash run_mac.sh` again.
If the board never appears as a port: it may need the CH340 driver (search "CH340 Mac driver").
