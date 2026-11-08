// Live-pitch version: Adafruit KB2040 + Audio BFF (MAX98357 I2S amp) + 3 NeoPixels,
// plus the KB2040's own NeoPixel.
// The scream is stored in flash (scream_sample.h) and resampled on the fly, so its pitch,
// volume and the pixel colour follow the FSR continuously while it's held. It keeps
// screaming for as long as it's held, repeating the middle of the scream, and cuts out on release.
// Board: "Adafruit KB2040" from the Raspberry Pi Pico/RP2040 core (Earle Philhower).

#include <I2S.h>
#include <Adafruit_NeoPixel.h>
#include "scream_sample.h"

#define FSR A3
#define PIXEL_PIN 2
#define NUM_PIXELS 3
#define I2S_BCLK 6  // Audio BFF BCLK; the RP2040 puts LRCLK on the next pin, D7
#define I2S_DATA 8  // Audio BFF DAT

// Set to true and open the Serial Monitor to see raw FSR readings while tuning.
#define CALIBRATE false

// Raw 12-bit readings (0-4095), FSR fed from 3V with a 3.3k pull-down. Retune with CALIBRATE.
const int PRESS_START = 400;     // scream starts
const int PRESS_RELEASE = 280;   // scream cuts out
const int PRESS_HARDEST = 3600;  // highest pitch and full volume
// Pitch rise at the hardest squeeze (12 = one octave), and loudness at the lightest press (1.0 = full).
const float MAX_SEMITONES = 12;
const float MIN_VOLUME = 0.8;
// Pixel colour from lightest to hardest press, blended in between: light yellow, orange, dark red.
const byte PRESS_COLOR[3][3] = {{255, 190, 60}, {255, 70, 0}, {255, 0, 0}};
const byte PIXEL_BRIGHTNESS = 150;

const int OUTPUT_RATE = 44100;
// Pitch and volume glide to new targets over ~25 ms and fade out over ~5 ms on release,
// so changes don't click.
const float GLIDE = 1.0f / (0.025f * OUTPUT_RATE);
const float FADE = 1.0f / (0.005f * OUTPUT_RATE);

Adafruit_NeoPixel pixels(NUM_PIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel boardPixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);  // the KB2040's own NeoPixel
I2S i2s(OUTPUT);
unsigned long lastPrint;
uint32_t shownColor;

// Written by the sensor loop on core 0, read by the audio loop on core 1.
volatile bool pressed = false;
volatile float targetStep = 1;  // sample positions to advance per output sample
volatile float targetVolume = 1;

// ---- Core 0: FSR and pixels ----

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pixels.begin();
  pixels.setBrightness(PIXEL_BRIGHTNESS);
  pixels.show();
  boardPixel.begin();
  boardPixel.setBrightness(PIXEL_BRIGHTNESS);
  boardPixel.show();
  shownColor = 0;
}

void loop() {
  int force = readForce();
  if (CALIBRATE && millis() - lastPrint >= 50) {
    Serial.println(force);
    lastPrint = millis();
  }

  // 0.0 at the lightest press, 1.0 at the hardest. Set the targets before `pressed`
  // so a new press starts at the right pitch.
  float amount = constrain((force - PRESS_START) / float(PRESS_HARDEST - PRESS_START), 0.0f, 1.0f);
  targetStep = float(SCREAM_SAMPLE_RATE) / OUTPUT_RATE * powf(2, amount * MAX_SEMITONES / 12);
  targetVolume = MIN_VOLUME + (1 - MIN_VOLUME) * amount;

  if (!pressed && force >= PRESS_START) {
    pressed = true;
  } else if (pressed && force < PRESS_RELEASE) {
    pressed = false;
  }

  showColor(pressed ? colorFor(amount) : 0);
  delay(5);
}

// Average a few readings to steady the FSR.
int readForce() {
  int total = 0;
  for (byte i = 0; i < 4; i++) {
    total += analogRead(FSR);
  }
  return total / 4;
}

uint32_t colorFor(float amount) {
  int i = amount < 0.5f ? 0 : 1;
  float t = amount * 2 - i;
  const byte *from = PRESS_COLOR[i];
  const byte *to = PRESS_COLOR[i + 1];
  return pixels.Color(mix(from[0], to[0], t), mix(from[1], to[1], t), mix(from[2], to[2], t));
}

byte mix(byte from, byte to, float t) {
  return from + (to - from) * t;
}

void showColor(uint32_t color) {
  if (color == shownColor) return;
  pixels.fill(color);
  pixels.show();
  boardPixel.fill(color);
  boardPixel.show();
  shownColor = color;
}

// ---- Core 1: audio ----

void setup1() {
  i2s.setBCLK(I2S_BCLK);
  i2s.setDATA(I2S_DATA);
  i2s.setBitsPerSample(16);
  i2s.begin(OUTPUT_RATE);
}

// Runs once per output sample; i2s.write16() blocks until the amp is ready for more.
void loop1() {
  static bool wasPressed = false;
  static bool playing = false;
  static float position, step, volume;

  bool isPressed = pressed;
  if (isPressed && !wasPressed) {
    position = 0;
    step = targetStep;
    volume = targetVolume;
    playing = true;
  }
  wasPressed = isPressed;

  int16_t out = 0;
  if (playing) {
    step += (targetStep - step) * GLIDE;
    volume += ((isPressed ? targetVolume : 0) - volume) * (isPressed ? GLIDE : FADE);
    // Linear interpolation between the two stored samples around the current position.
    int i = (int)position;
    float sample = SCREAM_DATA[i] + (SCREAM_DATA[i + 1] - SCREAM_DATA[i]) * (position - i);
    out = sample * volume;
    position += step;
    if (position >= SCREAM_LENGTH - 1) {
      // Jump back to the loop start. scream_sample.h crossfades the seam, so it's inaudible.
      position -= SCREAM_LENGTH - 1 - SCREAM_LOOP_START;
    }
    if (!isPressed && volume < 0.001f) {
      playing = false;
    }
  }
  i2s.write16(out, out);
}
