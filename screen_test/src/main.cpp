// TFT screen test: ESP32-S3 DevKit wired DIRECTLY to the 1.8" QDTech 128x160 module. No expansion board.
// Pins (camera-safe: the CAM DevKit's camera uses GPIO4-13 and 15-18, so none of those are used here).
// All on the RIGHT header (J3), neighbours: GPIO1, 2, 42, 41, 40, 39.
//   RST=1  CS=2  D/C=42  DIN(MOSI)=41  CLK(SCK)=40  BL=39   VCC=5V (if module jumper J1 open)   GND=GND
// Serial (115200, UART port):  1 = QDTech S6D02A1 init (default, from Adafruit_QDTech)
//                              2 = Adafruit ST7735 BLACKTAB   3 = ST7735 GREENTAB   4 = ST7735 REDTAB
//                              c = COMMAND PING (display off/on x4: proves CLK/DIN/CS/DC wiring)   B = toggle bit-bang mode
//                              w = WIRE WALK: sets each signal pin HIGH for 4 s, one at a time (check with a multimeter or LED)
//                              l = toggle SPI speed 4 MHz / 400 kHz   m = toggle SPI mode 0 / 3 (QDTech driver)
//                              k = clock demo (adapted from Adafruit_QDTech TFT_Clock)   i = invert colours   h = help
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "QDTechESP.h"

#define PIN_RST  1
#define PIN_CS   2
#define PIN_DC  42
#define PIN_MOSI 41
#define PIN_SCK  40
#define PIN_BL   39     // driven HIGH. If BL is wired to 3V3 instead, this pin is simply left unconnected.

static SPIClass spi(FSPI);
static QDTechESP qd(spi, PIN_CS, PIN_DC, PIN_RST);
static Adafruit_ST7735 st(&spi, PIN_CS, PIN_DC, PIN_RST);
static Adafruit_GFX* g = nullptr;
static const char* gname = "";
static bool invert = false, clockOn = false;
static uint32_t g_hz = 4000000;   // l = toggle 4 MHz / 400 kHz

static void bars(Adafruit_GFX& d, const char* title) {
  const uint16_t C[] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000};
  const char* N[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};
  for (int i = 0; i < 5; i++) {
    Serial.printf("  fill %s\n", N[i]);
    d.fillScreen(C[i]); delay(1200);
  }
  d.fillScreen(0x0000);
  d.drawRect(0, 0, d.width(), d.height(), 0xFFFF);
  int w = d.width() / 4;
  for (int i = 0; i < 4; i++) d.fillRect(i * w, 20, w, 30, (uint16_t[]){0xF800, 0x07E0, 0x001F, 0xFFE0}[i]);
  d.setTextColor(0xFFFF); d.setTextSize(1); d.setCursor(4, 4); d.print(title);
  d.setTextSize(2); d.setCursor(4, 60); d.print("SCREEN OK");
  d.setTextSize(1); d.setCursor(4, 90); d.print("R G B Y bars above");
  d.setCursor(4, 104); d.printf("%dx%d", d.width(), d.height());
}

static void run(int mode) {
  clockOn = false;
  Serial.printf("\n== mode %d ==\n", mode);
  if (mode == 1) { qd.begin(g_hz); g = &qd; gname = "QDTech S6D02A1"; }
  else {
    uint8_t tab = mode == 2 ? INITR_BLACKTAB : mode == 3 ? INITR_GREENTAB : INITR_REDTAB;
    st.setSPISpeed(g_hz); st.initR(tab); st.setRotation(0); g = &st;
    gname = mode == 2 ? "ST7735 BLACKTAB" : mode == 3 ? "ST7735 GREENTAB" : "ST7735 REDTAB";
  }
  Serial.printf("driver: %s. Watch the screen.\n", gname);
  bars(*g, gname);
  Serial.println("done. Screen shows colour fills then bars + 'SCREEN OK'? Press 1-4 to try another init, k = clock.");
}

// ---- clock (adapted from Adafruit_QDTech examples/TFT_Clock) ----
static float sx = 0, sy = 1, mx = 1, my = 0, hx = -1, hy = 0, sdeg, mdeg, hdeg;
static uint16_t osx = 64, osy = 64, omx = 64, omy = 64, ohx = 64, ohy = 64;
static uint32_t targetTime = 0; static uint8_t hh = 10, mm = 8, ss = 0;
static void clockFace() {
  g->setRotation(0); g->fillScreen(0x0000); g->setTextColor(0xFFFF, 0x0000); g->setTextSize(1);
  g->fillCircle(64, 64, 61, 0x001F); g->fillCircle(64, 64, 57, 0x0000);
  for (int i = 0; i < 360; i += 30) {
    float cx = cos((i - 90) * 0.0174532925), cy = sin((i - 90) * 0.0174532925);
    g->drawLine(cx * 57 + 64, cy * 57 + 64, cx * 50 + 64, cy * 50 + 64, 0x001F);
  }
  g->fillCircle(65, 65, 3, 0xF800); g->setCursor(34, 151); g->print("CLOCK TEST");
  osx = omx = ohx = osy = omy = ohy = 64; clockOn = true; targetTime = 0;
}
static void clockTick() {
  if (targetTime > millis()) return; targetTime = millis() + 1000;
  if (++ss == 60) { ss = 0; if (++mm > 59) { mm = 0; if (++hh > 23) hh = 0; } }
  sdeg = ss * 6; mdeg = mm * 6 + sdeg * 0.01666667; hdeg = hh * 30 + mdeg * 0.0833333;
  hx = cos((hdeg - 90) * 0.0174532925); hy = sin((hdeg - 90) * 0.0174532925);
  mx = cos((mdeg - 90) * 0.0174532925); my = sin((mdeg - 90) * 0.0174532925);
  sx = cos((sdeg - 90) * 0.0174532925); sy = sin((sdeg - 90) * 0.0174532925);
  g->drawLine(ohx, ohy, 65, 65, 0); g->drawLine(omx, omy, 65, 65, 0); g->drawLine(osx, osy, 65, 65, 0);
  g->drawLine(hx * 33 + 65, hy * 33 + 65, 65, 65, 0xFFFF); g->drawLine(mx * 44 + 65, my * 44 + 65, 65, 65, 0xFFFF);
  g->drawLine(sx * 47 + 65, sy * 47 + 65, 65, 65, 0xF800); g->fillCircle(65, 65, 3, 0xF800);
  osx = sx * 47 + 65; osy = sy * 47 + 65; omx = mx * 44 + 65; omy = my * 44 + 65; ohx = hx * 33 + 65; ohy = hy * 33 + 65;
  g->setCursor(34, 140); g->printf("%02d:%02d:%02d", hh, mm, ss);
}

// c: wiring ping. Sends only DISPLAY OFF / DISPLAY ON. If the picture changes on cue, CLK, DIN, CS and D/C are all fine.
static void ping() {
  Serial.println("\n== command ping ==");
  qd.begin(g_hz);
  for (int i = 0; i < 4; i++) {
    qd.rawCmd(0x28); Serial.println("  DISPLAY OFF sent: screen should go BLANK/white"); delay(1800);
    qd.rawCmd(0x29); Serial.println("  DISPLAY ON sent: noise/picture should come back"); delay(1800);
  }
  g = &qd; gname = "QDTech S6D02A1";
  Serial.println("ping done. Did the screen change on cue? YES = command wires OK. NO = a wire to CLK/DIN/CS/D-C is loose or wrong.");
}

// w: wire walk. Raises one signal pin at a time. Put a multimeter (DC volts) between the MODULE pin and GND:
// you must read ~3.3 V on exactly the module pin named in the log, and ~0 V on the others.
static void walk() {
  struct W { int pin; const char* name; };
  const W L[] = {{PIN_RST, "RST  = module pin 1"}, {PIN_CS, "CS   = module pin 2"}, {PIN_DC, "D/C  = module pin 3"},
                 {PIN_MOSI, "DIN  = module pin 4"}, {PIN_SCK, "CLK  = module pin 5"}, {PIN_BL, "BL   = module pin 7"}};
  qd.setBitBang(true);                       // releases the SPI peripheral so the pins are plain GPIO
  for (auto& w : L) { pinMode(w.pin, OUTPUT); digitalWrite(w.pin, LOW); }
  Serial.println("\n== wire walk: one pin HIGH at a time, 4 s each ==");
  for (auto& w : L) {
    digitalWrite(w.pin, HIGH);
    Serial.printf("  GPIO%-2d HIGH now -> %s : expect ~3.3 V on that module pin only\n", w.pin, w.name);
    delay(4000);
    digitalWrite(w.pin, LOW);
  }
  digitalWrite(PIN_BL, HIGH);
  qd.setBitBang(false);
  Serial.println("walk done (BL left high).");
}

static void help() { Serial.println("keys: 1=QDTech init  2/3/4=ST7735 black/green/red tab  c=command ping  w=wire walk  l=slow SPI  m=SPI mode  B=bit-bang on/off  k=clock  i=invert  h=help"); }

void setup() {
  Serial.begin(115200); delay(1500);
  Serial.println("\nTFT screen test (ESP32-S3 DevKit -> QDTech 1.8\" module)");
  Serial.printf("pins: RST=%d CS=%d DC=%d DIN=%d CLK=%d BL=%d\n", PIN_RST, PIN_CS, PIN_DC, PIN_MOSI, PIN_SCK, PIN_BL);
  if (PIN_BL >= 0) { pinMode(PIN_BL, OUTPUT); digitalWrite(PIN_BL, HIGH); }
  spi.begin(PIN_SCK, -1, PIN_MOSI, -1);
  qd.setPins(PIN_SCK, PIN_MOSI);
  help(); run(1);
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c >= '1' && c <= '4') run(c - '0');
    else if (c == 'c') ping();
    else if (c == 'w') walk();
    else if (c == 'l') { g_hz = (g_hz == 4000000) ? 400000 : 4000000; Serial.printf("SPI speed = %u Hz. Press 1 to test.\n", (unsigned)g_hz); }
    else if (c == 'm') { qd.setMode(qd.mode() == SPI_MODE0 ? SPI_MODE3 : SPI_MODE0); Serial.printf("SPI mode = %d. Press 1 to test.\n", qd.mode() == SPI_MODE3 ? 3 : 0); }
    else if (c == 'B') { qd.setBitBang(!qd.bitBang()); Serial.printf("bit-bang %s (QDTech driver only). Press 1 to test.\n", qd.bitBang() ? "ON" : "OFF"); }
    else if (c == 'k' && g) clockFace();
    else if (c == 'i' && g) { invert = !invert; if (g == &qd) qd.invertDisplay(invert); else st.invertDisplay(invert); Serial.printf("invert=%d\n", invert); }
    else if (c == 'h') help();
  }
  if (clockOn) clockTick();
}
