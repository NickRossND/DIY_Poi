// ============================================================
// bluetoothStrip_secondary.ino
// ============================================================
// Secondary ESP32 unit for DIY Poi.
//
// Receives LED commands wirelessly from the primary unit via
// ESP-NOW and runs the same pattern/POV logic. No Bluetooth
// is needed — the primary bridges BT → ESP-NOW so one command
// from your phone controls both units simultaneously.
//
// Flash this sketch onto the second ESP32. Hardware setup is
// identical to the primary (same DATA_PIN, NUM_LEDS).
//
// Architecture:
//   bluetoothStrip.ino           — BT comms, command parsing, ESP-NOW TX
//   bluetoothStrip_secondary.ino — ESP-NOW RX, same pattern logic, no BT
//   patterns.h                   — blinkPattern, pulsePattern
//   pov_patterns.h               — POV buffer, pattern generators, font
//
// Dependencies:
//   esp32 by Espressif Systems version 2.0.17 or 2.x.xx
// ============================================================

#include <FastLED.h>
#include <esp_now.h>
#include <WiFi.h>
#include "patterns.h"
#include "pov_patterns.h"

// ------------------- Hardware Configuration -------------------
const int NUM_LEDS = 5;
#define DATA_PIN 2

// ------------------- Global Variables -------------------
int bright = 65;
CRGB leds[NUM_LEDS];
String currentPattern = "";
CRGB currentColor = CRGB::Red;

// POV state (declared extern in pov_patterns.h, defined here)
CRGB     povBuffer[POV_MAX_COLS][POV_MAX_LEDS];
uint16_t povNumCols     = 60;
uint32_t povColDelayUs  = 2000;
uint16_t povCurrentCol  = 0;
uint32_t povLastColTime = 0;
bool     povActive      = false;

// ------------------- ESP-NOW receive buffer -------------------
// The receive callback runs on a different core than loop().
// We store the command here and process it in loop() to avoid
// calling FastLED from a WiFi task context.
volatile bool cmdReady = false;
char          cmdBuf[256];

// ============================================================
// Forward declarations
// ============================================================
void handleCommand(String cmd);
void parsePovColors(String s, CRGB* colors, uint8_t* count, uint8_t maxColors);
CRGB colorFromString(String s);

// ============================================================
// ESP-NOW receive callback  (runs on WiFi/BT core)
// ============================================================
void onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (len > 0 && len < (int)sizeof(cmdBuf)) {
    memcpy(cmdBuf, data, len);
    cmdBuf[len] = '\0';
    cmdReady = true;
  }
}

// ============================================================
// setup()
// ============================================================
void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
  } else {
    esp_now_register_recv_cb(onDataRecv);
    Serial.println("Secondary ready — waiting for ESP-NOW commands");
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
  }

  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(bright);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

// ============================================================
// loop()
// ============================================================
void loop() {
  // Process any command forwarded from the primary via ESP-NOW
  if (cmdReady) {
    cmdReady = false;
    handleCommand(String(cmdBuf));
  }

  if (!povActive) {
    if      (currentPattern == "blink1hz")  blinkPattern(1,  currentColor);
    else if (currentPattern == "blink10hz") blinkPattern(10, currentColor);
    else if (currentPattern == "blink50hz") blinkPattern(50, currentColor);
    else if (currentPattern == "pulse")     pulsePattern(currentColor);
  }

  if (povActive) {
    povTick();
  }

  yield();
}

// ============================================================
// handleCommand()  — same logic as primary; responses go to Serial
// ============================================================
void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  Serial.println("CMD = [" + cmd + "]");

  if (cmd == "led on") {
    povActive = false;
    fill_solid(leds, NUM_LEDS, currentColor);
    FastLED.show();

  } else if (cmd == "led off") {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    currentPattern = "";
    povActive = false;

  } else if (cmd.startsWith("color ")) {
    currentColor = colorFromString(cmd.substring(6));
    fill_solid(leds, NUM_LEDS, currentColor);
    FastLED.show();

  } else if (cmd.startsWith("bright ")) {
    bright = cmd.substring(7).toInt();
    if (bright > 255) {
      Serial.println("255 is max value.");
    } else {
      FastLED.setBrightness(bright);
      FastLED.show();
    }

  } else if (cmd.startsWith("pattern ")) {
    String pattern = cmd.substring(8);
    povActive = false;

    if      (pattern == "1") currentPattern = "blink1hz";
    else if (pattern == "2") currentPattern = "blink10hz";
    else if (pattern == "3") currentPattern = "blink50hz";
    else if (pattern == "4") currentPattern = "pulse";
    else                     currentPattern = "";

    Serial.println("Pattern set to: " + currentPattern);

  } else if (cmd.startsWith("pov ")) {
    String sub      = cmd.substring(4);
    int    spaceIdx = sub.indexOf(' ');

    String patName, colorStr;
    if (spaceIdx == -1) {
      patName  = sub;
      colorStr = "";
    } else {
      patName  = sub.substring(0, spaceIdx);
      colorStr = sub.substring(spaceIdx + 1);
    }

    CRGB    colors[4];
    uint8_t numColors = 0;
    parsePovColors(colorStr, colors, &numColors, 4);
    currentPattern = "";

    if (patName == "off") {
      povActive = false;
      fill_solid(leds, NUM_LEDS, CRGB::Black);
      FastLED.show();

    } else if (patName == "rainbow") {
      povRainbow(60);
      povActive = true;

    } else if (patName == "stripes") {
      if (numColors == 0) { colors[0] = CRGB::Red; colors[1] = CRGB::Blue; numColors = 2; }
      povStripes(60, colors, numColors);
      povActive = true;

    } else if (patName == "rings") {
      if (numColors == 0) {
        colors[0]=CRGB::Red; colors[1]=CRGB::Green;
        colors[2]=CRGB::Blue; numColors = 3;
      }
      povRings(60, colors, numColors);
      povActive = true;

    } else if (patName == "gradient") {
      if (numColors < 1) colors[0] = currentColor;
      if (numColors < 2) colors[1] = CRGB::Black;
      povGradient(60, colors[0], colors[1]);
      povActive = true;

    } else if (patName == "sine") {
      if (numColors < 1) colors[0] = CRGB::White;
      if (numColors < 2) colors[1] = CRGB::Black;
      povSine(60, colors[0], colors[1]);
      povActive = true;

    } else if (patName == "diamond") {
      if (numColors < 1) colors[0] = CRGB(255, 180, 0);
      if (numColors < 2) colors[1] = CRGB::Black;
      povDiamond(60, colors[0], colors[1]);
      povActive = true;

    } else if (patName == "zigzag") {
      if (numColors < 2) { colors[0] = CRGB::Red; colors[1] = CRGB::Black; numColors = 2; }
      povZigzag(60, colors, numColors);
      povActive = true;

    } else if (patName == "stepped") {
      if (numColors < 2) {
        colors[0]=CRGB::Red; colors[1]=CRGB::White;
        colors[2]=CRGB(255,180,0); colors[3]=CRGB::Black; numColors = 4;
      }
      povStepped(60, colors, numColors);
      povActive = true;

    } else if (patName == "sparkle") {
      if (numColors < 1) colors[0] = CRGB::White;
      if (numColors < 2) colors[1] = CRGB::Black;
      povSparkle(60, colors[0], colors[1]);
      povActive = true;

    } else if (patName == "text") {
      if (colorStr.length() == 0) {
        Serial.println("ERR: pov text <word>");
      } else {
        povText(colorStr.c_str(), currentColor, CRGB::Black);
        povActive = true;
      }

    } else {
      Serial.println("ERR: unknown POV pattern '" + patName + "'");
    }

  } else if (cmd.startsWith("speed ")) {
    uint32_t val = (uint32_t)cmd.substring(6).toInt();
    if (val >= 200 && val <= 100000) {
      povColDelayUs = val;
    } else {
      Serial.println("ERR: speed must be 200-100000");
    }

  } else {
    Serial.println("Unknown command");
  }
}

// ============================================================
// parsePovColors()
// ============================================================
void parsePovColors(String s, CRGB* colors, uint8_t* count, uint8_t maxColors) {
  *count = 0;
  s.trim();
  if (s.length() == 0) return;

  while (*count < maxColors && s.length() > 0) {
    int    commaIdx = s.indexOf(',');
    String part;

    if (commaIdx == -1) {
      part = s;
      s    = "";
    } else {
      part = s.substring(0, commaIdx);
      s    = s.substring(commaIdx + 1);
    }
    part.trim();
    if (part.length() > 0) {
      colors[(*count)++] = colorFromString(part);
    }
  }
}

// ============================================================
// colorFromString()
// ============================================================
CRGB colorFromString(String s) {
  s.trim(); s.toLowerCase();

  if (s == "random" || s == "rand") return CHSV(random8(), 255, 255);
  if (s == "off" || s == "black")   return CRGB::Black;

  if (s.length() == 7 && s[0] == '#') {
    uint32_t v = strtoul(s.c_str() + 1, nullptr, 16);
    return CRGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
  }

  struct { const char* name; CRGB c; } map[] = {
    {"red",CRGB::Red},{"green",CRGB::Green},{"blue",CRGB::Blue},
    {"white",CRGB::White},{"yellow",CRGB::Yellow},{"cyan",CRGB::Cyan},
    {"magenta",CRGB::Magenta},{"purple",CRGB::Purple},{"orange",CRGB::Orange},
    {"pink", CRGB(255,105,180)}, {"teal", CRGB(0,128,128)},
    {"gold", CRGB(255,180,0)}
  };

  for (auto &m : map)
    if (s == m.name) return m.c;

  return CRGB::White;
}
