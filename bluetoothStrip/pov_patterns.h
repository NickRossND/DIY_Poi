// ============================================================
// pov_patterns.h
// ============================================================
// Adds POV (Persistence of Vision) display capability to the
// existing LED strip controller.
//
// HOW POV WORKS:
//   The LED strip is stored as a 2D grid of columns and rows.
//   Each column = one angular slice of the spinning circle.
//   Each row    = one LED on the strip.
//
//   Think of it like a film strip on a projector reel:
//   each frame (column) is shown briefly, and when the strip
//   spins fast enough, your eye blends them into a full image.
//
//   povTick() is called every loop() and advances one column
//   per time interval (povColDelayUs). Lower delay = faster
//   column cycling (spin the strip faster to match).
//
// COMMANDS (sent from Bluetooth serial app):
//   pov rainbow               full colour wheel
//   pov stripes red,white,blue  vertical colour stripes
//   pov rings red,green,blue  one colour per LED row (concentric rings)
//   pov gradient purple,black smooth blend between two colours
//   pov sine yellow,black     sine wave on background
//   pov diamond gold,black    repeating diamond tiles
//   pov zigzag red,white,black  zigzag / chevron
//   pov stepped red,white,gold,black  Navajo-style stepped diamond
//   pov sparkle white,black   random glitter
//   pov text HELLO            text (uses current LED color + black bg)
//   pov off                   stop POV display
//   speed 2000                column delay in microseconds (default 2000)
//
// ADDING MORE LEDS LATER:
//   Change POV_MAX_LEDS to match your new strip length.
//   POV_MAX_COLS can also be increased for finer angular resolution.
// ============================================================

#ifndef POV_PATTERNS_H
#define POV_PATTERNS_H

#include <FastLED.h>

// "extern" means these variables are defined in bluetoothStrip.ino
// and we are just borrowing them here — same pattern as patterns.h.
extern CRGB leds[];
extern const int NUM_LEDS;
extern CRGB currentColor;   // used by pov text as default foreground

// ---------------------------------------------------------------
//  BUFFER DIMENSIONS
//  POV_MAX_LEDS  — increase when you add more physical LEDs
//  POV_MAX_COLS  — angular resolution (60 = 6° per slice)
// ---------------------------------------------------------------
#define POV_MAX_LEDS  5
#define POV_MAX_COLS  256    // supports up to ~63 text characters

// ---------------------------------------------------------------
//  FONT DIMENSIONS
// ---------------------------------------------------------------
#define POV_CHAR_W    3   // glyph width in columns
#define POV_CHAR_GAP  1   // blank column between characters
#define POV_CHAR_STEP 4   // total columns per character (3 + 1)

// ---------------------------------------------------------------
//  POV STATE
//  These variables are the "memory" of the POV system.
// ---------------------------------------------------------------
extern CRGB     povBuffer[POV_MAX_COLS][POV_MAX_LEDS]; // the full image grid
extern uint16_t povNumCols;      // active column count in povBuffer
extern uint32_t povColDelayUs;   // microseconds per column
extern uint16_t povCurrentCol;   // which column is being shown right now
extern uint32_t povLastColTime;  // timestamp of last column advance (micros)
extern bool     povActive;       // true = POV is running


// ============================================================
// POV ENGINE — call these from bluetoothStrip.ino
// ============================================================

// ------------------------------------------------------------
// povShowColumn()
// ------------------------------------------------------------
// Copies one column from the pixel buffer into the leds[] array
// and calls FastLED.show() to push it to the physical strip.
// This is called automatically by povTick().
// ------------------------------------------------------------
void povShowColumn(uint16_t col) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = povBuffer[col][i];
  }
  FastLED.show();
}

// ------------------------------------------------------------
// povTick()
// ------------------------------------------------------------
// Non-blocking timer — call this every loop().
// Advances to the next column only when povColDelayUs has passed.
//
// This uses micros() like a stopwatch rather than delay().
// delay() would freeze the Bluetooth receive loop;
// this approach checks the clock and moves on immediately if
// it's not time yet — like glancing at your watch rather than
// staring at it until the alarm goes off.
// ------------------------------------------------------------
void povTick() {
  uint32_t now = micros();
  if (now - povLastColTime >= povColDelayUs) {
    povLastColTime = now;
    povShowColumn(povCurrentCol);
    povCurrentCol = (povCurrentCol + 1) % povNumCols;
  }
}


// ============================================================
// INTERNAL HELPERS  (prefixed _ to signal private use)
// ============================================================

// Fill an entire column with one colour
void _povSetCol(uint16_t col, CRGB color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    povBuffer[col][i] = color;
  }
}

// Fill the entire buffer with one colour (useful as background)
void _povFillAll(uint16_t numCols, CRGB color) {
  for (uint16_t col = 0; col < numCols; col++) {
    _povSetCol(col, color);
  }
}


// ============================================================
// PATTERN: RAINBOW
// ============================================================
// Maps the full HSV colour wheel (hue 0-255) across all columns.
// Every LED in a column shares the same hue — so when the strip
// spins you see a smooth spectrum disc.
// ============================================================
void povRainbow(uint16_t numCols = 60) {
  povNumCols = numCols;
  for (uint16_t col = 0; col < numCols; col++) {
    uint8_t hue = (uint8_t)((col * 255UL) / numCols);
    CRGB c = CHSV(hue, 255, 255);
    for (int led = 0; led < NUM_LEDS; led++) {
      povBuffer[col][led] = c;
    }
  }
}


// ============================================================
// PATTERN: STRIPES
// ============================================================
// Repeating vertical colour stripes.
// Each group of stripeWidth columns gets the next colour in the
// list, then it wraps — like painting fence posts.
// ============================================================
void povStripes(uint16_t numCols, CRGB* colors, uint8_t numColors,
                uint8_t stripeWidth = 5) {
  if (numColors == 0) return;
  povNumCols = numCols;
  for (uint16_t col = 0; col < numCols; col++) {
    CRGB c = colors[(col / stripeWidth) % numColors];
    for (int led = 0; led < NUM_LEDS; led++) {
      povBuffer[col][led] = c;
    }
  }
}


// ============================================================
// PATTERN: RINGS
// ============================================================
// Assigns one colour per LED row. Since each row traces a
// fixed radius when spinning, you get concentric colour rings.
// Pass up to NUM_LEDS colours — wraps if fewer are given.
// ============================================================
void povRings(uint16_t numCols, CRGB* colors, uint8_t numColors) {
  if (numColors == 0) return;
  povNumCols = numCols;
  for (uint16_t col = 0; col < numCols; col++) {
    for (int led = 0; led < NUM_LEDS; led++) {
      povBuffer[col][led] = colors[led % numColors];
    }
  }
}


// ============================================================
// PATTERN: GRADIENT
// ============================================================
// Smooth linear blend from colors[0] to colors[1] across columns.
// FastLED's blend() interpolates two CRGB values using a 0-255 factor.
// ============================================================
void povGradient(uint16_t numCols, CRGB c1, CRGB c2) {
  povNumCols = numCols;
  for (uint16_t col = 0; col < numCols; col++) {
    // Map column index to blend factor 0-255
    uint8_t t = (uint8_t)((col * 255UL) / max((uint16_t)1, (uint16_t)(numCols - 1)));
    CRGB blended = blend(c1, c2, t);
    for (int led = 0; led < NUM_LEDS; led++) {
      povBuffer[col][led] = blended;
    }
  }
}


// ============================================================
// PATTERN: SINE WAVE
// ============================================================
// Draws a sine wave using FastLED's sin8() — a fast 8-bit version
// of sine that returns 0-255 for inputs 0-255.
//
// The wave row position is mapped from the sine output (0-255)
// to the LED row range (0 to NUM_LEDS-1).
// frequency = how many full wave cycles appear across all columns.
// ============================================================
void povSine(uint16_t numCols, CRGB waveColor, CRGB bgColor,
             float frequency = 2.0, int thickness = 1) {
  povNumCols = numCols;
  _povFillAll(numCols, bgColor);

  for (uint16_t col = 0; col < numCols; col++) {
    // sin8 input: 0-255 mapped across columns, scaled by frequency
    uint8_t angle  = (uint8_t)((col * 255UL * (uint32_t)frequency) / numCols);
    uint8_t sinVal = sin8(angle);                          // 0-255
    int row = (sinVal * (NUM_LEDS - 1)) / 255;            // map to LED row

    // Draw with optional thickness (how many LEDs wide the line is)
    for (int t = -(thickness / 2); t <= thickness / 2; t++) {
      int r = row + t;
      if (r >= 0 && r < NUM_LEDS) {
        povBuffer[col][r] = waveColor;
      }
    }
  }
}


// ============================================================
// PATTERN: DIAMOND
// ============================================================
// Uses Manhattan distance to tile repeating diamond shapes.
// Manhattan distance = |dx| + |dy|, like city blocks.
// Points inside the diamond satisfy: |dx|/half_period + |dy|/half_height <= 1
// ============================================================
void povDiamond(uint16_t numCols, CRGB fgColor, CRGB bgColor,
                uint8_t period = 10) {
  povNumCols  = numCols;
  float centerR = (NUM_LEDS - 1) / 2.0;
  float halfP   = period / 2.0;

  for (uint16_t col = 0; col < numCols; col++) {
    uint8_t colMod  = col % period;
    float   colDist = min((float)colMod, (float)(period - colMod));
    for (int led = 0; led < NUM_LEDS; led++) {
      float rowDist = fabs(led - centerR);
      bool inside = (centerR > 0) &&
                    ((colDist / halfP) + (rowDist / centerR) <= 1.0);
      povBuffer[col][led] = inside ? fgColor : bgColor;
    }
  }
}


// ============================================================
// PATTERN: ZIGZAG  (chevron)
// ============================================================
// A triangle wave bounces the line between LED row 0 and LED
// row NUM_LEDS-1. Colour cycles each time the direction flips.
// The last colour in the array is used as background.
// ============================================================
void povZigzag(uint16_t numCols, CRGB* colors, uint8_t numColors,
               int thickness = 1) {
  if (numColors == 0) return;
  CRGB bg = colors[numColors - 1];  // last colour = background
  povNumCols = numCols;
  _povFillAll(numCols, bg);

  int period = 2 * (NUM_LEDS - 1);      // columns for one full V-shape
  int nWave  = max(1, (int)numColors - 1); // number of wave colours

  for (uint16_t col = 0; col < numCols; col++) {
    int pos = col % period;
    // Triangle wave: counts up 0..NUM_LEDS-1, then back down
    int row = (pos < NUM_LEDS) ? pos : period - pos;
    int seg = (col / (NUM_LEDS - 1)) % nWave;  // which wave colour to use
    CRGB wc = colors[seg];

    for (int t = -(thickness / 2); t <= thickness / 2; t++) {
      int r = row + t;
      if (r >= 0 && r < NUM_LEDS) {
        povBuffer[col][r] = wc;
      }
    }
  }
}


// ============================================================
// PATTERN: STEPPED DIAMOND  (Navajo / Pueblo blanket inspired)
// ============================================================
// Like diamond, but with staircase edges instead of smooth diagonals.
// Uses integer Manhattan distance so the layers form clean steps.
// Each concentric layer gets the next colour in the array.
// The last colour is the background fill.
// ============================================================
void povStepped(uint16_t numCols, CRGB* colors, uint8_t numColors,
                uint8_t period = 12) {
  if (numColors == 0) return;
  CRGB bg  = colors[numColors - 1];
  int  ctr = NUM_LEDS / 2;        // centre LED row
  povNumCols = numCols;
  _povFillAll(numCols, bg);

  for (uint16_t col = 0; col < numCols; col++) {
    uint8_t colMod  = col % period;
    int     colDist = min((int)colMod, (int)(period - colMod));
    for (int led = 0; led < NUM_LEDS; led++) {
      int depth = colDist + abs(led - ctr);    // total Manhattan distance
      if (depth <= ctr) {
        int layer = ctr - depth;               // 0 = outermost, ctr = centre
        povBuffer[col][led] = colors[layer % numColors];
      }
    }
  }
}


// ============================================================
// PATTERN: SPARKLE
// ============================================================
// Randomly lights a fraction of LEDs (density).
// FastLED's random8() gives 0-255; comparing to a threshold
// is a fast way to implement probability without floating point.
// density: 0 = nothing sparkles, 255 = everything sparkles.
// ============================================================
void povSparkle(uint16_t numCols, CRGB sparkColor, CRGB bgColor,
                uint8_t density = 64) {
  povNumCols = numCols;
  for (uint16_t col = 0; col < numCols; col++) {
    for (int led = 0; led < NUM_LEDS; led++) {
      povBuffer[col][led] = (random8() < density) ? sparkColor : bgColor;
    }
  }
}


// ============================================================
// TEXT — 5×3 bitmap font
// ============================================================
// Each glyph is 5 rows tall (one per LED) and 3 columns wide.
// Each row value is a 3-bit integer:
//   bit 2 = left column, bit 1 = centre, bit 0 = right
//
// A character takes 4 columns total (3 glyph + 1 blank gap).
// At 60 columns: 60 / 4 = 15 characters per revolution.
// Longer strings auto-expand povNumCols.
// ============================================================

struct PovGlyph { char c; uint8_t rows[5]; };

const PovGlyph POV_FONT[] = {
  {' ', {0b000, 0b000, 0b000, 0b000, 0b000}},
  {'A', {0b010, 0b101, 0b111, 0b101, 0b101}},
  {'B', {0b110, 0b101, 0b110, 0b101, 0b110}},
  {'C', {0b011, 0b100, 0b100, 0b100, 0b011}},
  {'D', {0b110, 0b101, 0b101, 0b101, 0b110}},
  {'E', {0b111, 0b100, 0b110, 0b100, 0b111}},
  {'F', {0b111, 0b100, 0b110, 0b100, 0b100}},
  {'G', {0b011, 0b100, 0b101, 0b101, 0b011}},
  {'H', {0b101, 0b101, 0b111, 0b101, 0b101}},
  {'I', {0b111, 0b010, 0b010, 0b010, 0b111}},
  {'J', {0b001, 0b001, 0b001, 0b101, 0b010}},
  {'K', {0b101, 0b110, 0b100, 0b110, 0b101}},
  {'L', {0b100, 0b100, 0b100, 0b100, 0b111}},
  {'M', {0b111, 0b111, 0b101, 0b101, 0b101}},
  {'N', {0b101, 0b111, 0b111, 0b101, 0b101}},
  {'O', {0b010, 0b101, 0b101, 0b101, 0b010}},
  {'P', {0b110, 0b101, 0b110, 0b100, 0b100}},
  {'Q', {0b010, 0b101, 0b101, 0b111, 0b011}},
  {'R', {0b110, 0b101, 0b110, 0b101, 0b101}},
  {'S', {0b011, 0b100, 0b010, 0b001, 0b110}},
  {'T', {0b111, 0b010, 0b010, 0b010, 0b010}},
  {'U', {0b101, 0b101, 0b101, 0b101, 0b010}},
  {'V', {0b101, 0b101, 0b101, 0b010, 0b010}},
  {'W', {0b101, 0b101, 0b111, 0b111, 0b101}},
  {'X', {0b101, 0b101, 0b010, 0b101, 0b101}},
  {'Y', {0b101, 0b101, 0b010, 0b010, 0b010}},
  {'Z', {0b111, 0b001, 0b010, 0b100, 0b111}},
  {'0', {0b010, 0b101, 0b101, 0b101, 0b010}},
  {'1', {0b010, 0b110, 0b010, 0b010, 0b111}},
  {'2', {0b110, 0b001, 0b010, 0b100, 0b111}},
  {'3', {0b110, 0b001, 0b011, 0b001, 0b110}},
  {'4', {0b101, 0b101, 0b111, 0b001, 0b001}},
  {'5', {0b111, 0b100, 0b110, 0b001, 0b110}},
  {'6', {0b010, 0b100, 0b110, 0b101, 0b010}},
  {'7', {0b111, 0b001, 0b010, 0b010, 0b010}},
  {'8', {0b010, 0b101, 0b010, 0b101, 0b010}},
  {'9', {0b010, 0b101, 0b011, 0b001, 0b010}},
  {'!', {0b010, 0b010, 0b010, 0b000, 0b010}},
  {'?', {0b110, 0b001, 0b010, 0b000, 0b010}},
  {'.', {0b000, 0b000, 0b000, 0b000, 0b010}},
  {'-', {0b000, 0b000, 0b111, 0b000, 0b000}},
  {'+', {0b000, 0b010, 0b111, 0b010, 0b000}},
  {':', {0b000, 0b010, 0b000, 0b010, 0b000}},
};
const uint8_t POV_FONT_COUNT = sizeof(POV_FONT) / sizeof(POV_FONT[0]);

// Look up a character's glyph — returns the row data array
const uint8_t* povGetGlyph(char c) {
  c = toupper(c);
  for (uint8_t i = 0; i < POV_FONT_COUNT; i++) {
    if (POV_FONT[i].c == c) return POV_FONT[i].rows;
  }
  return POV_FONT[0].rows;   // fall back to space if not found
}

// Render text into the POV buffer
// text  : the string to display (auto-uppercased)
// fgColor : letter colour (defaults to currentColor in handleCommand)
// bgColor : background colour
void povText(const char* text, CRGB fgColor, CRGB bgColor) {
  uint8_t  len     = strlen(text);
  uint16_t numCols = (uint16_t)len * POV_CHAR_STEP;
  if (numCols > POV_MAX_COLS) numCols = POV_MAX_COLS;
  povNumCols = numCols;

  // Fill background
  _povFillAll(numCols, bgColor);

  // Draw each character
  uint16_t colOffset = 0;
  for (uint8_t ci = 0; ci < len && colOffset + POV_CHAR_W <= numCols; ci++) {
    const uint8_t* glyph = povGetGlyph(text[ci]);
    for (int row = 0; row < min(NUM_LEDS, 5); row++) {
      uint8_t bits = glyph[row];
      for (int bit = 0; bit < POV_CHAR_W; bit++) {
        // bit 2 = leftmost column, bit 0 = rightmost
        if ((bits >> (POV_CHAR_W - 1 - bit)) & 1) {
          povBuffer[colOffset + bit][row] = fgColor;
        }
      }
    }
    colOffset += POV_CHAR_STEP;
  }
}

#endif  // POV_PATTERNS_H
