// ============================================================
// patterns.h
// ============================================================
// This header file defines reusable LED animation functions
// (blinkPattern and pulsePattern). These are called from
// the main loop depending on the user’s Bluetooth command.
//
// The use of "extern" allows this header to reference variables
// that are defined in main.ino (so we don't need to redefine
// CRGB leds[] or NUM_LEDS again).
// ============================================================

#ifndef PATTERNS_H
#define PATTERNS_H

#include <FastLED.h>

// "extern" tells the compiler that these variables exist somewhere else
// (in our main program), and we just want to use them here.
extern CRGB leds[];    // array holding LED color data
extern const int NUM_LEDS;   // total number of LEDs in the strip
extern int bright;     // brightness value (0–255)

// ============================================================
// BLINK PATTERN FUNCTION
// ============================================================
// Makes LEDs turn on/off at a given frequency (Hz).
// Example: blinkPattern(10, CRGB::Blue) → blink 10 times/sec
// ============================================================
void blinkPattern(float frequency, CRGB color) {
  // Period (time for one full ON/OFF cycle) = 1/frequency (in seconds)
  // We convert to milliseconds for Arduino’s "millis()" timing.
  unsigned long period = 1000.0 / frequency;

  // "static" means these variables keep their values between calls.
  // They don't reset every time this function runs.
  static unsigned long lastToggle = 0; // stores last time LEDs flipped state
  static bool state = false;           // whether LEDs are currently ON or OFF

  // Compare elapsed time to half the period (so we toggle twice per full cycle)
  if (millis() - lastToggle >= period / 2) {
    state = !state; // toggle state between true (on) and false (off)

    // Fill entire LED array with either the chosen color or off (black)
    fill_solid(leds, NUM_LEDS, state ? color : CRGB::Black);

    // Send the new LED data out to the physical strip
    FastLED.show();

    // Record the current time so we know when to toggle next
    lastToggle = millis();
  }
}

// ============================================================
// PULSE PATTERN FUNCTION
// ============================================================
// Gradually increases and decreases LED brightness in a smooth wave.
// This is done by scaling the LED color brightness up and down over time.
// ============================================================
void pulsePattern(CRGB color, int speed = 5) {
  // "static" keeps variables' values across function calls
  static int brightness = 0;   // current brightness level
  static int direction = 1;    // +1 = increasing brightness, -1 = decreasing

  // Adjust brightness by "speed" each time the function is called
  brightness += direction * speed;

  // Flip direction if we hit the upper or lower brightness limits
  if (brightness >= 255) {
    brightness = 255;
    direction = -1;  // start fading out
  } else if (brightness <= 0) {
    brightness = 0;
    direction = 1;   // start fading in
  }

  // Make a copy of the chosen color and scale its intensity
  CRGB scaled = color;
  scaled.nscale8_video(brightness); // scales brightness smoothly

  // Apply that brightness to all LEDs
  fill_solid(leds, NUM_LEDS, scaled);
  FastLED.show();

  // Delay for smoother animation speed. Without this,
  // the pulse would move too quickly.
  delay(10);
}

#endif
