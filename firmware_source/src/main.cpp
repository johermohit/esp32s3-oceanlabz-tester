// OceanLabz ESP32-S3 AI expansion POC: discover + exercise OLED(I2C), INMP441 mic(I2S), MAX98357A amp(I2S), ST7735 TFT(SPI)
// Pin defaults follow OceanLabz's official tools/pin_configuration/pins_ai_explorer_s3.h, but their own docs
// disagree on the TFT (see README), so this firmware can HUNT for the real TFT wiring.
// Serial commands: s=I2C scan, m=mic meter, t=speaker tone, d=TFT color test, h=help
//   b = backlight hunt (drives each candidate pin high in turn)
//   p = TFT pin hunt   (tries every CLK/MOSI/DC/RST/CS assignment; working one is printed ON the screen)
//   B<pin> = set backlight pin used by p (B-1 = unknown/always on)
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <driver/i2s.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#include <WiFi.h>
#include <WebServer.h>

// ---- Wi-Fi monitor: join Wi-Fi "OceanLabz-POC" (no password), open http://192.168.4.1 ----
// Works while the TFT owns GPIO43/44 (the UART console pins). Log is mirrored to Serial and to the web page.
static String LOGBUF; static size_t LOGBASE = 0; static SemaphoreHandle_t LM;
static QueueHandle_t CQ; static volatile bool STOP = false;
static WebServer web(80);
static void logWrite(const char* s, size_t n) {
  Serial.write((const uint8_t*)s, n);
  xSemaphoreTake(LM, portMAX_DELAY);
  LOGBUF.concat(s, n);
  if (LOGBUF.length() > 6000) { LOGBASE += 3000; LOGBUF.remove(0, 3000); }
  xSemaphoreGive(LM);
}
static void logf(const char* fmt, ...) { char b[256]; va_list a; va_start(a, fmt); int n = vsnprintf(b, sizeof b, fmt, a); va_end(a); if (n > 0) logWrite(b, n < 255 ? n : 255); }
static void logln(const char* s) { logWrite(s, strlen(s)); logWrite("\n", 1); }
static void logln() { logWrite("\n", 1); }
static void logp(char c) { logWrite(&c, 1); }

// feeds web commands to the same parsers as Serial
class CmdStream : public Stream {
  String b; size_t i = 0;
public:
  void set(const String& s) { b = s; i = 0; }
  int available() override { return b.length() - i; }
  int read() override { return i < b.length() ? b[i++] : -1; }
  int peek() override { return i < b.length() ? b[i] : -1; }
  size_t write(uint8_t) override { return 1; }
  void flush() override {}
};
static CmdStream ss; static Stream* IN = &Serial;

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><meta name=viewport content="width=device-width,initial-scale=1"><title>OceanLabz POC</title>
<style>body{font:15px system-ui;margin:12px;background:#111;color:#eee}button{font:inherit;padding:8px 12px;margin:3px;border-radius:8px;border:1px solid #555;background:#222;color:#eee}
input{font:inherit;padding:8px;width:60%;border-radius:8px;border:1px solid #555;background:#222;color:#eee}pre{background:#000;padding:10px;border-radius:8px;height:55vh;overflow:auto;white-space:pre-wrap}</style>
<h3>OceanLabz POC</h3><div id=b></div><input id=c placeholder="T42 44 43 38 45 1"><button onclick="s(c.value)">Send</button><button onclick="fetch('/stop')">STOP hunt</button>
<pre id=l></pre><script>
for(const x of ['h','q','q1','q2','q3','q4','d','b','B42','P42 43 44 38 45 1','p','m','t','s'])b.insertAdjacentHTML('beforeend','<button onclick="s(\''+x+'\')">'+x+'</button>');
function s(x){fetch('/c?x='+encodeURIComponent(x))}
let n=0;async function poll(){try{const r=await fetch('/l?f='+n);const t=await r.text();n=+r.headers.get('X-Next');if(t){l.textContent+=t;l.scrollTop=l.scrollHeight}}catch(e){}setTimeout(poll,500)}poll()
</script>)HTML";

static void webTask(void*) {
  web.on("/", []() { web.send_P(200, "text/html", PAGE); });
  web.on("/c", []() { char c[48]; String x = web.arg("x"); x.toCharArray(c, sizeof c); xQueueSend(CQ, c, 0); web.send(200, "text/plain", "ok"); });
  web.on("/stop", []() { STOP = true; web.send(200, "text/plain", "ok"); });
  web.on("/l", []() {
    size_t f = web.arg("f").toInt();
    xSemaphoreTake(LM, portMAX_DELAY);
    size_t end = LOGBASE + LOGBUF.length(); if (f < LOGBASE) f = LOGBASE;
    String out = LOGBUF.substring(f - LOGBASE);
    xSemaphoreGive(LM);
    web.sendHeader("X-Next", String(end)); web.sendHeader("Access-Control-Expose-Headers", "X-Next");
    web.send(200, "text/plain", out);
  });
  web.begin();
  for (;;) { web.handleClient(); delay(5); }
}

// ---- adjust after you confirm with the scan / schematic ----
#define MIC_SCK 40
#define MIC_WS  39
#define MIC_SD  41
#define AMP_BCLK 48
#define AMP_LRC  21
#define AMP_DIN  47
// TFT: official header says CLK38 MOSI42 DC1 RST43 CS45 BL44; the OceanLabz PDF says MOSI43 SCLK44 DC38 RST1 CS45 BL42.
// Default = OceanLabz ESP32-S3 AI Expansion Board v1.2 guide (oceanlabz.in/esp32-s3-ai-expansion): BL42 DIN(MOSI)43 CLK44 DC38 CS45 RST1.
// 43/44 are also UART0, so the console must be on the native USB port (CDC on boot). Change at runtime with T / q / P, no reflash.
static int TFT_SCK = 44, TFT_MOSI = 43, TFT_DC = 38, TFT_CS = 45, TFT_RST = 1, TFT_BL = 42;
// candidate pins for the b and p hunts (set with P)
static int HUNT[10] = {42, 43, 44, 38, 45, 1}; static int NHUNT = 6;

// GPIOs safe to probe on an N16R8 module (26-37 = flash/PSRAM, 0/3/45/46 = strapping, 19/20 = USB)
static const int CAND[] = {1,2,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,21,38,39,40,41,42,43,44,47,48};

void i2cScan() {
  logln("I2C scan over all candidate SDA/SCL pairs (ignore camera-bus hits)...");
  int n = sizeof(CAND)/sizeof(CAND[0]);
  for (int a = 0; a < n; a++) for (int b = 0; b < n; b++) {
    if (a == b) continue;
    int sda = CAND[a], scl = CAND[b];
    Wire.end();
    if (!Wire.begin(sda, scl, 100000)) continue;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0)
        logf("  FOUND 0x%02X  SDA=%d SCL=%d%s\n", addr, sda, scl,
                      (addr == 0x3C || addr == 0x3D) ? "  <- SSD1306 OLED?" : "");
    }
  }
  Wire.end();
  logln("scan done");
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
  logln("Mic meter 5s - clap/talk. Flat 0 => wrong pins/L-R/orientation.");
  i2sInstall(I2S_NUM_0, (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), MIC_SCK, MIC_WS, I2S_PIN_NO_CHANGE, MIC_SD, 16000);
  int32_t buf[256];
  uint32_t end = millis() + 5000;
  while (millis() < end) {
    size_t got = 0;
    i2s_read(I2S_NUM_0, buf, sizeof(buf), &got, 100);
    int n = got / 4; if (!n) continue;
    double acc = 0; for (int i = 0; i < n; i++) { double s = (buf[i] >> 8); acc += s * s; }
    int rms = (int)sqrt(acc / n);
    logf("rms=%7d ", rms);
    for (int i = 0; i < min(rms / 2000, 60); i++) logp('#');
    logln();
    delay(100);
  }
  i2s_driver_uninstall(I2S_NUM_0);
}

void toneTest() {
  logln("Playing 440Hz for 2s on MAX98357A (keep volume low).");
  i2sInstall(I2S_NUM_1, (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), AMP_BCLK, AMP_LRC, AMP_DIN, I2S_PIN_NO_CHANGE, 16000);
  static int32_t b[160];
  for (int rep = 0; rep < 2 * 100; rep++) {   // 160 samples = 10ms
    for (int i = 0; i < 160; i++) b[i] = (int32_t)(sinf(2 * PI * 440 * (rep * 160 + i) / 16000.0f) * 3e7);
    size_t w; i2s_write(I2S_NUM_1, b, sizeof(b), &w, portMAX_DELAY);
  }
  i2s_driver_uninstall(I2S_NUM_1);
}

static SPIClass tftSPI(FSPI);
static const uint16_t COLS[] = {ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE, ST77XX_WHITE};

// draw the 4-colour cycle with the given wiring; msg lines are printed on the panel when it works
static void tftDraw(int sck, int mosi, int dc, int rst, int cs, int bl, int holdMs, const char* tag) {
  tftSPI.end();
  tftSPI.begin(sck, -1, mosi, cs);
  Adafruit_ST7735* t = new Adafruit_ST7735(&tftSPI, cs, dc, rst);
  if (bl >= 0) { pinMode(bl, OUTPUT); digitalWrite(bl, HIGH); }
  t->initR(INITR_BLACKTAB);
  for (auto c : COLS) { t->fillScreen(c); delay(holdMs); }
  t->fillScreen(ST77XX_BLACK); t->setTextColor(ST77XX_WHITE); t->setTextSize(1);
  t->setCursor(2, 2);  t->println(tag);
  t->printf("\nCLK  %d\nMOSI %d\nDC   %d\nRST  %d\nCS   %d\nBL   %d\n", sck, mosi, dc, rst, cs, bl);
  delete t;
}

void tftTest() {
  logf("TFT color cycle: CLK=%d MOSI=%d DC=%d RST=%d CS=%d BL=%d\n", TFT_SCK, TFT_MOSI, TFT_DC, TFT_RST, TFT_CS, TFT_BL);
  Serial.flush();
  tftDraw(TFT_SCK, TFT_MOSI, TFT_DC, TFT_RST, TFT_CS, TFT_BL, 500, "POC OK");
  Serial.end(); Serial.begin(115200);   // 43/44 are also UART0; take them back for the console
}

// Backlight hunt: each candidate pin goes HIGH alone for 1.5 s. Note the printed pin when the screen glows.
void blHunt() {
  logln("Backlight hunt: watch the TFT. Each pin is driven HIGH for 1.5s (screen needs 3.3V/GND connected).");
  for (int k = 0; k < NHUNT; k++) {
    int pin = HUNT[k];
    logf("  GPIO%d HIGH now\n", pin); Serial.flush();
    pinMode(pin, OUTPUT); digitalWrite(pin, HIGH); delay(1500);
    digitalWrite(pin, LOW); pinMode(pin, INPUT);
    Serial.end(); Serial.begin(115200);
  }
  logln("done. If it never lit: BL may be tied to 3.3V (use B-1), or the header uses another pin.");
}

// TFT hunt: every assignment of {CLK,MOSI,DC,RST,CS} over the candidate pins. The correct one paints the mapping on the panel.
void tftHunt() {
  int pins[10], n = 0;
  for (int k = 0; k < NHUNT; k++) if (HUNT[k] != TFT_BL) pins[n++] = HUNT[k];      // BL pin excluded when known (B<pin>)
  logf("TFT hunt over %d pins (BL=%d). GPIO43/44 are the UART console, so NO per-try logging: watch the panel, it prints the mapping when it works. Hold BOOT to stop.\n", n, TFT_BL); Serial.flush(); pinMode(0, INPUT_PULLUP); STOP = false;
  int idx[5], used[10] = {0};
  int count = 0;
  // iterative permutation of 5 roles over n pins
  int stack[5]; int depth = 0; stack[0] = -1;
  while (depth >= 0) {
    {
      if (stack[depth] >= 0) used[stack[depth]] = 0;
      int c = stack[depth] + 1;
      while (c < n && used[c]) c++;
      if (c >= n) { depth--; continue; }
      stack[depth] = c; used[c] = 1; idx[depth] = c;
      if (depth < 4) { depth++; stack[depth] = -1; continue; }
      // full assignment
      count++;
      int sck = pins[idx[0]], mosi = pins[idx[1]], dc = pins[idx[2]], rst = pins[idx[3]], cs = pins[idx[4]];
      char tag[24]; snprintf(tag, sizeof tag, "FOUND #%d", count);
      tftDraw(sck, mosi, dc, rst, cs, TFT_BL, 120, tag);
      if (digitalRead(0) == LOW || STOP) { STOP = false; Serial.end(); Serial.begin(115200); logf("stopped after #%d (see mapping on the panel)\n", count); return; }
    }
  }
  Serial.end(); Serial.begin(115200);
  logf("hunt finished, %d combos tried. Nothing lit => wrong candidate set or no power/GND; check wiring and BL.\n", count);
}

// T bl clk mosi dc cs rst : set TFT pins then press d.  e.g.  T38 19 20 47 45 21
static void setTft() {
  int v[6]; for (int i = 0; i < 6; i++) v[i] = IN->parseInt();
  TFT_BL = v[0]; TFT_SCK = v[1]; TFT_MOSI = v[2]; TFT_DC = v[3]; TFT_CS = v[4]; TFT_RST = v[5];
  logf("TFT set: BL=%d CLK=%d MOSI=%d DC=%d CS=%d RST=%d (press d to test)\n", v[0], v[1], v[2], v[3], v[4], v[5]);
}
// P a b c ... : set the hunt candidate pins (up to 10), terminated by newline
static void setHunt() {
  NHUNT = 0;
  while (NHUNT < 10) { int v = IN->parseInt(); if (!v && IN->peek() < 0) break; HUNT[NHUNT++] = v; if (IN->peek() == '\n' || IN->peek() < 0) break; }
  logf("hunt pins (%d):", NHUNT); for (int i = 0; i < NHUNT; i++) logf(" %d", HUNT[i]); logln();
}
// q = list presets. q1, q2, ... = run ONLY that preset and leave it on screen (it also becomes the pins used by d).
struct Preset { const char* n; int bl, clk, mosi, dc, cs, rst; };
static const Preset PRESETS[] = {
  {"expansion-v1.2", 42, 44, 43, 38, 45,  1},   // OceanLabz Expansion Board guide
  {"tutorial-cam",   38, 19, 20, 47, 45, 21},   // OceanLabz XiaoZhi camera-board tutorial
  {"AIExplorer-h",   44, 38, 42,  1, 45, 43},   // OceanLabz AI Explorer repo header
  {"xiaozhi-lcd",    42, 21, 47, 40, 41, 45},   // upstream XiaoZhi bread-compact-wifi-lcd
};
void presets() {
  int n = sizeof(PRESETS) / sizeof(PRESETS[0]);
  int k = IN->parseInt();
  if (k < 1 || k > n) {
    logln("Presets (type q<number> + Enter, e.g. q1):");
    for (int i = 0; i < n; i++) { const Preset& p = PRESETS[i];
      logf("  %d  %-15s BL=%d CLK=%d MOSI=%d DC=%d CS=%d RST=%d\n", i + 1, p.n, p.bl, p.clk, p.mosi, p.dc, p.cs, p.rst); }
    return;
  }
  const Preset& p = PRESETS[k - 1];
  logf("Running preset %d (%s): BL=%d CLK=%d MOSI=%d DC=%d CS=%d RST=%d. Watch the screen.\n", k, p.n, p.bl, p.clk, p.mosi, p.dc, p.cs, p.rst);
  Serial.flush();
  TFT_BL = p.bl; TFT_SCK = p.clk; TFT_MOSI = p.mosi; TFT_DC = p.dc; TFT_CS = p.cs; TFT_RST = p.rst;
  tftDraw(p.clk, p.mosi, p.dc, p.rst, p.cs, p.bl, 1000, p.n);   // red, green, blue, white (1s each), then name + pins stay on screen
  Serial.end(); Serial.begin(115200);
  logf("Done preset %d. Did the screen show colours and the text '%s'? If yes this is your wiring.\n", k, p.n);
}

void help() { logln("cmds: s=I2C scan, m=mic, t=tone, d=display, b=backlight hunt, p=TFT pin hunt (hold BOOT to stop), B<pin>=set BL pin (-1 unknown), q=list presets / q<n>=run one, T bl clk mosi dc cs rst=set TFT pins, P pins...=set hunt pins, h=help"); }

void setup() {
  Serial.begin(115200); delay(1500);
  LM = xSemaphoreCreateMutex(); CQ = xQueueCreate(8, 48);
  WiFi.mode(WIFI_AP); WiFi.softAP("OceanLabz-POC");
  xTaskCreate(webTask, "web", 8192, NULL, 1, NULL);
  logln("OceanLabz S3 POC. Wi-Fi monitor: join 'OceanLabz-POC' then open http://192.168.4.1");
  help();
}

static void runCmd(Stream* in) {
  IN = in;
  switch (IN->read()) {
    case 's': i2cScan(); break;
    case 'm': micTest(); break;
    case 't': toneTest(); break;
    case 'd': tftTest(); break;
    case 'q': presets(); break;
    case 'T': setTft(); break;
    case 'P': setHunt(); break;
    case 'b': blHunt(); break;
    case 'p': tftHunt(); break;
    case 'B': { int v = IN->parseInt(); TFT_BL = v; logf("BL pin = %d\n", TFT_BL); break; }
    case 'h': help(); break;
  }
  IN = &Serial;
}

void loop() {
  char c[48];
  if (xQueueReceive(CQ, c, 0) == pdTRUE) { ss.setTimeout(30); ss.set(String(c) + "\n"); runCmd(&ss); return; }
  if (Serial.available()) runCmd(&Serial);
  delay(5);
}
