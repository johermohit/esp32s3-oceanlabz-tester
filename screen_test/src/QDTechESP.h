// ESP32 port of Adafruit_QDTech (https://github.com/zigwart/Adafruit_QDTech) for the QDTech 1.8" 128x160
// module with a Samsung S6D02A1 controller. The AVR port-register / SPDR code is replaced with the ESP32
// SPIClass; the init sequence (from Henning Karlsen's UTFT via Gilchrist's Adafruit_QDTech) is unchanged.
// Original: Adafruit_QDTech by Gilchrist, based on Adafruit's ST7735 library (Limor Fried/Ladyada), MIT license.
#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>

class QDTechESP : public Adafruit_GFX {
 public:
  QDTechESP(SPIClass& spi, int8_t cs, int8_t dc, int8_t rst) : Adafruit_GFX(128, 160), _spi(spi), _cs(cs), _dc(dc), _rst(rst) {}

  // Bit-bang mode drives CLK/DIN with plain digitalWrite (no SPI peripheral) to rule out SPI/pin-matrix problems.
  void setPins(int8_t sck, int8_t mosi) { _sck = sck; _mosi = mosi; }
  void setBitBang(bool on) {
    _bb = on;
    if (on) { _spi.end(); pinMode(_sck, OUTPUT); pinMode(_mosi, OUTPUT); digitalWrite(_sck, LOW); }
    else _spi.begin(_sck, -1, _mosi, -1);
  }
  bool bitBang() const { return _bb; }
  void setMode(uint8_t m) { _mode = m; }
  uint8_t mode() const { return _mode; }
  void rawCmd(uint8_t c) { cmd(c); }     // send one command byte, nothing else (for the wiring ping test)

  void begin(uint32_t hz = 4000000) {
    _hz = hz;
    pinMode(_cs, OUTPUT); digitalWrite(_cs, HIGH);
    pinMode(_dc, OUTPUT); digitalWrite(_dc, HIGH);
    if (_rst >= 0) {                      // hardware reset
      pinMode(_rst, OUTPUT);
      digitalWrite(_rst, HIGH); delay(100);
      digitalWrite(_rst, LOW);  delay(100);
      digitalWrite(_rst, HIGH); delay(200);
    }
    commandList(INIT);
    setRotation(0);
  }

  void setAddrWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    cmd(0x2A); data(0); data(x0); data(0); data(x1);
    cmd(0x2B); data(0); data(y0); data(0); data(y1);
    cmd(0x2C);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x < 0 || x >= _width || y < 0 || y >= _height) return;
    setAddrWindow(x, y, x + 1, y + 1);
    pixels(c, 1);
  }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override { fillRect(x, y, 1, h, c); }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override { fillRect(x, y, w, 1, c); }
  void fillScreen(uint16_t c) override { fillRect(0, 0, _width, _height, c); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override {
    if (x >= _width || y >= _height || w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
    if (x + w > _width) w = _width - x;
    if (y + h > _height) h = _height - y;
    setAddrWindow(x, y, x + w - 1, y + h - 1);
    pixels(c, (uint32_t)w * h);
  }

  void setRotation(uint8_t m) override {   // 0 portrait, 1 landscape, as in the original library
    cmd(0x36);
    rotation = m % 4;
    switch (rotation) {
      case 0: data(0x40 | 0x80 | 0x08); _width = 128; _height = 160; break;
      case 1: data(0x80 | 0x20 | 0x08); _width = 160; _height = 128; break;
      case 2: data(0x08);               _width = 128; _height = 160; break;
      case 3: data(0x40 | 0x20 | 0x08); _width = 160; _height = 128; break;
    }
  }
  void invertDisplay(bool i) override { cmd(i ? 0x21 : 0x20); }

 private:
  SPIClass& _spi; int8_t _cs, _dc, _rst, _sck = -1, _mosi = -1; uint32_t _hz = 4000000; bool _bb = false; uint8_t _mode = SPI_MODE0;
  void bb(uint8_t b) { for (int i = 7; i >= 0; i--) { digitalWrite(_mosi, (b >> i) & 1); digitalWrite(_sck, HIGH); digitalWrite(_sck, LOW); } }

  void cmd(uint8_t c) { if (_bb) { digitalWrite(_dc, LOW); digitalWrite(_cs, LOW); bb(c); digitalWrite(_cs, HIGH); return; } _spi.beginTransaction(SPISettings(_hz, MSBFIRST, _mode)); digitalWrite(_dc, LOW); digitalWrite(_cs, LOW); _spi.transfer(c); digitalWrite(_cs, HIGH); _spi.endTransaction(); }
  void data(uint8_t d) { if (_bb) { digitalWrite(_dc, HIGH); digitalWrite(_cs, LOW); bb(d); digitalWrite(_cs, HIGH); return; } _spi.beginTransaction(SPISettings(_hz, MSBFIRST, _mode)); digitalWrite(_dc, HIGH); digitalWrite(_cs, LOW); _spi.transfer(d); digitalWrite(_cs, HIGH); _spi.endTransaction(); }
  void pixels(uint16_t c, uint32_t n) {
    if (_bb) { digitalWrite(_dc, HIGH); digitalWrite(_cs, LOW); while (n--) { bb(c >> 8); bb(c); } digitalWrite(_cs, HIGH); return; }
    uint8_t buf[256]; for (int i = 0; i < 256; i += 2) { buf[i] = c >> 8; buf[i + 1] = c; }
    _spi.beginTransaction(SPISettings(_hz, MSBFIRST, _mode)); digitalWrite(_dc, HIGH); digitalWrite(_cs, LOW);
    while (n) { uint32_t k = n > 128 ? 128 : n; _spi.writeBytes(buf, k * 2); n -= k; }
    digitalWrite(_cs, HIGH); _spi.endTransaction();
  }

  static constexpr uint8_t DELAY = 0x80;
  void commandList(const uint8_t* a) {
    uint8_t n = *a++;
    while (n--) {
      cmd(*a++);
      uint8_t na = *a++; uint16_t ms = na & DELAY; na &= ~DELAY;
      while (na--) data(*a++);
      if (ms) { ms = *a++; if (ms == 255) ms = 500; delay(ms); }
    }
  }
  static constexpr uint8_t INIT[] = {
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
    0x11, DELAY, 50,
    0xf3, 2 + DELAY, 0x00, 0x01, 50, 0xf3, 2 + DELAY, 0x00, 0x03, 50, 0xf3, 2 + DELAY, 0x00, 0x07, 50, 0xf3, 2 + DELAY, 0x00, 0x0f, 50,
    0xf4, 15 + DELAY, 0x00, 0x04, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0x07, 0x00, 0x3C, 0x36, 0x00, 0x3C, 0x36, 0x00, 50,
    0xf3, 2 + DELAY, 0x00, 0x1f, 50, 0xf3, 2 + DELAY, 0x00, 0x7f, 50, 0xf3, 2 + DELAY, 0x00, 0xff, 50,
    0xfd, 11, 0x00, 0x00, 0x00, 0x17, 0x10, 0x00, 0x00, 0x01, 0x00, 0x16, 0x16,
    0xf4, 15, 0x00, 0x09, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0x07, 0x00, 0x3C, 0x36, 0x00, 0x3C, 0x36, 0x00,
    0x36, 1, 0x08, 0x35, 1, 0x00, 0x3a, 1 + DELAY, 0x05, 150, 0x29, 0, 0x2c, 0};
};
constexpr uint8_t QDTechESP::INIT[];
