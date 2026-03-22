#include <Wire.h>
#include <U8g2lib.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Button pins — all wired to GND, using internal pull-up
#define BTN_UP   D5
#define BTN_DOWN D6
#define BTN_SEL  D7
#define BTN_BACK D3
#define BTN_CTRL D0  // GPIO16: needs external 10K pull-up to 3V3

// Debounce
struct Button {
  int pin;
  bool last;
  unsigned long lastTime;
};

Button buttons[] = {
  {BTN_UP,   HIGH, 0},
  {BTN_DOWN, HIGH, 0},
  {BTN_SEL,  HIGH, 0},
  {BTN_BACK, HIGH, 0},
  {BTN_CTRL, HIGH, 0},
};

enum { B_UP, B_DOWN, B_SEL, B_BACK, B_CTRL };
bool btn[5] = {};
const unsigned long DEBOUNCE = 180;

void readButtons() {
  for (int i = 0; i < 5; i++) {
    bool val = digitalRead(buttons[i].pin);
    btn[i] = false;
    if (val == LOW && buttons[i].last == HIGH &&
        millis() - buttons[i].lastTime > DEBOUNCE) {
      buttons[i].lastTime = millis();
      btn[i] = true;
    }
    buttons[i].last = val;
  }
}

// State
bool lightOn = false;
bool fanOn = false;
int brightness = 255;  // 0-255

enum Screen { MAIN_MENU, HOME, LIGHT, FAN, SETTINGS, SET_BRIGHT, ABOUT };
Screen screen = MAIN_MENU;
int cursor = 0;
int setCursor = 0;

// Menu items
const char *mainItems[] = {"Home", "Light", "Fan", "Settings", "About"};
const int mainCount = 5;
const char *setItems[] = {"Brightness", "Back"};
const int setCount = 2;

// --- Drawing helpers ---

void drawHeader(const char *title) {
  u8g2.setFont(u8g2_font_ncenB08_tr);
  int w = u8g2.getStrWidth(title);
  u8g2.drawStr((128 - w) / 2, 11, title);
  u8g2.drawHLine(0, 14, 128);
}

void drawList(const char **items, int count, int cur, int yStart) {
  u8g2.setFont(u8g2_font_ncenB08_tr);
  int maxVisible = 3;
  int topIndex = 0;
  if (cur >= maxVisible) topIndex = cur - maxVisible + 1;
  if (topIndex > count - maxVisible) topIndex = count - maxVisible;
  if (topIndex < 0) topIndex = 0;

  for (int i = 0; i < maxVisible && (topIndex + i) < count; i++) {
    int idx = topIndex + i;
    int y = yStart + i * 16;
    if (idx == cur) {
      u8g2.drawBox(0, y - 11, 128, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(6, y, items[idx]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(6, y, items[idx]);
    }
  }

  // scroll indicator
  if (count > maxVisible) {
    int barH = 48 * maxVisible / count;
    int barY = yStart - 11 + (48 - barH) * topIndex / (count - maxVisible);
    u8g2.drawBox(125, barY, 3, barH);
  }
}

void drawStatusIcon(int x, int y, const char *label, bool on) {
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(x, y, label);
  if (on) {
    u8g2.drawDisc(x + u8g2.getStrWidth(label) + 8, y - 4, 4);
  } else {
    u8g2.drawCircle(x + u8g2.getStrWidth(label) + 8, y - 4, 4);
  }
}

void drawToggleScreen(const char *title, bool state) {
  drawHeader(title);
  u8g2.setFont(u8g2_font_ncenB14_tr);
  const char *stateStr = state ? "ON" : "OFF";
  int w = u8g2.getStrWidth(stateStr);
  u8g2.drawStr((128 - w) / 2, 40, stateStr);

  // draw filled/empty circle indicator
  if (state)
    u8g2.drawDisc(64, 54, 4);
  else
    u8g2.drawCircle(64, 54, 4);

  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 64, "[SEL]Tog");
  u8g2.drawStr(78, 64, "[BACK]<-");
}

void drawProgressBar(int x, int y, int w, int h, int val, int maxVal) {
  u8g2.drawFrame(x, y, w, h);
  int fill = (long)(w - 2) * val / maxVal;
  u8g2.drawBox(x + 1, y + 1, fill, h - 2);
}

// --- Screen draw functions ---

void drawMainMenu() {
  drawHeader("MENU");
  // add status dots in header
  u8g2.setFont(u8g2_font_ncenB08_tr);
  if (lightOn) u8g2.drawDisc(4, 6, 2);
  if (fanOn) u8g2.drawDisc(12, 6, 2);
  drawList(mainItems, mainCount, cursor, 28);
}

void drawHome() {
  drawHeader("HOME");
  drawStatusIcon(8, 32, "Light:", lightOn);
  drawStatusIcon(8, 48, "Fan:", fanOn);
  u8g2.setFont(u8g2_font_ncenB08_tr);
  char buf[20];
  unsigned long sec = millis() / 1000;
  sprintf(buf, "Up: %lum %lus", sec / 60, sec % 60);
  u8g2.drawStr(8, 62, buf);
}

void drawLight() {
  drawToggleScreen("LIGHT", lightOn);
}

void drawFan() {
  drawToggleScreen("FAN", fanOn);
}

void drawSettings() {
  drawHeader("SETTINGS");
  drawList(setItems, setCount, setCursor, 28);
}

void drawBrightness() {
  drawHeader("BRIGHTNESS");
  char buf[16];
  int pct = brightness * 100 / 255;
  sprintf(buf, "%d%%", pct);
  u8g2.setFont(u8g2_font_ncenB14_tr);
  int w = u8g2.getStrWidth(buf);
  u8g2.drawStr((128 - w) / 2, 36, buf);
  drawProgressBar(8, 42, 112, 10, brightness, 255);
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 64, "[U/D]Adj");
  u8g2.drawStr(68, 64, "[CTRL]Rst");
}

void drawAbout() {
  drawHeader("ABOUT");
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(8, 30, "ESP8266 Menu UI");
  u8g2.drawStr(8, 44, "SH1106 128x64");
  u8g2.drawStr(8, 58, "5-Button Control");
}

// --- Input handling ---

void handleMainMenu() {
  if (btn[B_UP])   cursor = (cursor - 1 + mainCount) % mainCount;
  if (btn[B_DOWN]) cursor = (cursor + 1) % mainCount;
  if (btn[B_SEL]) {
    switch (cursor) {
      case 0: screen = HOME; break;
      case 1: screen = LIGHT; break;
      case 2: screen = FAN; break;
      case 3: screen = SETTINGS; setCursor = 0; break;
      case 4: screen = ABOUT; break;
    }
  }
  // CTRL: quick toggle light/fan from main menu
  if (btn[B_CTRL]) {
    if (cursor == 1) lightOn = !lightOn;
    if (cursor == 2) fanOn = !fanOn;
  }
}

void handleHome() {
  if (btn[B_BACK]) screen = MAIN_MENU;
}

void handleLight() {
  if (btn[B_SEL] || btn[B_CTRL]) lightOn = !lightOn;
  if (btn[B_BACK]) screen = MAIN_MENU;
}

void handleFan() {
  if (btn[B_SEL] || btn[B_CTRL]) fanOn = !fanOn;
  if (btn[B_BACK]) screen = MAIN_MENU;
}

void handleSettings() {
  if (btn[B_UP])   setCursor = (setCursor - 1 + setCount) % setCount;
  if (btn[B_DOWN]) setCursor = (setCursor + 1) % setCount;
  if (btn[B_SEL]) {
    if (setCursor == 0) screen = SET_BRIGHT;
    if (setCursor == 1) screen = MAIN_MENU;
  }
  if (btn[B_BACK]) screen = MAIN_MENU;
}

void handleBrightness() {
  if (btn[B_UP])   { brightness += 15; if (brightness > 255) brightness = 255; }
  if (btn[B_DOWN]) { brightness -= 15; if (brightness < 0) brightness = 0; }
  if (btn[B_CTRL]) brightness = 255; // reset
  if (btn[B_SEL] || btn[B_BACK]) screen = SETTINGS;
  u8g2.setContrast(brightness);
}

void handleAbout() {
  if (btn[B_BACK] || btn[B_SEL]) screen = MAIN_MENU;
}

void setup() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SEL, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(BTN_CTRL, INPUT);  // D0/GPIO16: no internal pull-up, use external

  Wire.begin(D2, D1);
  u8g2.begin();
  u8g2.setContrast(brightness);
}

void loop() {
  readButtons();

  switch (screen) {
    case MAIN_MENU:  handleMainMenu(); break;
    case HOME:       handleHome(); break;
    case LIGHT:      handleLight(); break;
    case FAN:        handleFan(); break;
    case SETTINGS:   handleSettings(); break;
    case SET_BRIGHT: handleBrightness(); break;
    case ABOUT:      handleAbout(); break;
  }

  u8g2.clearBuffer();
  switch (screen) {
    case MAIN_MENU:  drawMainMenu(); break;
    case HOME:       drawHome(); break;
    case LIGHT:      drawLight(); break;
    case FAN:        drawFan(); break;
    case SETTINGS:   drawSettings(); break;
    case SET_BRIGHT: drawBrightness(); break;
    case ABOUT:      drawAbout(); break;
  }
  u8g2.sendBuffer();
  delay(50);
}
