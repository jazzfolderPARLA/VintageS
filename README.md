# VintageS (VintageM) – Custom firmware for the M-VAVE FM-1

A sound-focused firmware for the M-VAVE FM-1: a synthesizer module with
**no sequencer and no arpeggiator**, multiple sound banks, a simplified
two-track mixer and a streamlined panel for playing live.

Based on [Melodee](https://github.com/keremimo/melodee) by Kerem Kilic, which is
itself based on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita
(Hügelton Instruments). See *Credits* and *License* below.

---

## Features at a glance

- Multiple synth engines / sound banks selectable from the panel
- Two-track mixer on the GLO page (volume and pan per track)
- Three insert effects per track plus a shared effects chain (CHAIN or SPLIT routing)
- Preset browser with categories, favorites and a recent list
- Save presets only, together with their effect settings ⚠️
- Configurable pitch bend range (−12 to +12) per track ⚠️
- Custom boot screen
- Stereo USB audio, class-compliant USB MIDI and TRS MIDI
- Native SysEx import/export (DX7, CZ-1, Prophet)
- Return to the official M-VAVE firmware at any time

---

## Sound engines and banks

| Engine | Description |
|---|---|
| **PROPHET** | Sequential Prophet-5 Rev 4 programs: two oscillators with hard sync, Poly-Mod, LFO and noise-wheel modulation, SSI (Rev 1/2) or Curtis (Rev 3) four-pole filter, separate filter and amplifier envelopes, Vintage, glide and unison. 200 factory programs and 128 user slots (P001–P128). 8 voices per track on Standard (dual-core) builds, 5 on Single core. |
| **FM6** | Classic 6-operator FM, Dexed-based: 32 algorithms, up to 16 voices, operator, envelope, LFO and pitch-envelope pages, DX7 SysEx import, 64 native user slots (F001–F064). Selectable MODERN / MARK I / OPL engine modes. |
| **CZ-1** | Native Casio CZ-1 tones: two lines with their own eight-point pitch, timbre and volume envelopes, ring and noise modulation, detune and vibrato. 64 Casio factory presets and 128 native user slots (Z001–Z128). |
| **SID** | Commodore MOS 6581 / 8580 modelled at register level (reSID-based): three oscillators, pulse, triangle, saw and noise, ring mod, hard sync, the chip's own envelope generator and filter curves. 32 factory presets; MODEL switches between 6581 and 8580. |
| **OberVave** (bank E) ⚠️ | Engine based on [zvenson/obervave](https://github.com/zvenson/obervave) with its own sound bank. Works in single mode only (no dual or split). |
| *(other banks)* ⚠️ | Add the rest of your banks here (A–E) with their real names. |

The DRUM engine was removed to free space for the other engines.

### Selecting a bank
Long-press **SCL** to enter the sound banks, then turn **SELECT** to switch
between banks (A, B, C, D, E…). ⚠️

---

## Preset browser and favorites

Turn **PRESETS** to open the sound browser.

- **KNOB 1** chooses the list, **KNOB 2** chooses the sound.
- Lists available: **FAV** (favorites), **RECENT** (sounds browsed since
  power-on, newest first) and one list per category:
  `BASS · LEAD · PAD · KEYS · ORGAN · STRING · BRASS · WIND · PLUCK · BELL · DRUM · FX · OTHER`.
  The last chosen category is remembered with the device settings.
- **KNOB 4** chooses the engine and starts it from its **INIT** sound
  (nothing changed, effects off), which is the starting point for designing
  a sound from scratch. **Init sound** in a sound's sheet (hold OCT+) does
  the same for the current track's engine.
- Native user slots (Prophet, FM6, CZ-1) appear alongside the factory sounds
  and **can be marked as favorites**. A slot takes the category of the factory
  sound with the same name, otherwise it is guessed from the words in its name
  (BASS, PIANO, STRINGS…).
- **Knob acceleration:** slow turns move one step per detent; fast spins
  speed up to 3, 5 and then 8 steps per detent, so a half turn can sweep a
  whole 0–127 range. While a list moves fast the screen follows immediately
  and the sound loads when the knob rests. MENU > **KNOB ACCEL** OFF disables it.
- Hold **SAVE** to return to the sound you had before browsing.
- ⚠️ Add the exact gesture you use to add or remove a preset favorite.

### Scale favorites
- On the SCL page, press **SCL** again for the settings page (ROOT, FAV, QNT, TRN).
  **KNOB 2 (FAV)** saves or removes the active scale as a favorite without
  changing its tuning.
- Device favorites survive power-off. In the scale browser, **KNOB 1** can
  select the **FAV** family; an empty favorites list keeps the current scale
  and shows how to add one.

---

## Saving presets ⚠️

**SAVE** only saves presets (there are no projects in this firmware):

1. Press **SAVE**. The firmware asks whether to save in the same slot or elsewhere.
2. Search for the destination slot with the **PRESETS** knob.
3. Confirm with the assigned key.

The edited sound is saved together with its effect settings. Saving needs
playback stopped. Native slots (Prophet, FM6, CZ-1) store the tone; the 64
general slots (U01–U64) are available for the other engines.

**Import SysEx:** a valid USB SysEx dump (DX7, CZ-1, Prophet, Melodee) opens
the matching USER list. Pick the destination with KNOB 1, press OCT+ and choose
**Import here**. Occupied slots ask before being replaced. **Cancel import**
leaves everything unchanged.

**Bulk presets:** hold OCT+ and turn SELECT, KNOB 1 or KNOB 2 to select a
contiguous range of USER or STORE slots. Release, then tap OCT+ for
**Send selected** (USB SysEx), **Erase selected** (one confirmation) or
**Clear selection**.

---

## Two-track mixer (GLO page) ⚠️

Press **GLO** to open the global page:

| Control | Function |
|---|---|
| KNOB 1 | Track 1 volume (main track) |
| KNOB 2 | Track 2 volume |
| KNOB 3 | Track 1 pan (L/R) |
| KNOB 4 | Track 2 pan (L/R) |

**MASTER** sets the overall volume and **ALGORITHM** selects the active track.

---

## Pitch bend range ⚠️

For engines that don't store the bend range inside the preset, go to
**GLO > MIDI > BEND 1 / BEND 2** and set the range from **−12 to +12**
semitones with **KNOB 3** and **KNOB 4**.

---

## Effects

### Track inserts
Each track has **three serial FX inserts**, all usable at once, including
repeated effect types. Press **FX** and turn **SELECT** between FX 1, FX 2 and FX 3.

- **KNOB 1** – type: `OFF, PHASER, WIDTH, FORMANT, TAPE, SPIT, ECHO, ROOM, REVERB, CHORUS`
- **KNOB 2** – MIX
- **KNOB 3** – rate (Echo time, Room/Reverb size)
- **KNOB 4** – depth, focus, jitter, bite, repeats or decay depending on the effect

| Effect | Notes |
|---|---|
| Phaser | Four all-pass sections per side |
| Width | Processes the left side, keeps the right dry (mono collapse changes its tone) |
| Formant | Jumps among five vowel shapes |
| Tape | Drifting crossover filters with top roll-off, JITTER adds movement |
| Spit | Abruptly retuned resonant band-pass with saturation |
| Echo | Short reduced-rate delay, 10–370 ms |
| Room | Small damped four-comb room |
| Reverb | Diffused stereo eight-delay feedback tank; DEPTH extends the tail from short space to deep wash |
| Chorus | Two modulated stereo delay taps at full 44.1 kHz, no feedback |

Filter FX, Echo and Room run at 11.025 kHz and Reverb at 5.5125 kHz to save
CPU; the synths and the dry path stay at 44.1 kHz. If memory runs out, only
that slot is bypassed and **MEMORY FULL** is shown (bypass and re-enable to retry).

### Shared send chain: CHAIN vs SPLIT
Turn **SELECT** past the inserts to reach **FX SEND** and **SEND FX 1–3**.
Each track's SEND is its post-volume, post-pan amount into a shared stereo
effects section, and RETURN blends it into the main output.

- **CHAIN** (default): all three shared slots run in series as one chain,
  fed by a single send level per track.
- **SPLIT**: the three slots become separate sends. **FX SENDS > SEND 1–3**
  exposes an independent level per track for each global slot; every slot
  receives its own mix of tracks, and its return joins the dry output.
  SEND 1 shares the CHAIN send level, and SEND 2/3 levels are remembered when
  you return to CHAIN.

Changing MODE fades the return before resetting the shared effect tails.
Switching the shared chain off fades its return to silence. Routing mode and
send levels are saved with presets. One shared Reverb uses the same memory no
matter how many tracks feed it.

---

## Sound, MIDI and USB

- **Voices:** FM6 up to 16 voices, PROPHET up to 8 per track (Standard build).
- **Tuning:** A4 adjustable from 400 to 480 Hz in 1 Hz steps (default 440) on
  **GLO > GLOBAL**, with **TUNE** adding a fine offset in cents.
- **Undo:** hold **EDIT** to undo the last sound load (up to 8 levels).
- **MIDI:** USB and TRS MIDI in, routing by channel group (CH1-4, CH5-8, CH9-12,
  CH13-16) or SEL to receive all channels on the selected track. Pitch bend,
  sustain, panic, clock from internal, USB or TRS.
- **USB audio:** no drivers needed. *Melodee Out* plays the computer through the
  FM-1; *Melodee In* records the synth master in stereo (default) or 4 isolated
  mono stems (HOME-held menu > USB INPUT). 16/24-bit at 44.1 kHz; each device
  can be disabled with USB AUDIO.
- **Lights:** key and button lights show what is playing and what is available
  (menu LIGHTS sets the level).
- **Screen:** Rubik typeface, track colors, three palettes (NIGHT, DAY, CONTRAST),
  and a custom **VINTAGE MODULE** boot screen (displayed 1 second longer).
- **Clock:** runs at the boot loader's 360 MHz and never changes it.

---

## Builds, installation and recovery

- **Standard** (dual-core) and **Single core** builds. Choose Single core if your
  FM-1 crashes, freezes or restarts into update mode with Standard; it never
  starts the second core.
- Output file: **`VintageM.fwsc`**, built with GitHub Actions.
- ⚠️ Add your own installation steps here (the upstream web installer installs
  Melodee, not this firmware).
- Boot protection: an interrupted update returns to the USB update loader.
  Hold **OCT−** while switching on to request it.
- **Return to the official firmware:** use M-VAVE's own updater.
- Use at your own risk.

---

## Credits

- Melodee by Ellic Studio (Kerem Kilic, @keremimo)
- Based on Felucca by Leo Kuroshita (@kurogedelic), Hügelton Instruments
- OberVave engine: zvenson/obervave ⚠️
- FM6: msfa from Dexed (Apache-2.0); CZ-1: uPD933 model by Devin Acker in MAME (BSD-3-Clause);
  SID: after reSID by Dag Lem (GPL-2.0-or-later); Prophet-5 programs by Sequential
- Font: Rubik (SIL OFL 1.1)

## License

GPL-3.0-only. See `LICENSE` and `LICENSING.md`. "Felucca", "Hügelton Instruments",
"M-VAVE" and "FM-1" are trademarks of their respective owners; this project is
not affiliated with or endorsed by any of them.
