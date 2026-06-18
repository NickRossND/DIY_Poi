// ============================================================
// main.ino
// ============================================================
// This program controls a WS2812 LED strip using Bluetooth commands.
// The ESP32 receives text commands like “led on”, “pattern 1”,
// “color red”, etc., and performs corresponding LED animations.
//
// Architecture:
//   • main.ino handles setup, Bluetooth communication, and parsing.
//   • patterns.h defines reusable animation effects.
// 
// Dependencies:
// esp32 by Espressif Systems version 2.0.17 or 2.x.xx to use classic Bluetooth
// ============================================================

#include "BluetoothSerial.h"  // Enables Bluetooth Serial communication
#include <FastLED.h>          // Library for controlling WS2812 LEDs
#include "patterns.h"         // Include our custom LED pattern functions

// #if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
// #error Bluetooth is not enabled! Please run `make menuconfig` to enable it.
// #endif

// ------------------- Hardware Configuration -------------------
const int NUM_LEDS = 5;      // Number of LEDs on your WS2812 strip use const int so patterns.h file can recognize it
#define DATA_PIN 2     // GPIO pin connected to LED data line

// ------------------- Global Variables -------------------
int bright = 65;         // Default brightness level (0–255)
BluetoothSerial SerialBT; // Bluetooth serial object for communication
CRGB leds[NUM_LEDS];      // Array of LED color data
String line;               // Buffer to store incoming Bluetooth command
String currentPattern = ""; // Which animation pattern is currently active
CRGB currentColor = CRGB::Red; // Default LED color (red)

// ============================================================
// setup()
// ============================================================
// Runs once at startup. Initializes Serial, Bluetooth, and LED strip.
// ============================================================
void setup() {
  Serial.begin(115200);          // USB serial for debugging
  SerialBT.begin("ESP32-BT-Slave"); // Name that appears over Bluetooth

  // Initialize FastLED: specify LED type, data pin, and color order
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);

  // Set overall brightness (acts as a global dimmer)
  FastLED.setBrightness(bright);

  // Turn all LEDs off initially
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  Serial.println("Bluetooth ready. Try commands like:");
  Serial.println("  led on / led off");
  Serial.println("  color red / color #FF00AA");
  Serial.println("  pattern 1–4");
}

// ============================================================
// loop()
// ============================================================
// Runs repeatedly after setup(). Handles incoming Bluetooth data
// and continuously updates LED patterns (non-blocking).
// ============================================================
void loop() {
  // ------------------------------------------------------------
  // Step 1: Read Bluetooth data character-by-character
  // ------------------------------------------------------------
  while (SerialBT.available()) {
    char c = SerialBT.read();

    // A newline or carriage return indicates the end of a command
    if (c == '\r' || c == '\n') {
      if (line.length()) {        // Only process if buffer isn't empty
        handleCommand(line);      // Parse and execute command
        line = "";                // Clear the buffer
      }
    } else {
      // Otherwise, keep appending characters to the command buffer
      line += c;
    }
  }

  // ------------------------------------------------------------
  // Step 2: Continuously run the current pattern
  // ------------------------------------------------------------
  // Each pattern function handles its own timing logic using millis().
  if (currentPattern == "blink1hz")      blinkPattern(1, currentColor);
  else if (currentPattern == "blink10hz") blinkPattern(10, currentColor);
  else if (currentPattern == "blink50hz") blinkPattern(50, currentColor);
  else if (currentPattern == "pulse")    pulsePattern(currentColor);

  // A short delay helps stabilize Bluetooth and timing loops.
  delay(5);
}

// ============================================================
// handleCommand()
// ============================================================
// This function parses a complete command string (e.g. "pattern 1")
// and executes the corresponding LED behavior.
// ============================================================
void handleCommand(String cmd) {
  cmd.trim();          // Remove leading/trailing spaces or \r\n
  cmd.toLowerCase();   // Make command case-insensitive
  Serial.println("CMD = [" + cmd + "]");

  // --------------- LED ON ---------------
  if (cmd == "led on") {
    fill_solid(leds, NUM_LEDS, currentColor);
    FastLED.show();

  // --------------- LED OFF ---------------
  } else if (cmd == "led off") {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    currentPattern = "";  // stop any active pattern

  // --------------- COLOR CHANGE ---------------
  } else if (cmd.startsWith("color ")) {
    // Extract the substring after "color "
    String colorName = cmd.substring(6);
    currentColor = colorFromString(colorName);
    fill_solid(leds, NUM_LEDS, currentColor);
    FastLED.show();

  // --------------- CHANGE BRIGHTNESS ---------------
  } else if (cmd.startsWith("bright ")) {
      String brightness = cmd.substring(7);                   // input integer to set brightness value
      bright = brightness.toInt();
      if (bright > 255) {
        Serial.println("255 is max value. Please lower.");
      
      } else{ 
        FastLED.setBrightness(bright); // ← replaces current brightness
        FastLED.show(); }

  // --------------- PATTERN SELECT ---------------
  } else if (cmd.startsWith("pattern ")) {
    String pattern = cmd.substring(8); // extract everything after "pattern "

    // Match user input to defined patterns
    if (pattern == "1") currentPattern = "blink1hz";
    else if (pattern == "2") currentPattern = "blink10hz";
    else if (pattern == "3") currentPattern = "blink50hz";
    else if (pattern == "4") currentPattern = "pulse";
    else currentPattern = ""; // stop pattern if invalid number

    Serial.println("Pattern set to: " + currentPattern);

  // --------------- UNKNOWN COMMAND ---------------
  } else {
    Serial.println("Unknown command");
  }
}

// ============================================================
// colorFromString()
// ============================================================
// Converts text like "red", "blue", "#FFAA33", or "random"
// into an actual CRGB color object recognized by FastLED.
// ============================================================
CRGB colorFromString(String s) {
  s.trim(); s.toLowerCase();

  // Generate a random color if user types "random"
  if (s == "random" || s == "rand") return CHSV(random8(), 255, 255);

  // "off" or "black" both mean turn LEDs off
  if (s == "off" || s == "black") return CRGB::Black;

  // Hex color format "#RRGGBB"
  if (s.length() == 7 && s[0] == '#') {
    // Convert string to integer (base 16)
    uint32_t v = strtoul(s.c_str() + 1, nullptr, 16);
    return CRGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
  }

  // Common color name lookup table
  struct { const char* name; CRGB c; } map[] = {
    {"red",CRGB::Red},{"green",CRGB::Green},{"blue",CRGB::Blue},
    {"white",CRGB::White},{"yellow",CRGB::Yellow},{"cyan",CRGB::Cyan},
    {"magenta",CRGB::Magenta},{"purple",CRGB::Purple},{"orange",CRGB::Orange},
    {"pink", CRGB(255,105,180)}, {"teal", CRGB(0,128,128)}
  };

  // Iterate through the list and find a match
  for (auto &m : map)
    if (s == m.name) return m.c;

  // Default fallback if color name isn't recognized
  return CRGB::White;
}
