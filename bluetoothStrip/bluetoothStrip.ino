// ============================================================
// bluetoothStrip.ino
// ============================================================
// Controls a WS2812 LED strip via Bluetooth text commands.
//
// EXISTING COMMANDS (unchanged):
//   led on / led off
//   color red / color #FF00AA / color random
//   bright 128
//   pattern 1–4   (1=blink1hz, 2=blink10hz, 3=blink50hz, 4=pulse)
//
// NEW POV COMMANDS:
//   pov rainbow
//   pov stripes red,white,blue
//   pov rings red,green,blue,yellow,purple
//   pov gradient purple,black
//   pov sine yellow,black
//   pov diamond gold,black
//   pov zigzag red,white,black
//   pov stepped red,white,gold,black
//   pov sparkle white,black
//   pov text HELLO           (uses current color as text, black background)
//   pov off
//   speed 2000               (column delay in microseconds; lower = faster)
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

#include "BluetoothSerial.h"
#include <FastLED.h>
#include "patterns.h"       // existing — NOT modified
#include "pov_patterns.h"   // POV buffer, pattern generators, font
#include <esp_now.h>        // ESP-NOW peer-to-peer radio
#include <WiFi.h>           // required for ESP-NOW init

// ------------------- Hardware Configuration -------------------
const int NUM_LEDS = 5;
#define DATA_PIN 2

// ------------------- Global Variables -------------------
int bright = 65;
BluetoothSerial SerialBT;
CRGB leds[NUM_LEDS];
String line;
String currentPattern = "";
CRGB currentColor = CRGB::Red;

// POV state (declared extern in pov_patterns.h, defined here)
CRGB     povBuffer[POV_MAX_COLS][POV_MAX_LEDS];
uint16_t povNumCols     = 60;
uint32_t povColDelayUs  = 2000;
uint16_t povCurrentCol  = 0;
uint32_t povLastColTime = 0;
bool     povActive      = false;

// ESP-NOW broadcast address — reaches all nearby ESP-NOW receivers
uint8_t espNowBroadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Set to true by the BT connect callback; loop() sends the help text then clears it
volatile bool sendHelpOnConnect = false;

// ============================================================
// btCallback()
// ============================================================
// Runs on the BT stack task — only set a flag here; do not call
// SerialBT directly from this context.
// ============================================================
void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    sendHelpOnConnect = true;
  }
}

// ============================================================
// setup()
// ============================================================
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32-BT-Slave");
  SerialBT.register_callback(btCallback);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
  } else {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, espNowBroadcast, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
    Serial.println("ESP-NOW ready — broadcasting to secondary");
  }

  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(bright);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  Serial.println("Bluetooth ready. Commands:");
  Serial.println("  led on / led off");
  Serial.println("  color red / color #FF00AA");
  Serial.println("  bright 0-255");
  Serial.println("  pattern 1-4");
  Serial.println("  pov rainbow / pov stripes red,white,blue / pov off");
  Serial.println("  pov sine yellow,black / pov diamond gold,black");
  Serial.println("  pov zigzag red,white,black / pov stepped red,white,gold,black");
  Serial.println("  pov sparkle white,black / pov text HELLO");
  Serial.println("  speed 2000  (microseconds per column)");
}

// ============================================================
// loop()
// ============================================================
void loop() {
  // ------------------------------------------------------------
  // Send command help text on first connect (flag set by btCallback)
  // ------------------------------------------------------------
  if (sendHelpOnConnect) {
    sendHelpOnConnect = false;
    SerialBT.println("=== DIY Poi Commands ===");
    SerialBT.println("led on / led off");
    SerialBT.println("color <name|#RRGGBB|random>");
    SerialBT.println("  names: red green blue white yellow cyan");
    SerialBT.println("         magenta purple orange pink teal gold black");
    SerialBT.println("bright <0-255>");
    SerialBT.println("pattern 1=1Hz  2=10Hz  3=50Hz  4=pulse");
    SerialBT.println("pov rainbow");
    SerialBT.println("pov stripes red,white,blue");
    SerialBT.println("pov rings red,green,blue");
    SerialBT.println("pov gradient purple,black");
    SerialBT.println("pov sine yellow,black");
    SerialBT.println("pov diamond gold,black");
    SerialBT.println("pov zigzag red,white,black");
    SerialBT.println("pov stepped red,white,gold,black");
    SerialBT.println("pov sparkle white,black");
    SerialBT.println("pov text HELLO  (uses current color)");
    SerialBT.println("pov off");
    SerialBT.println("speed <200-100000>  (us/col, default 2000)");
    SerialBT.println("========================");
  }

  // ------------------------------------------------------------
  // Step 1: Read Bluetooth data character by character
  // ------------------------------------------------------------
  while (SerialBT.available()) {
    char c = SerialBT.read();
    if (c == '\r' || c == '\n') {
      if (line.length()) {
        esp_now_send(espNowBroadcast, (uint8_t*)line.c_str(), line.length() + 1);
        handleCommand(line);
        line = "";
      }
    } else {
      line += c;
    }
  }

  // ------------------------------------------------------------
  // Step 2: Run blink/pulse patterns (only when POV is not active)
  // POV and blink/pulse are mutually exclusive — only one runs at a time.
  // ------------------------------------------------------------
  if (!povActive) {
    if      (currentPattern == "blink1hz")  blinkPattern(1,  currentColor);
    else if (currentPattern == "blink10hz") blinkPattern(10, currentColor);
    else if (currentPattern == "blink50hz") blinkPattern(50, currentColor);
    else if (currentPattern == "pulse")     pulsePattern(currentColor);
  }

  // ------------------------------------------------------------
  // Step 3: Advance the POV column timer (only when POV is active)
  // ── NEW ──
  // povTick() checks micros() and shows the next column if enough
  // time has passed. It returns immediately if it's not time yet,
  // so the BT receive loop above never gets starved.
  // ------------------------------------------------------------
  if (povActive) {
    povTick();
  }

  // ------------------------------------------------------------
  // yield() lets the ESP32's FreeRTOS background tasks run
  // (including the Bluetooth stack). Replaces the old delay(5)
  // which would have blocked the POV column timer from hitting
  // its scheduled time slots.
  // ------------------------------------------------------------
  yield();
}

// ============================================================
// handleCommand()
// ============================================================
void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  Serial.println("CMD = [" + cmd + "]");

  // --------------- LED ON ---------------
  if (cmd == "led on") {
    povActive = false;                          // ── NEW: stop POV if running
    fill_solid(leds, NUM_LEDS, currentColor);
    FastLED.show();

  // --------------- LED OFF ---------------
  } else if (cmd == "led off") {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    currentPattern = "";
    povActive = false;                          // ── NEW: stop POV if running

  // --------------- COLOR CHANGE ---------------
  } else if (cmd.startsWith("color ")) {
    String colorName = cmd.substring(6);
    currentColor = colorFromString(colorName);
    fill_solid(leds, NUM_LEDS, currentColor);
    FastLED.show();

  // --------------- BRIGHTNESS ---------------
  } else if (cmd.startsWith("bright ")) {
    bright = cmd.substring(7).toInt();
    if (bright > 255) {
      Serial.println("255 is max value. Please lower.");
    } else {
      FastLED.setBrightness(bright);
      FastLED.show();
    }

  // --------------- BLINK / PULSE PATTERNS ---------------
  } else if (cmd.startsWith("pattern ")) {
    String pattern = cmd.substring(8);
    povActive = false;                          // ── NEW: stop POV if running

    if      (pattern == "1") currentPattern = "blink1hz";
    else if (pattern == "2") currentPattern = "blink10hz";
    else if (pattern == "3") currentPattern = "blink50hz";
    else if (pattern == "4") currentPattern = "pulse";
    else                     currentPattern = "";

    Serial.println("Pattern set to: " + currentPattern);

  // ----------------------------------------------------------------
  // ── NEW: POV PATTERNS ──
  // Format: "pov <name> [color1,color2,...]"
  // The colour list is optional — defaults are set per pattern below.
  // Colour names and hex codes use the same colorFromString() as
  // the existing "color" command.
  // ----------------------------------------------------------------
  } else if (cmd.startsWith("pov ")) {
    String sub      = cmd.substring(4);         // everything after "pov "
    int    spaceIdx = sub.indexOf(' ');

    // Split into pattern name and (optional) colour string
    String patName, colorStr;
    if (spaceIdx == -1) {
      patName  = sub;
      colorStr = "";
    } else {
      patName  = sub.substring(0, spaceIdx);
      colorStr = sub.substring(spaceIdx + 1);
    }

    // Parse up to 4 colours from comma-separated string
    CRGB    colors[4];
    uint8_t numColors = 0;
    parsePovColors(colorStr, colors, &numColors, 4);

    // Stop blink/pulse when POV takes over
    currentPattern = "";

    // ── Pattern dispatch ──────────────────────────────────
    if (patName == "off") {
      povActive = false;
      fill_solid(leds, NUM_LEDS, CRGB::Black);
      FastLED.show();
      SerialBT.println("OK: POV off");

    } else if (patName == "rainbow") {
      povRainbow(60);
      povActive = true;
      SerialBT.println("OK: POV rainbow");

    } else if (patName == "stripes") {
      // Default: red + blue if no colours given
      if (numColors == 0) { colors[0] = CRGB::Red; colors[1] = CRGB::Blue; numColors = 2; }
      povStripes(60, colors, numColors);
      povActive = true;
      SerialBT.println("OK: POV stripes");

    } else if (patName == "rings") {
      // Default: cycle through red, green, blue across LED rows
      if (numColors == 0) {
        colors[0]=CRGB::Red; colors[1]=CRGB::Green;
        colors[2]=CRGB::Blue; numColors = 3;
      }
      povRings(60, colors, numColors);
      povActive = true;
      SerialBT.println("OK: POV rings");

    } else if (patName == "gradient") {
      // Default: currentColor fading to black
      if (numColors < 1) colors[0] = currentColor;
      if (numColors < 2) colors[1] = CRGB::Black;
      povGradient(60, colors[0], colors[1]);
      povActive = true;
      SerialBT.println("OK: POV gradient");

    } else if (patName == "sine") {
      // Default: white wave on black
      if (numColors < 1) colors[0] = CRGB::White;
      if (numColors < 2) colors[1] = CRGB::Black;
      povSine(60, colors[0], colors[1]);
      povActive = true;
      SerialBT.println("OK: POV sine");

    } else if (patName == "diamond") {
      // Default: gold diamonds on black
      if (numColors < 1) colors[0] = CRGB(255, 180, 0);
      if (numColors < 2) colors[1] = CRGB::Black;
      povDiamond(60, colors[0], colors[1]);
      povActive = true;
      SerialBT.println("OK: POV diamond");

    } else if (patName == "zigzag") {
      // Default: red zigzag on black (last colour = background)
      if (numColors < 2) { colors[0] = CRGB::Red; colors[1] = CRGB::Black; numColors = 2; }
      povZigzag(60, colors, numColors);
      povActive = true;
      SerialBT.println("OK: POV zigzag");

    } else if (patName == "stepped") {
      // Default: red/white/gold on black (last colour = background)
      if (numColors < 2) {
        colors[0]=CRGB::Red; colors[1]=CRGB::White;
        colors[2]=CRGB(255,180,0); colors[3]=CRGB::Black; numColors = 4;
      }
      povStepped(60, colors, numColors);
      povActive = true;
      SerialBT.println("OK: POV stepped");

    } else if (patName == "sparkle") {
      // Default: white sparks on black
      if (numColors < 1) colors[0] = CRGB::White;
      if (numColors < 2) colors[1] = CRGB::Black;
      povSparkle(60, colors[0], colors[1]);
      povActive = true;
      SerialBT.println("OK: POV sparkle");

    } else if (patName == "text") {
      // colorStr IS the text here (no colour argument for text).
      // Text uses currentColor as foreground and black as background.
      // Example: "pov text HELLO"  (colorStr = "hello" after toLowerCase)
      if (colorStr.length() == 0) {
        SerialBT.println("ERR: pov text <word>");
      } else {
        povText(colorStr.c_str(), currentColor, CRGB::Black);
        povActive = true;
        SerialBT.println("OK: POV text -> " + colorStr);
      }

    } else {
      SerialBT.println("ERR: unknown POV pattern '" + patName + "'");
    }

  // ----------------------------------------------------------------
  // ── NEW: SPEED COMMAND ──
  // Sets the column display time in microseconds.
  // Lower value = columns advance faster = strip must spin faster.
  // Range: 200 us (very fast) to 100000 us (very slow / 10 Hz)
  // ----------------------------------------------------------------
  } else if (cmd.startsWith("speed ")) {
    uint32_t val = (uint32_t)cmd.substring(6).toInt();
    if (val >= 200 && val <= 100000) {
      povColDelayUs = val;
      SerialBT.print("OK: speed = ");
      SerialBT.print(val);
      SerialBT.println(" us/col");
    } else {
      SerialBT.println("ERR: speed must be 200-100000");
    }

  // --------------- UNKNOWN COMMAND ---------------
  } else {
    Serial.println("Unknown command");
    SerialBT.println("ERR: unknown command");
  }
}

// ============================================================
// parsePovColors()
// ============================================================
// Splits a comma-separated colour string (e.g. "red,white,blue")
// into an array of CRGB values using the existing colorFromString().
// Fills 'colors[]' and sets *count to the number of colours found.
// maxColors caps how many are parsed.
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
// colorFromString()  — unchanged from original
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
    {"gold", CRGB(255,180,0)}   // ── NEW: added gold for stepped/diamond defaults
  };

  for (auto &m : map)
    if (s == m.name) return m.c;

  return CRGB::White;
}
