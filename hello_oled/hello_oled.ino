#include <Wire.h>
#include <U8g2lib.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#define BTN_DOWN D5
#define BTN_SEL  D6

// State
enum Screen { MENU, HOME, LIGHT, FAN };
Screen screen = MENU;
int menuCursor = 0;
int subCursor = 0;
bool lightOn = false;
bool fanOn = false;

// Debounce
bool lastDown = HIGH, lastSel = HIGH;
unsigned long lastDownTime = 0, lastSelTime = 0;
const unsigned long DEBOUNCE = 200;

bool pressed(int pin, bool &last, unsigned long &lastTime) {
  bool val = digitalRead(pin);
  if (val == LOW && last == HIGH && millis() - lastTime > DEBOUNCE) {
    lastTime = millis();
    last = val;
    return true;
  }
  last = val;
  return false;
}

void drawMenu() {
  const char *items[] = {"Home", "Light", "Fan"};
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(20, 12, "=== MENU ===");
  u8g2.setFont(u8g2_font_ncenB10_tr);
  for (int i = 0; i < 3; i++) {
    int y = 30 + i * 14;
    if (i == menuCursor) {
      u8g2.drawStr(4, y, ">");
    }
    u8g2.drawStr(18, y, items[i]);
  }
}

void drawHome() {
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(20, 14, "~ Home ~");
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(4, 32, lightOn ? "Light: ON" : "Light: OFF");
  u8g2.drawStr(4, 46, fanOn ? "Fan:   ON" : "Fan:   OFF");
  u8g2.drawStr(4, 62, "[SEL] Back");
}

void drawToggleScreen(const char *label, bool state) {
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(4, 14, label);
  u8g2.drawStr(70, 14, state ? "ON" : "OFF");

  u8g2.setFont(u8g2_font_ncenB08_tr);
  const char *items[] = {"Toggle", "Back"};
  for (int i = 0; i < 2; i++) {
    int y = 38 + i * 16;
    if (i == subCursor)
      u8g2.drawStr(4, y, ">");
    u8g2.drawStr(18, y, items[i]);
  }
}

void setup() {
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SEL, INPUT_PULLUP);
  Wire.begin(D2, D1);
  u8g2.begin();
}

void loop() {
  bool down = pressed(BTN_DOWN, lastDown, lastDownTime);
  bool sel = pressed(BTN_SEL, lastSel, lastSelTime);

  switch (screen) {
    case MENU:
      if (down) menuCursor = (menuCursor + 1) % 3;
      if (sel) {
        subCursor = 0;
        if (menuCursor == 0) screen = HOME;
        else if (menuCursor == 1) screen = LIGHT;
        else screen = FAN;
      }
      break;

    case HOME:
      if (sel) screen = MENU;
      break;

    case LIGHT:
      if (down) subCursor = (subCursor + 1) % 2;
      if (sel) {
        if (subCursor == 0) lightOn = !lightOn;
        else { screen = MENU; }
      }
      break;

    case FAN:
      if (down) subCursor = (subCursor + 1) % 2;
      if (sel) {
        if (subCursor == 0) fanOn = !fanOn;
        else { screen = MENU; }
      }
      break;
  }

  u8g2.clearBuffer();
  switch (screen) {
    case MENU:  drawMenu(); break;
    case HOME:  drawHome(); break;
    case LIGHT: drawToggleScreen("Light:", lightOn); break;
    case FAN:   drawToggleScreen("Fan:", fanOn); break;
  }
  u8g2.sendBuffer();
  delay(50);
}
