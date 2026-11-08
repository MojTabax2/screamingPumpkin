# Screaming Pumpkin

A Halloween pumpkin that screams when you squeeze it. The harder you squeeze, the higher and louder
the scream gets, and the LEDs shift from light yellow through orange to dark red. It keeps screaming
for as long as you hold on, cuts out the moment you let go, and runs on a rechargeable battery.

## How it works

A force-sensitive resistor (FSR) on the pumpkin forms a voltage divider with a fixed resistor, so harder
presses give a higher analog reading. The main build stores the scream in a microcontroller's flash and
plays it through an I2S amplifier. It resamples the audio in real time, so pitch and volume track the
squeeze continuously instead of jumping between fixed levels.

## Builds

| Folder | Hardware | Behaviour | Status |
|---|---|---|---|
| [`PumpkinLivePitch/`](PumpkinLivePitch/) | Adafruit KB2040 + Audio BFF (MAX98357) | Continuous pitch (+0 to +12 semitones) and volume; scream loops while held | **Main build** |
| [`PumpkinLevels/`](PumpkinLevels/) | Arduino Nano + DFPlayer Mini | 3 force thresholds → 3 pre-pitched MP3s, volumes 25/27/30 | Reference |
| [`Pumpkin/`](Pumpkin/) | Arduino Nano + DFPlayer Mini | Original v1: one threshold, one scream | Original |

The [original v1 schematic](image%20(1).png) shows the Nano build before the v2 changes: the FSR is now fed
from the supply rail instead of 3V3, and the pull-down is now 3.3 kΩ instead of 1 MΩ.

## Hardware (main build)

| Part | Notes |
|---|---|
| Adafruit KB2040 (RP2040) | Dual-core, 8 MB flash, onboard NeoPixel |
| Adafruit Audio BFF (MAX98357 I2S amp) | Wired with jumpers, because it's shaped for a QT Py. Its microSD slot isn't used. |
| Speaker | 4–8 Ω, 3 W max |
| FSR + 3.3 kΩ pull-down | Fed from the KB2040's 3V pin (never 5 V) |
| 3× NeoPixel breakouts + 330–470 Ω data resistor | Chained on D2 |
| 470–1000 µF electrolytic capacitor | Across the Audio BFF's 5V/GND |
| Adafruit 18650 Li-ion, 2200 mAh (protected) | JST-PH lead |
| Adafruit Micro-Lipo charger | 500 mA charge jumper soldered |
| Adafruit MiniBoost 5V @ 1A (TPS61023) | Battery → 5 V |
| 3-pin slide switch | In the battery's + line |

## Wiring (main build)

| KB2040 pin | Connects to |
|---|---|
| RAW | 5 V rail (MiniBoost output, or the board's USB power during bench tests) |
| G | GND rail |
| 3V | One FSR leg |
| A3 | Other FSR leg, plus 3.3 kΩ to GND |
| D6 | Audio BFF BCLK (QT Py pad "A3") |
| D7 | Audio BFF LRCLK (QT Py pad "A2"). Must be the pin after BCLK on the RP2040. |
| D8 | Audio BFF DAT (QT Py pad "A1") |
| D2 | 330–470 Ω → NeoPixel #1 DIN → #2 → #3 |

Power chain: `18650 → Micro-Lipo (BAT) → slide switch → MiniBoost IN → 5 V rail`.
Ground runs straight from the charger to the MiniBoost, not through the switch.

[`WIRING.md`](WIRING.md) has full details for both builds: battery switch pinout, capacitor placement,
grounding, the NeoPixel 3.3 V data-line caveat, and USB bench testing.

## Firmware: `PumpkinLivePitch`

The two RP2040 cores split the work:

- **Core 0, sensor and LEDs:** every 5 ms it averages 4 FSR readings (12-bit) and turns them into a
  press amount from 0 to 1 between `PRESS_START` and `PRESS_HARDEST`. From that it sets the target pitch
  and volume and updates the NeoPixels. Hysteresis (`PRESS_START` 400 / `PRESS_RELEASE` 280) keeps the
  scream from flickering on and off at the edge.
- **Core 1, audio:** it produces one 16-bit sample per `loop1()` call at 44.1 kHz. `i2s.write16()`
  blocks when the I2S buffer is full, which sets the pace.

Audio details:

- **Sample:** 22.05 kHz, 16-bit mono, compiled into flash from `scream_sample.h` (about 90 KB). No SD card.
- **Pitch:** the read position advances `22050 / 44100 × 2^(semitones / 12)` samples per output sample, with
  linear interpolation. That's 0.5 at rest and 1.0 at +12 semitones. Since it never skips stored samples,
  shifting up doesn't add aliasing.
- **Volume:** 80% at the lightest press, 100% at the hardest.
- **Smoothing:** pitch and volume glide to new targets over about 25 ms, and the scream fades out over about
  5 ms on release, so there are no clicks.
- **Looping:** the opening of the scream plays once, then 0.20–2.06 s of it repeats while the FSR is held.
  `make_scream_header.py` builds a 100 ms equal-power crossfade into the sample and nudges the loop end to
  the best-matching point, so jumping back to the loop start is seamless.
- **LEDs:** the 3 external pixels and the KB2040's onboard pixel (GPIO 17) fade light yellow `(255,190,60)`
  → orange `(255,70,0)` → dark red `(255,0,0)` with press strength, at brightness 150/255. They're off when
  nothing is pressing.

Tuning constants at the top of the sketch:

| Constant | Default | Meaning |
|---|---|---|
| `PRESS_START` / `PRESS_RELEASE` | 400 / 280 | Reading (0–4095) that starts / stops the scream |
| `PRESS_HARDEST` | 3600 | Reading for maximum pitch and volume |
| `MAX_SEMITONES` | 12 | Pitch rise at the hardest squeeze |
| `MIN_VOLUME` | 0.8 | Loudness at the lightest press (1.0 = full) |
| `PRESS_COLOR`, `PIXEL_BRIGHTNESS` | see above | LED colours and brightness |
| `CALIBRATE` | `false` | Print raw FSR readings to the Serial Monitor |

## Firmware: `PumpkinLevels` (reference)

Arduino Nano driving a DFPlayer Mini over SoftwareSerial (D8/D9, 1 kΩ on the DFPlayer RX line). The FSR on
A3 is fed from 5 V with a 3.3 kΩ pull-down. Readings of 100 / 400 / 750 select levels 1–3, and each level
plays `/mp3/000N.mp3` (+0 / +4 / +8 semitones) at volume 25 / 27 / 30. A 40 ms attack window catches the peak
of a fast slap, and squeezing harder mid-scream restarts at the higher level. Two NeoPixels on D2 show the
same yellow-to-red gradient. The SD card contents are in [`sd_card/`](sd_card/).

## Audio tools

Both need Python 3 and `ffmpeg` on PATH. Both trim leading silence so the scream starts instantly.

```bash
# Main build: regenerate PumpkinLivePitch/scream_sample.h (auto-picks the loop section)
python tools/make_scream_header.py "scream audio/fear-scream.mp3"
python tools/make_scream_header.py "scream audio/fear-scream.mp3" --loop 0.3 1.8   # choose the loop (seconds)

# Reference build: write mp3/0001-0003.mp3 for the DFPlayer SD card
python tools/make_scream_mp3s.py "scream audio/fear-scream.mp3" sd_card
```

## Building and uploading

1. Arduino IDE 2 → Preferences → Additional boards manager URLs:
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
2. Boards Manager: install **Raspberry Pi Pico/RP2040** by Earle F. Philhower (built with 5.5.0).
3. Library Manager: install **Adafruit NeoPixel** (built with 1.15.4). For the reference build, also install
   **DFRobotDFPlayerMini** (1.0.6).
4. Select board **Adafruit KB2040** and open `PumpkinLivePitch/PumpkinLivePitch.ino`. For the first upload,
   hold the KB2040's BOOT button while plugging it in.

Switch the battery off while USB is connected.

## Calibration

The FSR's response depends heavily on how it's mounted, so calibrate it in the pumpkin:

1. Set `CALIBRATE` to `true`, upload, and open the Serial Monitor.
2. Press lightly, medium and hard.
3. Set `PRESS_START`, `PRESS_RELEASE` and `PRESS_HARDEST` from the readings.
4. Set `CALIBRATE` back to `false` and upload again.

Readings don't depend on supply voltage (the FSR and the ADC share the 3.3 V rail), so values tuned on USB
stay valid on battery. A lower pull-down (2.2 kΩ) leaves more room for hard squeezes; a higher one (10 kΩ)
makes light touches more sensitive.

## Power and battery

- The slide switch breaks the battery's + line, so nothing drains when it's off.
- **Charging:** switch off, then plug USB into the Micro-Lipo. At 500 mA, a full charge takes about 5 hours.
- **Runtime:** roughly a day of switched-on time per charge. That's an estimate, not a measurement; idle draw
  dominates, and screaming adds little.
- **Over-discharge:** the cell's built-in protection board cuts it off at about 2.5–2.75 V. The MiniBoost
  doesn't cut off earlier (it runs down to about 0.5 V input), so an empty battery shows up as the pumpkin
  going dark suddenly. Recharge it, which also resets the protection.
- **Off-season storage:** store the cell about half charged (3.7–3.8 V) somewhere cool, and top it up every few months.

## Future improvements

- **Low-battery warning and soft cutoff.** Measure the battery from the firmware so the pumpkin warns you
  before the cell's protection cuts it off:
  - **Hardware:** a 100 kΩ / 100 kΩ divider from the switched battery line (MiniBoost IN) to the free A0 pin,
    with 0.1 µF from A0 to GND. It draws about 20 µA while on and nothing when switched off.
  - **Firmware:** read A0 every few seconds while quiet (the voltage sags during screams). Blink the pixels red
    below about 3.4 V, and stop screaming below about 3.2 V.
  - The slide switch stays the real power-off.
- **Swap the KB2040 for a QT Py RP2040.** The Audio BFF would then plug straight on with no jumper wires.
  It's the same chip, so only the pin numbers in the sketch would change.

## Repository layout

```
PumpkinLivePitch/   main build: sketch + scream_sample.h (generated)
PumpkinLevels/      reference DFPlayer build
Pumpkin/            original v1 sketch
tools/              audio conversion scripts
sd_card/mp3/        DFPlayer SD card files (generated)
scream audio/       source scream recording
WIRING.md           breadboard and battery wiring for both builds
image (1).png       original v1 schematic
```
