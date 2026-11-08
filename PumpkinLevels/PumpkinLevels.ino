// 3-level version: Arduino Nano + DFPlayer Mini + 2 NeoPixels.
// Harder presses play a higher-pitched, louder scream and shift the pixels from
// light yellow to dark red. The scream only plays while pressed and cuts out on release.
// SD card: /mp3/0001.mp3 (level 1) to /mp3/0003.mp3 (level 3), made by tools/make_scream_mp3s.py.

#include <DFRobotDFPlayerMini.h>
#include <SoftwareSerial.h>
#include <Adafruit_NeoPixel.h>

#define FSR A3
#define PIXEL_PIN 2
#define NUM_PIXELS 2

// Set to true and open the Serial Monitor (9600 baud) to see raw FSR readings while tuning.
#define CALIBRATE false

// Raw analogRead() value needed to reach scream level 1, 2 and 3
// (FSR fed from 5V with a 3.3k pull-down). Retune with CALIBRATE.
const int LEVEL_THRESHOLD[3] = {100, 400, 750};
// Below this the press counts as released and the scream cuts out.
const int RELEASE_THRESHOLD = 70;
// DFPlayer volume (0-30) for each level.
const byte LEVEL_VOLUME[3] = {25, 27, 30};
// Pixel colour at each level, blended in between: light yellow, orange, dark red.
const byte LEVEL_COLOR[3][3] = {{255, 190, 60}, {255, 70, 0}, {255, 0, 0}};
const byte PIXEL_BRIGHTNESS = 150;

// Squeezing harder mid-scream restarts it at the higher level (it never drops back down).
const bool ESCALATE = true;
// How long a new press gets to reach its peak before a level is picked, so a hard
// slap goes straight to level 3 instead of starting at level 1.
const unsigned long ATTACK_MS = 40;
// Shortest gap between restarts while escalating, so the DFPlayer isn't flooded with commands.
const unsigned long MIN_SWITCH_MS = 150;

SoftwareSerial mySerial(8, 9);
DFRobotDFPlayerMini myDFPlayer;
Adafruit_NeoPixel pixels(NUM_PIXELS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
byte level;  // 0 = quiet, 1-3 = screaming at that level
byte currentVolume;
unsigned long levelStartedAt;
unsigned long lastPrint;
uint32_t shownColor;

void setup() {
  level = 0;
  pinMode(FSR, INPUT);
  pixels.begin();
  pixels.setBrightness(PIXEL_BRIGHTNESS);
  pixels.show();
  shownColor = 0;
  Serial.begin(9600);
  mySerial.begin(9600);

  // On battery the DFPlayer powers up together with the Nano and needs a moment to
  // read the SD card. Glow light yellow meanwhile so you can see it's switched on.
  showColor(colorFor(0));
  delay(1500);
  showColor(0);

  if (!myDFPlayer.begin(mySerial, /*isACK = */false, /*doReset = */false)) {  //Use serial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while(true){
      delay(0); // Code to compatible with ESP8266 watch dog.
    }
  }

  currentVolume = LEVEL_VOLUME[0];
  myDFPlayer.volume(currentVolume);
}

void loop() {
  int force = readForce();
  if (CALIBRATE && millis() - lastPrint >= 50) {
    Serial.println(force);
    lastPrint = millis();
  }

  if (level == 0) {
    if (force >= LEVEL_THRESHOLD[0]) {
      unsigned long pressedAt = millis();
      while (millis() - pressedAt < ATTACK_MS) {
        int reading = readForce();
        if (reading > force) force = reading;
      }
      startScream(levelFor(force));
    }
  } else if (force < RELEASE_THRESHOLD) {
    stopScream();
  } else if (ESCALATE && levelFor(force) > level && millis() - levelStartedAt >= MIN_SWITCH_MS) {
    startScream(levelFor(force));
  }

  if (level > 0) {
    // Never show a colour below the level that's playing.
    showColor(colorFor(force > LEVEL_THRESHOLD[level - 1] ? force : LEVEL_THRESHOLD[level - 1]));
  }
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

byte levelFor(int force) {
  byte n = 1;
  while (n < 3 && force >= LEVEL_THRESHOLD[n]) {
    n++;
  }
  return n;
}

void startScream(byte newLevel) {
  if (LEVEL_VOLUME[newLevel - 1] != currentVolume) {
    currentVolume = LEVEL_VOLUME[newLevel - 1];
    myDFPlayer.volume(currentVolume);
  }
  myDFPlayer.playMp3Folder(newLevel);  // /mp3/000N.mp3, picked by name rather than SD copy order
  level = newLevel;
  levelStartedAt = millis();
  Serial.print(F("Level "));
  Serial.println(level);
}

void stopScream() {
  myDFPlayer.stop();
  level = 0;
  showColor(0);
}

uint32_t colorFor(int force) {
  if (force >= LEVEL_THRESHOLD[2]) {
    return pixels.Color(LEVEL_COLOR[2][0], LEVEL_COLOR[2][1], LEVEL_COLOR[2][2]);
  }
  byte i = force >= LEVEL_THRESHOLD[1] ? 1 : 0;
  long span = LEVEL_THRESHOLD[i + 1] - LEVEL_THRESHOLD[i];
  long into = constrain(force - LEVEL_THRESHOLD[i], 0, span);
  const byte *from = LEVEL_COLOR[i];
  const byte *to = LEVEL_COLOR[i + 1];
  return pixels.Color(mix(from[0], to[0], into, span), mix(from[1], to[1], into, span), mix(from[2], to[2], into, span));
}

byte mix(byte from, byte to, long into, long span) {
  return from + (to - from) * into / span;
}

void showColor(uint32_t color) {
  if (color == shownColor) return;
  pixels.fill(color);
  pixels.show();
  shownColor = color;
}
