// OceanLabz ESP32-S3 AI expansion POC: discover + exercise OLED(I2C), INMP441 mic(I2S), MAX98357A amp(I2S), ST7735 TFT(SPI)
// Pin defaults come from OceanLabz's XiaoZhi wiring page and are UNVERIFIED for the expansion board.
// Serial commands: s=I2C scan all pin pairs, m=mic level meter, t=speaker tone, d=TFT color test, h=help
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <driver/i2s.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ---- adjust after you confirm with the scan / schematic ----
#define MIC_SCK 2
#define MIC_WS  1
#define MIC_SD  42
#define AMP_BCLK 40
#define AMP_LRC  41
#define AMP_DIN  39
#define TFT_SCK 19
#define TFT_MOSI 20
#define TFT_DC 47
#define TFT_CS 45
#define TFT_RST 21
#define TFT_BL 38

// GPIOs safe to probe on an N16R8 module (26-37 = flash/PSRAM, 0/3/45/46 = strapping, 19/20 = USB)
static const int CAND[] = {1,2,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,21,38,39,40,41,42,47,48};

void i2cScan() {
  Serial.println("I2C scan over all candidate SDA/SCL pairs (ignore camera-bus hits)...");
  int n = sizeof(CAND)/sizeof(CAND[0]);
  for (int a = 0; a < n; a++) for (int b = 0; b < n; b++) {
    if (a == b) continue;
    int sda = CAND[a], scl = CAND[b];
    Wire.end();
    if (!Wire.begin(sda, scl, 100000)) continue;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0)
        Serial.printf("  FOUND 0x%02X  SDA=%d SCL=%d%s\n", addr, sda, scl,
                      (addr == 0x3C || addr == 0x3D) ? "  <- SSD1306 OLED?" : "");
    }
  }
  Wire.end();
  Serial.println("scan done");
}

void i2sInstall(i2s_port_t p, i2s_mode_t mode, int bclk, int ws, int dout, int din, int rate) {
  i2s_config_t c = {};
  c.mode = mode; c.sample_rate = rate;
  c.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  c.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;   // INMP441 L/R tied to GND => left
  c.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  c.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  c.dma_buf_count = 8; c.dma_buf_len = 256;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = bclk; pins.ws_io_num = ws; pins.data_out_num = dout; pins.data_in_num = din;
  i2s_driver_install(p, &c, 0, NULL);
  i2s_set_pin(p, &pins);
}

void micTest() {
  Serial.println("Mic meter 5s - clap/talk. Flat 0 => wrong pins/L-R/orientation.");
  i2sInstall(I2S_NUM_0, (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), MIC_SCK, MIC_WS, I2S_PIN_NO_CHANGE, MIC_SD, 16000);
  int32_t buf[256];
  uint32_t end = millis() + 5000;
  while (millis() < end) {
    size_t got = 0;
    i2s_read(I2S_NUM_0, buf, sizeof(buf), &got, 100);
    int n = got / 4; if (!n) continue;
    double acc = 0; for (int i = 0; i < n; i++) { double s = (buf[i] >> 8); acc += s * s; }
    int rms = (int)sqrt(acc / n);
    Serial.printf("rms=%7d ", rms);
    for (int i = 0; i < min(rms / 2000, 60); i++) Serial.print('#');
    Serial.println();
    delay(100);
  }
  i2s_driver_uninstall(I2S_NUM_0);
}

void toneTest() {
  Serial.println("Playing 440Hz for 2s on MAX98357A (keep volume low).");
  i2sInstall(I2S_NUM_1, (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), AMP_BCLK, AMP_LRC, AMP_DIN, I2S_PIN_NO_CHANGE, 16000);
  static int32_t b[160];
  for (int rep = 0; rep < 2 * 100; rep++) {   // 160 samples = 10ms
    for (int i = 0; i < 160; i++) b[i] = (int32_t)(sinf(2 * PI * 440 * (rep * 160 + i) / 16000.0f) * 3e7);
    size_t w; i2s_write(I2S_NUM_1, b, sizeof(b), &w, portMAX_DELAY);
  }
  i2s_driver_uninstall(I2S_NUM_1);
}

void tftTest() {
  Serial.println("TFT color cycle (ST7735 128x160).");
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  static Adafruit_ST7735 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);
  tft.initR(INITR_BLACKTAB);
  uint16_t cols[] = {ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE, ST77XX_WHITE};
  for (auto c : cols) { tft.fillScreen(c); delay(500); }
  tft.fillScreen(ST77XX_BLACK); tft.setCursor(4, 4); tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2); tft.println("POC OK");
}

void help() { Serial.println("cmds: s=I2C scan, m=mic, t=tone, d=display, h=help"); }

void setup() { Serial.begin(115200); delay(1500); Serial.println("OceanLabz S3 POC"); help(); }

void loop() {
  if (!Serial.available()) return;
  switch (Serial.read()) {
    case 's': i2cScan(); break;
    case 'm': micTest(); break;
    case 't': toneTest(); break;
    case 'd': tftTest(); break;
    case 'h': help(); break;
  }
}
