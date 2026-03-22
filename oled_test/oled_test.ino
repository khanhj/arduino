#include <Wire.h>
#include <U8g2lib.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

unsigned long fps;

void countFPS(void (*drawFunc)()) {
  unsigned long start = millis();
  int frames = 0;
  while (millis() - start < 2000) {
    u8g2.clearBuffer();
    drawFunc();
    u8g2.sendBuffer();
    frames++;
    yield();
  }
  fps = frames / 2;
}

void showResult(const char *label, unsigned long f) {
  char buf[32];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(4, 14, label);
  sprintf(buf, "%lu FPS", f);
  u8g2.setFont(u8g2_font_ncenB14_tr);
  u8g2.drawStr(4, 40, buf);
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(4, 60, "Next in 2s...");
  u8g2.sendBuffer();
  delay(2000);
}

// --- Test patterns ---

void fillAll() {
  u8g2.drawBox(0, 0, 128, 64);
}

void checkerboard() {
  for (int y = 0; y < 64; y += 8)
    for (int x = 0; x < 128; x += 8)
      if ((x / 8 + y / 8) % 2 == 0)
        u8g2.drawBox(x, y, 8, 8);
}

void horizontalLines() {
  for (int y = 0; y < 64; y += 2)
    u8g2.drawHLine(0, y, 128);
}

void verticalLines() {
  for (int x = 0; x < 128; x += 2)
    u8g2.drawVLine(x, 0, 64);
}

void diagonalLines() {
  for (int i = 0; i < 192; i += 6)
    u8g2.drawLine(i, 0, i - 64, 64);
}

void concentricCircles() {
  for (int r = 4; r < 32; r += 4)
    u8g2.drawCircle(64, 32, r);
}

void randomPixels() {
  for (int i = 0; i < 500; i++)
    u8g2.drawPixel(random(128), random(64));
}

void textScroll() {
  static int offset = 0;
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(offset % 256 - 128, 20, "Performance Test!");
  u8g2.drawStr(offset % 256 - 128, 40, "ESP8266 + SH1106");
  u8g2.drawStr(offset % 256 - 128, 60, "128x64 OLED");
  offset += 3;
}

int bbx = 0, bby = 0, bdx = 2, bdy = 1;
void bouncingBox() {
  u8g2.drawBox(bbx, bby, 20, 14);
  bbx += bdx; bby += bdy;
  if (bbx <= 0 || bbx >= 108) bdx = -bdx;
  if (bby <= 0 || bby >= 50) bdy = -bdy;
}

// --- Pixel scan tests ---

void scanSlow() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(4, 14, "Pixel Scan: Slow");
  u8g2.sendBuffer();
  delay(1000);
  u8g2.clearBuffer();
  for (int y = 0; y < 64; y += 2) {
    for (int x = 0; x < 128; x += 2) {
      u8g2.drawPixel(x, y);
    }
    yield();
    if (y % 4 == 0) {
      u8g2.sendBuffer();
      delay(30);
    }
  }
  u8g2.sendBuffer();
  delay(1500);
}

void scanFast() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(4, 14, "Pixel Scan: Fast");
  u8g2.sendBuffer();
  delay(1000);
  u8g2.clearBuffer();
  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 128; x++) {
      u8g2.drawPixel(x, y);
    }
    u8g2.sendBuffer();
    yield();
  }
  delay(1500);
}

void invertTest() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(10, 36, "Invert Test");
  u8g2.sendBuffer();
  delay(500);
  for (int i = 0; i < 6; i++) {
    u8g2_SendF(u8g2.getU8g2(), "c", 0xA7); // invert
    delay(300);
    u8g2_SendF(u8g2.getU8g2(), "c", 0xA6); // normal
    delay(300);
    yield();
  }
}

void setup() {
  Wire.begin(D2, D1);
  u8g2.begin();

  // Splash
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(8, 20, "OLED Perf");
  u8g2.drawStr(8, 40, "Test Suite");
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(8, 58, "128x64 SH1106");
  u8g2.sendBuffer();
  delay(2000);
}

void loop() {
  // FPS benchmarks
  countFPS(fillAll);         showResult("Fill Screen", fps);
  countFPS(checkerboard);    showResult("Checkerboard", fps);
  countFPS(horizontalLines); showResult("H-Lines", fps);
  countFPS(verticalLines);   showResult("V-Lines", fps);
  countFPS(diagonalLines);   showResult("Diag Lines", fps);
  countFPS(concentricCircles); showResult("Circles", fps);
  countFPS(randomPixels);    showResult("500 Rnd Pixels", fps);
  countFPS(textScroll);      showResult("Text Scroll", fps);
  countFPS(bouncingBox);     showResult("Bouncing Box", fps);

  // Pixel scan tests
  scanSlow();
  scanFast();

  // Invert test
  invertTest();

  // Done
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(10, 24, "All Tests");
  u8g2.drawStr(10, 44, "Complete!");
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(10, 62, "Restarting...");
  u8g2.sendBuffer();
  delay(3000);
}
