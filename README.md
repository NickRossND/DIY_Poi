# DIY Poi — LED Strip Controller

Bluetooth-controlled WS2812 LED poi with POV (Persistence of Vision) patterns.
One phone connection controls both poi simultaneously via ESP-NOW.

---

## Hardware Requirements

| Component | Spec |
|---|---|
| Microcontroller | ESP32 DevKit1 (×2) |
| LED strip | WS2812 / NeoPixel, 5 LEDs per strip |
| Data pin | GPIO 2 |
| Power | 5V (USB or battery pack) |

---

## Software Dependencies

Install both of these through the Arduino IDE Library / Board Manager before flashing:

- **Board:** `esp32` by Espressif Systems — version 2.0.17 or any 2.x.xx
- **Library:** `FastLED` by Daniel Garcia

---

## Flashing the Two Units

### Primary ESP32 (connects to your phone via Bluetooth)

Open `bluetoothStrip/bluetoothStrip.ino` in Arduino IDE and flash it.

### Secondary ESP32 (receives commands wirelessly from the primary)

Open `bluetoothStrip_secondary/bluetoothStrip_secondary.ino` in Arduino IDE and flash it.

The two sketches are independent — flash them one at a time with the unit connected over USB.

---

## How the Two-Unit Link Works

```
Your phone
    │  Bluetooth Classic
    ▼
Primary ESP32  ──────────────────────────────►  Secondary ESP32
  (bluetoothStrip.ino)    ESP-NOW broadcast    (bluetoothStrip_secondary.ino)
  runs LEDs locally        ~1ms latency          runs same pattern locally
```

- You connect your Bluetooth app to the **primary only** — it is named `ESP32-BT-Slave`.
- Every command you send is forwarded to the secondary via **ESP-NOW** (ESP32's built-in peer-to-peer radio) before the primary processes it.
- Both units run the same pattern code, so they stay in sync.
- The secondary needs no Bluetooth app connection.

### Verifying the secondary is running

With the secondary connected over USB, open the Arduino Serial Monitor (115200 baud).
You should see:

```
Secondary ready — waiting for ESP-NOW commands
MAC: AA:BB:CC:DD:EE:FF
```

Once the primary is also powered on and you send a command, the secondary prints each received command:

```
CMD = [pov rainbow]
```

---

## Connecting Your Phone

1. Power on both ESP32 units.
2. On your phone, open a **Bluetooth Serial Terminal** app.
   - Android: *Serial Bluetooth Terminal* (Kai Morich) works well.
   - iOS: *Bluetooth Serial Terminal* or similar.
3. Pair with **`ESP32-BT-Slave`** (this is the primary unit).
4. Connect and start sending commands from the list below.
5. Commands are not case-sensitive.

---

## Command Reference

### LED On / Off

Turn all LEDs on (using the currently set color) or off.

```
led on
led off
```

---

### Color

Set the active color. Used by `led on`, `pattern`, and `pov text`.

```
color red
color blue
color green
color white
color yellow
color cyan
color magenta
color purple
color orange
color pink
color teal
color gold
color black
color random
color #FF00AA
```

Hex codes must be in `#RRGGBB` format.

---

### Brightness

Set the global brightness (0–255). The default is 65.

```
bright 30
bright 128
bright 255
```

---

### Blink / Pulse Patterns

Repeating animations using the currently set color. These stop automatically when a `pov` command is sent.

| Command | Effect |
|---|---|
| `pattern 1` | Blink at 1 Hz (once per second) |
| `pattern 2` | Blink at 10 Hz |
| `pattern 3` | Blink at 50 Hz |
| `pattern 4` | Smooth pulse (brightness fades in and out) |

```
pattern 1
pattern 4
```

---

### POV Patterns

POV (Persistence of Vision) patterns fill an internal image buffer that the strip cycles through as it spins. When the poi spins fast enough, your eye blends the columns into a full circular image.

All `pov` commands stop any active blink/pulse pattern and vice versa.

#### Stop POV

```
pov off
```

#### Rainbow

Full color wheel across one revolution. No colors needed.

```
pov rainbow
```

#### Stripes

Repeating vertical color bands. Pass 2–4 colors separated by commas.

```
pov stripes red,white,blue
pov stripes green,black
pov stripes cyan,magenta,yellow
```

#### Rings

Each LED row gets its own color, creating concentric rings when spinning.

```
pov rings red,green,blue
pov rings red,white,blue,gold
```

#### Gradient

Smooth linear blend from the first color to the second across one revolution.

```
pov gradient purple,black
pov gradient red,blue
pov gradient gold,black
```

#### Sine Wave

A sine wave drawn in the first color on a background of the second color.

```
pov sine yellow,black
pov sine white,blue
pov sine cyan,black
```

#### Diamond

Repeating diamond tile pattern. First color is the diamond, second is the background.

```
pov diamond gold,black
pov diamond white,purple
pov diamond red,black
```

#### Zigzag

A chevron / zigzag line that bounces between the outer and inner LEDs.
The last color in the list is always the background.

```
pov zigzag red,white,black
pov zigzag cyan,magenta,black
pov zigzag gold,black
```

#### Stepped Diamond

A Navajo-inspired stepped diamond with concentric color layers.
The last color is the background fill.

```
pov stepped red,white,gold,black
pov stepped cyan,white,black
pov stepped purple,pink,black
```

#### Sparkle

Random glitter effect. First color is the spark, second is the background.

```
pov sparkle white,black
pov sparkle gold,black
pov sparkle cyan,blue
```

#### Text

Scrolls text across one revolution using the **currently set color** as the letter color on a black background. Set your color with `color <name>` first.

```
color white
pov text HELLO

color gold
pov text POI

color cyan
pov text ND
```

---

### Speed

Sets how long each column is displayed, in microseconds. Lower = faster column cycling, which means the poi needs to spin faster to look correct. The default is 2000 µs.

| Value | Description |
|---|---|
| `speed 500` | Very fast — for a fast-spinning poi |
| `speed 2000` | Default |
| `speed 5000` | Slow — good for testing patterns while stationary |

```
speed 500
speed 2000
speed 4000
```

Valid range: 200–100000.

---

## Changing Number of LEDs

Both sketches default to **5 LEDs** on **GPIO 2**. To change this:

1. In `bluetoothStrip/bluetoothStrip.ino`:
   ```cpp
   const int NUM_LEDS = 5;   // change this
   #define DATA_PIN 2         // change this if needed
   ```

2. In `bluetoothStrip_secondary/bluetoothStrip_secondary.ino`:
   ```cpp
   const int NUM_LEDS = 5;   // change to match
   #define DATA_PIN 2
   ```

3. In **both** `pov_patterns.h` files:
   ```cpp
   #define POV_MAX_LEDS  5   // change to match NUM_LEDS
   ```

---

## Troubleshooting

**Secondary is not responding**
- Make sure both units are powered on before sending commands.
- ESP-NOW uses the WiFi radio — keep the units within ~30 m of each other.
- Open the secondary's Serial Monitor to confirm it printed `Secondary ready`.

**Bluetooth won't connect**
- Only one phone can be connected to the primary at a time.
- If a previous connection is stale, forget the device in your phone's Bluetooth settings and re-pair.

**Patterns look blurry or smeared**
- Adjust `speed` to match how fast you are spinning. Start with `speed 2000` and decrease the value if the image looks stretched.

**POV text is hard to read**
- Keep text short (4–6 characters fits well at 60 columns).
- Use a high-contrast color pair, e.g., `color white` then `pov text HELLO`.
