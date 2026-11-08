# Breadboard wiring

**We're going with Build B** (KB2040). Build A is kept for reference.

- **Build A – `PumpkinLevels`**: Arduino Nano + DFPlayer Mini + 2 NeoPixels, 3 fixed pitch levels.
- **Build B – `PumpkinLivePitch`**: Adafruit KB2040 + Audio BFF + 3 NeoPixels + the KB2040's onboard
  NeoPixel, pitch follows force continuously. This build gets the battery stage below.

The original single-level sketch is still in `Pumpkin/`.

## Bench testing on USB (before the battery)

Leave the battery stage off and let the board's USB power feed the 5 V rail. Everything else is
wired as in the tables below.

- **Build A:** Nano **5V** pin → 5 V rail. Arduino IDE board: "Arduino Nano", processor "ATmega328P".
  Switch to "ATmega328P (Old Bootloader)" if the upload fails to sync.
- **Build B:** KB2040 **RAW** → 5 V rail. Board: "Adafruit KB2040" under Raspberry Pi Pico/RP2040. For the first
  upload, hold the KB2040's BOOT button while plugging it in. Later uploads don't need it.
- **Calibrate now.** The FSR and the analog reference share the same supply, so the readings don't
  depend on the supply voltage. Thresholds tuned on USB carry over to battery unchanged.
- USB supplies about 500 mA. If the loudest scream crackles or resets the board on USB, that's the USB
  limit, not the design. Lower the volume while bench testing (e.g. `LEVEL_VOLUME` in build A). The boost
  converter can supply more current.

## Battery stage (both builds)

Parts: protected 18650 (2200 mAh, JST-PH), Adafruit Micro-Lipo charger, slide switch, and an
**Adafruit MiniBoost 5V @ 1A (TPS61023, product 4654), which you still need to buy**. The charger only
charges; it doesn't make 5 V. The Nano and DFPlayer need a steady 5 V, which a single cell (3.0–4.2 V) can't supply on its own.

```
18650 ──JST-PH──► Micro-Lipo
Micro-Lipo BAT pad ──► switch MIDDLE pin
switch OUTER pin (one side) ──► MiniBoost IN
switch OUTER pin (other side)    leave unconnected
Micro-Lipo GND pad ─────────────► MiniBoost GND      (ground never goes through the switch)
MiniBoost OUT (5V) ──► breadboard + rail
MiniBoost GND      ──► breadboard − rail
```

### Slide switch (3 pins)

A 3-pin slide switch connects its middle pin to one outer pin or the other, depending on which way
it's slid. Only the battery's + wire goes through it.

1. **Middle pin** ← Micro-Lipo **BAT** pad.
2. **One outer pin** → MiniBoost **IN**. Sliding toward this connection is ON.
3. **Other outer pin:** connect nothing. **Never wire it to GND.** Sliding to it would short the battery.
4. Before connecting the boost, check with a multimeter in continuity mode which outer pin the middle
   connects to in each position. Some switches connect to the pin on the same side as the slider,
   others to the opposite side.

- **Charging:** switch OFF, then plug USB into the charger. The charger defaults to 100 mA (about 22 h
  for 2200 mAh). Solder its 500 mA jumper closed to charge in about 5 h. The cell is fine at that rate.
- **Uploading code:** switch the battery OFF while the USB cable is in the Nano or KB2040.
- **Switch rating:** the loudest screams pull about 1–1.5 A from the cell. If the pumpkin resets or
  glitches on loud screams with a tiny breadboard switch, run the switch to the MiniBoost **EN** pin instead
  (middle pin → EN, one side → GND = off, other side → IN = on). The switch then carries no load current,
  and EN low fully disconnects the output.
- **Runtime:** roughly 30 hours switched on (about 45 mA idle at 5 V). Screaming barely matters.

## Build A: Nano + DFPlayer (`PumpkinLevels`)

| From | To |
|---|---|
| 5 V rail | Nano **5V** pin (not VIN), DFPlayer VCC (pin 1), NeoPixel 5V/+, one FSR leg |
| GND rail | Nano GND, DFPlayer GND (pins 7 **and** 10), NeoPixel GND/− |
| Other FSR leg | Nano A3, plus a 3.3 kΩ resistor to GND |
| Nano D9 | 1 kΩ → DFPlayer RX (pin 2) |
| Nano D8 | DFPlayer TX (pin 3) |
| Nano D2 | 330–470 Ω → NeoPixel #1 DIN |
| NeoPixel #1 DOUT | NeoPixel #2 DIN |
| DFPlayer SPK1 / SPK2 (pins 6, 8) | speaker |

Remove the plain LED from D6. Pixel #2's DOUT stays unconnected.

### DFPlayer capacitor

The DFPlayer's amplifier pulls current in sharp bursts that follow the audio waveform. On battery,
the boost converter plus breadboard wiring can't follow them instantly, so the 5 V dips on loud
peaks. You hear pops and crackle, and the DFPlayer or Nano can reset mid-scream. A local capacitor
supplies those bursts instead.

- **Part:** 470–1000 µF electrolytic, rated 10 V or more (16 V is typical). A 0.1 µF ceramic alongside it
  helps with hiss and buzz.
- **Where:** directly across the DFPlayer's **VCC (pin 1)** and **GND (pin 7)**, in the same breadboard
  rows as those pins. Don't put it at the far end of the power rail; the closer it is to the module, the better it works.
- **Polarity:** the long leg (+) goes to VCC. The striped side (−) goes to GND. A reversed electrolytic can pop.
- **Ground routing:** run the DFPlayer's GND wire straight back to the MiniBoost GND, and take the
  FSR's 3.3 kΩ ground from the Nano's GND pin. That keeps speaker current out of the FSR's ground, which
  would otherwise make the force reading jump during loud screams.

### NeoPixels

- They chain: data goes in on **DIN** and out on **DOUT** to the next pixel. Only pixel #1 connects to the Nano.
- The 330–470 Ω resistor goes in the data line, close to pixel #1's DIN.
- Using an RGBW pixel? Change `NEO_GRB` to `NEO_GRBW` in the sketch. If the colours come out swapped, try `NEO_RGB`.
- The pixels share the big DFPlayer capacitor if they're on the same breadboard. Add a 100 µF across
  their 5V/GND if they're on long wires.
- To use a different number of pixels, change `NUM_PIXELS` in the sketch.

## Build B: KB2040 + Audio BFF (`PumpkinLivePitch`)

The Audio BFF is shaped for a QT Py, so it won't plug onto the KB2040. Wire it with jumpers.
The BFF's pads are labelled with QT Py pin names.

| From | To |
|---|---|
| 5 V rail | KB2040 **RAW**, BFF **5V**, NeoPixel 5V/+ |
| GND rail | KB2040 G, BFF GND, NeoPixel GND/− |
| KB2040 **3V** | one FSR leg (**never 5 V**: RP2040 pins are 3.3 V only) |
| Other FSR leg | KB2040 A3, plus 3.3 kΩ to GND |
| KB2040 D6 | BFF **A3** (BCLK) |
| KB2040 D7 | BFF **A2** (LRCLK) |
| KB2040 D8 | BFF **A1** (DAT) |
| KB2040 D2 | 330–470 Ω → NeoPixel #1 DIN |
| NeoPixel #1 DOUT | NeoPixel #2 DIN |
| NeoPixel #2 DOUT | NeoPixel #3 DIN |
| BFF speaker terminal | speaker (4–8 Ω, 3 W max) |

- The RP2040 requires LRCLK on the pin right after BCLK, which is why the clocks are on D6/D7.
- Before wiring anything else, power RAW from the 5 V rail with USB unplugged and check that the
  **3V** pin reads 3.3 V.
- The KB2040's onboard NeoPixel needs no wiring. It shows the same colour as the other three.
- Put the same 470–1000 µF capacitor across the BFF's 5V/GND. If it's still on the DFPlayer board, move it.
- The BFF's microSD slot isn't used: the scream lives in the KB2040's flash.
- **NeoPixel caveat:** the KB2040 sends 3.3 V data to 5 V pixels. That usually works but isn't guaranteed.
  If they flicker or ignore commands, power the pixels from the MiniBoost **IN** side (battery voltage,
  after the switch) instead of the 5 V rail. That only works with the switch in the battery line. With
  the switch on EN, the IN side is always live and the pixels would slowly drain the battery while "off".
- Louder: solder the BFF gain jumper to 12 dB (default 9 dB).

## Audio files

Both scripts need `ffmpeg` on PATH. Both have already been run on `scream audio/fear-scream.mp3`:

- **Build A:** copy the **`mp3` folder** from `sd_card/` to the root of the SD card. The card should end up with
  `mp3/0001.mp3`, `mp3/0002.mp3` and `mp3/0003.mp3`, and nothing else is needed on it.
- **Build B:** the scream is already in `PumpkinLivePitch/scream_sample.h`. Just upload the sketch.
  While the FSR is held, the start plays once and then 0.20–2.06 s of the scream repeats seamlessly until release.

To redo them with a different recording:

- **Build A:** `python tools/make_scream_mp3s.py scream.mp3 E:/` writes `mp3/0001–0003.mp3`
  (+0, +4, +8 semitones) to the SD card at `E:`. Add `--keep-length` to raise the pitch without speeding
  it up, or use `--semitones 0 3 7` to change the steps.
- **Build B:** `python tools/make_scream_header.py scream.mp3` rewrites
  `PumpkinLivePitch/scream_sample.h`. It picks the loud, steady middle as the loop and prints it.
  Use `--loop 0.3 1.8` (seconds) to choose a different section. `--demo` writes a synthetic placeholder scream instead.

Both scripts trim leading silence, so the scream starts the instant you press.

## Calibrating

Set `#define CALIBRATE true`, upload, and watch the Serial Monitor while pressing lightly, medium and
hard on the FSR **mounted in the pumpkin**. Mounting changes the readings a lot.

- Build A: set `LEVEL_THRESHOLD` (readings 0–1023) and `RELEASE_THRESHOLD`.
- Build B: set `PRESS_START`, `PRESS_RELEASE` and `PRESS_HARDEST` (readings 0–4095).

Set it back to `false` afterwards. A lower pull-down resistor (2.2 kΩ) gives more room for hard squeezes,
and a higher one (10 kΩ) makes light touches more sensitive.
