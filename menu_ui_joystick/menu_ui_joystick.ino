#include <Wire.h>
#include <U8g2lib.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --- EEPROM Settings ---
#define EEPROM_MAGIC 0x82660001
struct Settings {
  uint32_t magic;
  bool lightOn;
  bool fanOn;
  int brightness;
  int timerIndex;
  bool invertDisplay;
};

// --- WiFi ---
const char* WIFI_SSID = "x70";
const char* WIFI_PASS = "244466666";
const char* SERVER_URL = "https://8x8.tail73f41e.ts.net/api/state";

// --- Joystick ---
#define JOY_VRY  A0   // up/down axis
#define JOY_SW   D5   // joystick click = select
// VRX not used (ESP8266 has only 1 analog input)

// --- Select button ---
#define BTN_SEL_EXT D4  // external select button (also onboard LED)

// --- Back button (D3) ---
#define BTN_BACK D3   // short press = back, long press = ctrl

// Joystick thresholds (3.3V → A0 reads 0-1024, center ~512)
#define JOY_UP_THRESH    200
#define JOY_DOWN_THRESH  800
#define JOY_DEAD_CENTER  1    // must return to center before next trigger

// Input state
enum { B_UP, B_DOWN, B_SEL, B_BACK, B_CTRL };
bool btn[5] = {};

// Joystick state
bool joyWasCenter = true;
unsigned long joyRepeatTime = 0;
const unsigned long JOY_FIRST_REPEAT = 400;  // ms before repeat starts
const unsigned long JOY_REPEAT_RATE  = 150;  // ms between repeats
int joyHeldDir = 0;  // -1=up, 1=down, 0=center
unsigned long joyHeldSince = 0;

// SW debounce (joystick click)
bool lastSW = HIGH;
unsigned long lastSWTime = 0;

// External select button debounce
bool lastSelExt = HIGH;
unsigned long lastSelExtTime = 0;

// Back button: short/long press
bool lastBack = HIGH;
unsigned long backPressTime = 0;
bool backHandled = false;
const unsigned long LONG_PRESS = 500;  // ms for ctrl
const unsigned long DEBOUNCE = 180;

void readInputs() {
  btn[B_UP] = btn[B_DOWN] = btn[B_SEL] = btn[B_BACK] = btn[B_CTRL] = false;

  // --- Joystick Y axis ---
  int vry = analogRead(JOY_VRY);
  int dir = 0;
  if (vry < JOY_UP_THRESH) dir = -1;       // up
  else if (vry > JOY_DOWN_THRESH) dir = 1;  // down

  if (dir == 0) {
    joyWasCenter = true;
    joyHeldDir = 0;
  } else if (joyWasCenter) {
    // first trigger
    if (dir == -1) btn[B_UP] = true;
    if (dir == 1)  btn[B_DOWN] = true;
    joyWasCenter = false;
    joyHeldDir = dir;
    joyHeldSince = millis();
    joyRepeatTime = millis();
  } else if (dir == joyHeldDir) {
    // held — auto repeat
    unsigned long held = millis() - joyHeldSince;
    if (held > JOY_FIRST_REPEAT && millis() - joyRepeatTime > JOY_REPEAT_RATE) {
      if (dir == -1) btn[B_UP] = true;
      if (dir == 1)  btn[B_DOWN] = true;
      joyRepeatTime = millis();
    }
  }

  // --- Joystick SW (click = select) ---
  bool sw = digitalRead(JOY_SW);
  if (sw == LOW && lastSW == HIGH && millis() - lastSWTime > DEBOUNCE) {
    btn[B_SEL] = true;
    lastSWTime = millis();
  }
  lastSW = sw;

  // --- External select button (D4) ---
  bool selExt = digitalRead(BTN_SEL_EXT);
  if (selExt == LOW && lastSelExt == HIGH && millis() - lastSelExtTime > DEBOUNCE) {
    btn[B_SEL] = true;
    lastSelExtTime = millis();
  }
  lastSelExt = selExt;

  // --- Back button: short = back, long hold = ctrl ---
  bool back = digitalRead(BTN_BACK);
  if (back == LOW && lastBack == HIGH) {
    // pressed
    backPressTime = millis();
    backHandled = false;
  }
  if (back == LOW && !backHandled) {
    if (millis() - backPressTime >= LONG_PRESS) {
      btn[B_CTRL] = true;
      backHandled = true;
    }
  }
  if (back == HIGH && lastBack == LOW) {
    // released
    if (!backHandled && millis() - backPressTime < LONG_PRESS) {
      btn[B_BACK] = true;
    }
    backHandled = false;
  }
  lastBack = back;
}

// State
bool lightOn = false;
bool fanOn = false;
int brightness = 255;

// --- Screen auto-off timer ---
int timerIndex = 0;
const unsigned long timerValues[] = {0, 10000, 30000, 60000, 300000};
const char *timerLabels[] = {"Off", "10s", "30s", "1 min", "5 min"};
const int timerCount = 5;
unsigned long lastActivityTime = 0;
bool screenOff = false;

// --- Invert display ---
bool invertDisplay = false;

// --- State tracking for server sync ---
bool prevLightOn = false;
bool prevFanOn = false;
int prevBrightness = 255;
int prevTimerIndex = 0;
bool prevInvertDisplay = false;
unsigned long lastSyncTime = 0;
const unsigned long SYNC_INTERVAL = 5000;  // periodic sync every 5s

enum Screen { MAIN_MENU, HOME, LIGHT, FAN, SETTINGS, SET_BRIGHT, SET_TIMER, SET_INVERT, ABOUT };
Screen screen = MAIN_MENU;
int cursor = 0;
int setCursor = 0;

// Menu items
const char *mainItems[] = {"Home", "Light", "Fan", "Settings", "About"};
const int mainCount = 5;
const char *setItems[] = {"Brightness", "Screen Timer", "Invert Color", "Back"};
const int setCount = 4;

// --- WiFi helpers ---

void wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  // Non-blocking — we check WiFi.status() in loop
}

bool stateChanged() {
  return lightOn != prevLightOn ||
         fanOn != prevFanOn ||
         brightness != prevBrightness ||
         timerIndex != prevTimerIndex ||
         invertDisplay != prevInvertDisplay;
}

void saveStateSnapshot() {
  prevLightOn = lightOn;
  prevFanOn = fanOn;
  prevBrightness = brightness;
  prevTimerIndex = timerIndex;
  prevInvertDisplay = invertDisplay;

  // Save to EEPROM
  Settings s = {
    EEPROM_MAGIC,
    lightOn,
    fanOn,
    brightness,
    timerIndex,
    invertDisplay
  };
  EEPROM.put(0, s);
  EEPROM.commit();
}

void sendStateToServer() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();  // skip cert verification for tailscale
  HTTPClient http;
  http.setTimeout(3000);

  if (http.begin(client, SERVER_URL)) {
    http.addHeader("Content-Type", "application/json");

    char json[256];
    snprintf(json, sizeof(json),
      "{\"light\":%s,\"fan\":%s,\"brightness\":%d,"
      "\"timer_index\":%d,\"timer_label\":\"%s\","
      "\"invert\":%s,\"uptime_sec\":%lu,\"wifi_rssi\":%d}",
      lightOn ? "true" : "false",
      fanOn ? "true" : "false",
      brightness,
      timerIndex,
      timerLabels[timerIndex],
      invertDisplay ? "true" : "false",
      millis() / 1000,
      WiFi.RSSI()
    );

    http.POST(json);
    http.end();
    saveStateSnapshot();
    lastSyncTime = millis();
  }
}

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
  if (on)
    u8g2.drawDisc(x + u8g2.getStrWidth(label) + 8, y - 4, 4);
  else
    u8g2.drawCircle(x + u8g2.getStrWidth(label) + 8, y - 4, 4);
}

void drawToggleScreen(const char *title, bool state) {
  drawHeader(title);
  u8g2.setFont(u8g2_font_ncenB14_tr);
  const char *stateStr = state ? "ON" : "OFF";
  int w = u8g2.getStrWidth(stateStr);
  u8g2.drawStr((128 - w) / 2, 40, stateStr);

  if (state)
    u8g2.drawDisc(64, 54, 4);
  else
    u8g2.drawCircle(64, 54, 4);

  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 64, "[Click]Tog");
  u8g2.drawStr(78, 64, "[Back]<-");
}

void drawProgressBar(int x, int y, int w, int h, int val, int maxVal) {
  u8g2.drawFrame(x, y, w, h);
  int fill = (long)(w - 2) * val / maxVal;
  u8g2.drawBox(x + 1, y + 1, fill, h - 2);
}

// --- Screen draw functions ---

void drawMainMenu() {
  drawHeader("MENU");
  u8g2.setFont(u8g2_font_ncenB08_tr);
  if (lightOn) u8g2.drawDisc(4, 6, 2);
  if (fanOn) u8g2.drawDisc(12, 6, 2);
  // WiFi indicator in top-right
  if (WiFi.status() == WL_CONNECTED) u8g2.drawDisc(122, 6, 2);
  else u8g2.drawCircle(122, 6, 2);
  drawList(mainItems, mainCount, cursor, 28);
}

void drawHome() {
  drawHeader("HOME");
  drawStatusIcon(8, 32, "Light:", lightOn);
  drawStatusIcon(8, 48, "Fan:", fanOn);
  u8g2.setFont(u8g2_font_ncenB08_tr);
  // WiFi status
  if (WiFi.status() == WL_CONNECTED) {
    char buf[20];
    sprintf(buf, "WiFi: %ddBm", WiFi.RSSI());
    u8g2.drawStr(68, 32, buf);
  } else {
    u8g2.drawStr(68, 32, "WiFi: --");
  }
  char buf[20];
  unsigned long sec = millis() / 1000;
  sprintf(buf, "Up: %lum %lus", sec / 60, sec % 60);
  u8g2.drawStr(8, 62, buf);
}

void drawLight() { drawToggleScreen("LIGHT", lightOn); }
void drawFan()   { drawToggleScreen("FAN", fanOn); }

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
  u8g2.drawStr(0, 64, "[Joy]Adj");
  u8g2.drawStr(68, 64, "[Hold]Rst");
}

void drawScreenTimer() {
  drawHeader("SCREEN TIMER");
  u8g2.setFont(u8g2_font_ncenB14_tr);
  int w = u8g2.getStrWidth(timerLabels[timerIndex]);
  u8g2.drawStr((128 - w) / 2, 40, timerLabels[timerIndex]);

  u8g2.setFont(u8g2_font_ncenB08_tr);
  if (timerIndex > 0) u8g2.drawStr(4, 40, "<");
  if (timerIndex < timerCount - 1) u8g2.drawStr(120, 40, ">");

  u8g2.drawStr(0, 64, "[Joy]Adj");
  u8g2.drawStr(78, 64, "[Back]<-");
}

void drawInvertColor() {
  drawToggleScreen("INVERT COLOR", invertDisplay);
}

void drawAbout() {
  drawHeader("ABOUT");
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(8, 30, "ESP8266 Menu UI");
  u8g2.drawStr(8, 44, "SH1106 128x64");
  u8g2.drawStr(8, 58, "Joystick+2 Buttons");
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
    if (setCursor == 1) screen = SET_TIMER;
    if (setCursor == 2) screen = SET_INVERT;
    if (setCursor == 3) screen = MAIN_MENU;
  }
  if (btn[B_BACK]) screen = MAIN_MENU;
}

void handleBrightness() {
  if (btn[B_UP])   { brightness += 15; if (brightness > 255) brightness = 255; }
  if (btn[B_DOWN]) { brightness -= 15; if (brightness < 0) brightness = 0; }
  if (btn[B_CTRL]) brightness = 255;
  if (btn[B_SEL] || btn[B_BACK]) screen = SETTINGS;
  u8g2.setContrast(brightness);
}

void handleScreenTimer() {
  if (btn[B_UP])   { timerIndex++; if (timerIndex >= timerCount) timerIndex = timerCount - 1; }
  if (btn[B_DOWN]) { timerIndex--; if (timerIndex < 0) timerIndex = 0; }
  if (btn[B_SEL] || btn[B_BACK]) screen = SETTINGS;
}

void handleInvertColor() {
  if (btn[B_SEL] || btn[B_CTRL]) {
    invertDisplay = !invertDisplay;
    u8g2.sendF("c", invertDisplay ? 0xA7 : 0xA6);
  }
  if (btn[B_BACK]) screen = SETTINGS;
}

void handleAbout() {
  if (btn[B_BACK] || btn[B_SEL]) screen = MAIN_MENU;
}

// Check if any input is active
bool anyInputActive() {
  return btn[B_UP] || btn[B_DOWN] || btn[B_SEL] || btn[B_BACK] || btn[B_CTRL];
}

void setup() {
  pinMode(JOY_SW, INPUT_PULLUP);
  pinMode(BTN_SEL_EXT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  Wire.begin(D2, D1);
  u8g2.begin();

  // Load EEPROM Settings
  EEPROM.begin(sizeof(Settings));
  Settings s;
  EEPROM.get(0, s);
  if (s.magic == EEPROM_MAGIC) {
    lightOn = s.lightOn;
    fanOn = s.fanOn;
    brightness = s.brightness;
    timerIndex = s.timerIndex;
    invertDisplay = s.invertDisplay;
  }
  
  u8g2.setContrast(brightness);
  if (invertDisplay) u8g2.sendF("c", 0xA7);
  
  // Initialize sync state so it sends the initial loaded state up to the server
  saveStateSnapshot();

  lastActivityTime = millis();

  // Start WiFi connection (non-blocking)
  wifiConnect();
}

void loop() {
  readInputs();

  // Track activity for screen timer
  if (anyInputActive()) {
    lastActivityTime = millis();
    if (screenOff) {
      screenOff = false;
      u8g2.setPowerSave(0);
      return;  // consume the wake input
    }
  }

  // Auto screen off on inactivity
  if (!screenOff && timerValues[timerIndex] > 0) {
    if (millis() - lastActivityTime >= timerValues[timerIndex]) {
      screenOff = true;
      u8g2.setPowerSave(1);
    }
  }

  if (screenOff) return;

  switch (screen) {
    case MAIN_MENU:  handleMainMenu(); break;
    case HOME:       handleHome(); break;
    case LIGHT:      handleLight(); break;
    case FAN:        handleFan(); break;
    case SETTINGS:   handleSettings(); break;
    case SET_BRIGHT: handleBrightness(); break;
    case SET_TIMER:  handleScreenTimer(); break;
    case SET_INVERT: handleInvertColor(); break;
    case ABOUT:      handleAbout(); break;
  }

  // Send state to server on change or periodically
  if (stateChanged() || millis() - lastSyncTime >= SYNC_INTERVAL) {
    sendStateToServer();
  }

  // WiFi auto-reconnect
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 10000) {
      wifiConnect();
      lastReconnect = millis();
    }
  }

  u8g2.clearBuffer();
  switch (screen) {
    case MAIN_MENU:  drawMainMenu(); break;
    case HOME:       drawHome(); break;
    case LIGHT:      drawLight(); break;
    case FAN:        drawFan(); break;
    case SETTINGS:   drawSettings(); break;
    case SET_BRIGHT: drawBrightness(); break;
    case SET_TIMER:  drawScreenTimer(); break;
    case SET_INVERT: drawInvertColor(); break;
    case ABOUT:      drawAbout(); break;
  }
  u8g2.sendBuffer();
  delay(30);
}
