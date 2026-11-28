// FINAL screen test: OceanLabz AI ESP32-S3 Expansion Board v1.2 + its 1.8" 128x160 TFT, screen plugged into the board.
// Pins from OceanLabz's own guide (oceanlabz.in/esp32-s3-ai-expansion). No serial, no typing: just watch the screen.
// Slow software SPI (plain pin toggling) so wiring and speed cannot be the problem.
// It loops 4 drivers. Each shows RED, GREEN, BLUE, then a big letter:
//   A = QDTech / S6D02A1 (init from github.com/zigwart/Adafruit_QDTech)   B/C/D = ST7735 black/green/red tab
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_RST   1
#define TFT_CS   45
#define TFT_DC   38
#define TFT_MOSI 43
#define TFT_SCLK 44
#define TFT_BL   42

// ---------- S6D02A1 (QDTech) driver, bit-banged ----------
static void bb(uint8_t b) { for (int i = 7; i >= 0; i--) { digitalWrite(TFT_MOSI, (b >> i) & 1); digitalWrite(TFT_SCLK, HIGH); digitalWrite(TFT_SCLK, LOW); } }
static void wc(uint8_t c) { digitalWrite(TFT_DC, LOW);  digitalWrite(TFT_CS, LOW); bb(c); digitalWrite(TFT_CS, HIGH); }
static void wd(uint8_t d) { digitalWrite(TFT_DC, HIGH); digitalWrite(TFT_CS, LOW); bb(d); digitalWrite(TFT_CS, HIGH); }

static const uint8_t QD_INIT[] = {
    29,
    0xf0, 2, 0x5a, 0x5a, 0xfc, 2, 0x5a, 0x5a, 0x26, 1, 0x01,
    0xfa, 15, 0x02, 0x1f, 0x00, 0x10, 0x22, 0x30, 0x38, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3d, 0x02, 0x01,
    0xfb, 15, 0x21, 0x00, 0x02, 0x04, 0x07, 0x0a, 0x0b, 0x0c, 0x0c, 0x16, 0x1e, 0x30, 0x3f, 0x01, 0x02,
    0xfd, 11, 0x00, 0x00, 0x00, 0x17, 0x10, 0x00, 0x01, 0x01, 0x00, 0x1f, 0x1f,
    0xf4, 15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0x07, 0x00, 0x3C, 0x36, 0x00, 0x3C, 0x36, 0x00,
    0xf5, 13, 0x00, 0x70, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6d, 0x66, 0x06,
    0xf6, 11, 0x02, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x02, 0x00, 0x06, 0x01, 0x00,
    0xf2, 17, 0x00, 0x01, 0x03, 0x08, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x04, 0x08, 0x08,
    0xf8, 1, 0x11, 0xf7, 4, 0xc8, 0x20, 0x00, 0x00, 0xf3, 2, 0x00, 0x00,
    0x11, 0x80, 50,
    0xf3, 2 + 0x80, 0x00, 0x01, 50, 0xf3, 2 + 0x80, 0x00, 0x03, 50, 0xf3, 2 + 0x80, 0x00, 0x07, 50, 0xf3, 2 + 0x80, 0x00, 0x0f, 50,
    0xf4, 15 + 0x80, 0x00, 0x04, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0x07, 0x00, 0x3C, 0x36, 0x00, 0x3C, 0x36, 0x00, 50,
    0xf3, 2 + 0x80, 0x00, 0x1f, 50, 0xf3, 2 + 0x80, 0x00, 0x7f, 50, 0xf3, 2 + 0x80, 0x00, 0xff, 50,
    0xfd, 11, 0x00, 0x00, 0x00, 0x17, 0x10, 0x00, 0x00, 0x01, 0x00, 0x16, 0x16,
    0xf4, 15, 0x00, 0x09, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0x07, 0x00, 0x3C, 0x36, 0x00, 0x3C, 0x36, 0x00,
    0x36, 1, 0x08, 0x35, 1, 0x00, 0x3a, 1 + 0x80, 0x05, 150, 0x29, 0, 0x2c, 0};

class QD : public Adafruit_GFX {
 public:
  QD() : Adafruit_GFX(128, 160) {}
  void begin() {
    for (int p : {TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST}) pinMode(p, OUTPUT);
    digitalWrite(TFT_CS, HIGH); digitalWrite(TFT_SCLK, LOW);
    digitalWrite(TFT_RST, HIGH); delay(100); digitalWrite(TFT_RST, LOW); delay(100); digitalWrite(TFT_RST, HIGH); delay(200);
    const uint8_t* a = QD_INIT; uint8_t n = *a++;
    while (n--) { wc(*a++); uint8_t na = *a++; uint8_t ms = na & 0x80; na &= 0x7F; while (na--) wd(*a++); if (ms) { uint16_t d = *a++; delay(d == 255 ? 500 : d); } }
    wc(0x36); wd(0xC8);                                   // rotation 0, as in the original library
  }
  void win(int x0, int y0, int x1, int y1) { wc(0x2A); wd(0); wd(x0); wd(0); wd(x1); wc(0x2B); wd(0); wd(y0); wd(0); wd(y1); wc(0x2C); }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override { if (x < 0 || y < 0 || x >= 128 || y >= 160) return; win(x, y, x, y); wd(c >> 8); wd(c); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override {
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
    if (x + w > 128) w = 128 - x; if (y + h > 160) h = 160 - y; if (w <= 0 || h <= 0) return;
    win(x, y, x + w - 1, y + h - 1);
    digitalWrite(TFT_DC, HIGH); digitalWrite(TFT_CS, LOW);
    for (int32_t i = (int32_t)w * h; i > 0; i--) { bb(c >> 8); bb(c); }
    digitalWrite(TFT_CS, HIGH);
  }
  void fillScreen(uint16_t c) override { fillRect(0, 0, 128, 160, c); }
};

static QD qd;
static Adafruit_ST7735 st(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);   // software SPI on the same pins

static void show(Adafruit_GFX& d, const char* letter, const char* name) {
  d.fillScreen(ST77XX_RED);   delay(800);
  d.fillScreen(ST77XX_GREEN); delay(800);
  d.fillScreen(ST77XX_BLUE);  delay(800);
  d.fillScreen(ST77XX_BLACK);
  d.setTextColor(ST77XX_WHITE); d.setTextSize(8); d.setCursor(40, 30); d.print(letter);
  d.setTextSize(1); d.setCursor(10, 120); d.print(name);
  delay(3000);
}

void setup() { pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH); }

void loop() {
  qd.begin();                show(qd, "A", "QDTech S6D02A1");
  st.initR(INITR_BLACKTAB);  show(st, "B", "ST7735 BLACKTAB");
  st.initR(INITR_GREENTAB);  show(st, "C", "ST7735 GREENTAB");
  st.initR(INITR_REDTAB);    show(st, "D", "ST7735 REDTAB");
}
