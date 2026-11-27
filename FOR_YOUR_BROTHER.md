# Test steps (about 10 minutes)
1. Plug the board into a laptop with a data USB-C cable. Use the port on the ESP32 module marked UART/COM (if unsure, try the other one).
2. Open the link you were sent in **Chrome or Edge** on a laptop (not a phone).
3. Click **Flash test firmware**, choose the port, wait until it says finished. If it can't connect: hold BOOT, tap RESET, release BOOT, try again.
4. Click **Connect serial**, choose the port. Press each button in order:
   - **I2C scan** (wait about 1 minute)
   - **Mic meter** (clap or talk during the 5 seconds)
   - **Speaker tone** (keep the speaker away from your ear)
   - **TFT colors** (tell us what colors you actually see)
5. Click **Copy log** and paste it into a message. Also tell us: did the OLED light up, did you hear the tone, what did the TFT show. A photo of the board helps.
