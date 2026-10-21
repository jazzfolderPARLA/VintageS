# Vintage Module

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

![Melodee 1.0.0: the Melodee logo and six firmware screens](docs/media/melodee-1.0.0.png)


Custom firmware for the M-VAVE FM-1. This is a streamlined, "synth-only" modification focused entirely on pure sound design and live performance, removing all sequencing and arpeggiation clutter to optimize processing for the synthesizer engines.

## Core Features

* *Just Synthesizers:* Pure sound modules optimized for external MIDI control and live playing.
* *No Sequencer / No Arpeggiator:* All SEQ and ARP functionalities have been completely removed to free up resources and maximize stability.
* *Dynamic Voice Allocation:* Leverages the dual-core architecture with a global budget of *18 shared voice units* (16 if the second core is offline). Voices are dynamically stolen using a Least Recently Used (LRU) algorithm across active layers.
* *Custom Favorites List:* Easily tag, store, and recall your favorite presets across different engines for rapid access during live performance.

---

## Keyboard Modes (Split & Layer)

Reshaping the multi-timbral capability of the hardware, VintageS handles layers cleanly. *Layer B is always the track/channel immediately following the selected track (Layer A).* Because each track already runs an independent engine instance, polyphony is not split in half; each engine preserves its own full processing capabilities.

* *SINGLE:* Standard operation. Only Layer A (the selected track) sounds.
* *DUAL:* Every key triggers both Layer A and Layer B simultaneously. Layer B can use its own independent scale and octave transposition.
* *SPLIT:* Notes below the split_point route exclusively to Layer B, while notes from the split point upward route to Layer A.

### Hardware Interface & UI Controls (ui_input.c)
The physical SEQ and ARP layout has been completely re-mapped to handle the performance modes and browser utilities:
* *Short Press (SEQ):* Cycles sequentially through SINGLE ➔ DUAL LAYER ➔ SPLIT. The screen updates to display the active mode.
* *Held SEQ + Key Press:* Sets that specific key as the new split_point and automatically switches the mode to SPLIT (e.g., displaying SPLIT 60). The key marking the split point illuminates while SEQ is held. The split point persists across power cycles (the active mode resets to SINGLE).
* *Held SEQ + PRESETS Encoder:* Adjusts and assigns the sound patch for Layer B independently.
* *SEQ LED Feedback:* 
  * *SINGLE:* LED Off.
  * *DUAL:* LED Solid On.
  * *SPLIT:* LED Blinking (synchronised to the internal tempo clock).
* *ARP Button Quick Actions:* Short press skips to the next preset program with an automatic loop wrap-around (128 ➔ 1). A long press opens the comprehensive global Bank Selection Menu.

Note: Since the sequencer pages are removed, the internal tempo clock remains locked to its last saved value. If you need to adjust it, please use an external MIDI clock source.

---

## The 4 Sound Banks & Engines

VintageS organizes its synthesis architecture into 4 dedicated sound banks featuring legendary vintage hardware emulations:

### Bank 1: DX7 (FM6 Engine)
* *Native Storage:* Natively stores 128-byte packed Dexed/DX7 voices into slots F001–F064.
* *SysEx Support:* Accepts direct SysEx cartridge dumps over USB without needing external unpacking.
* *Controls:* Direct control over Master Level, Master Rate, Envelope Generator, and Detune.

### Bank 2: Prophet-5 (PROPHET Engine)
* *Factory Patches:* Includes all 200 factory programs from Sequential's v1.03 bank.
* *User Memory:* 128 native user slots (P001–P128) for custom sound design.
* *Modelling:* Features fractional state SSI (Rev 1/2) and Curtis (Rev 3) four-pole filter emulations, Poly-Mod, and hard sync.

### Bank 3: Juno-106 (JUNO Engine)
* *Complete Emulation:* Ported and integrated from the FM-106 architecture.
* *Presets:* Includes *all official factory presets* pre-loaded and ready to play.

### Bank 4: CZ & SID Dual Engine
* *Casio CZ-1:* Includes 64 classic native CZ preset tones with Phase Distortion synthesis continuously evaluated via FPU.
* *Commodore SID:* Register-level emulation of the MOS 6581/8580 chips featuring 32 factory presets, pulse width modulation, combined waveforms, and filter curves.

---

## Technical Specifications & Polyphony

Voice allocation is unit-based. Depending on the complexity of the synthesis engine, each active voice consumes a different amount of units from the global budget (18 units total):

| Engine / Bank | Max Notes (SINGLE) | Unit Cost per Voice | Notes & Features |
| :--- | :---: | :---: | :--- |
| *DX7* (Bank 1) | 16 | 1 unit | 16-voice max capacity. |
| *Prophet-5* (Bank 2) | 8 | 2 units | Drops to 5 notes (3 units/voice) if CPU1 is offline. |
| *Juno-106* (Bank 3) | 6 | 2 units | Full factory bank integrated. |
| *CZ-1* (Bank 4) | 8 | 2 units | 8-point DCO/DCW/DCA envelopes. |
| *SID* (Bank 4) | 4 | 2 units | Register-level emu (reSID based). |

### Polyphony in DUAL Mode (Examples)
Since keys trigger both layers simultaneously, the total maximum notes depend on the combined unit cost of both active engines:
* *DX7 + DX7 (Default):* 2 units per note = *9 max notes*
* *DX7 + Prophet-5* / *DX7 + Juno-106:* 3 units per note = *6 max notes*
* *Prophet-5 + Juno-106* / *Juno-106 + Juno-106:* 4 units per note = *4 max notes*

---

## Installation

### Method 1: Web Installer (Recommended)
1. Connect your M-VAVE FM-1 to your computer via *USB*.
2. Open the web installer using *Google Chrome* or *Microsoft Edge*.
3. Choose the *Synth only* build option.
4. Press *Install* and wait for the verification to finish.

### Method 2: Terminal Installation
For developers and advanced users, install directly from the terminal via Python:
bash
python tools/fm1_install.py


Note: Use at your own risk. You can safely return to the official stock firmware at any time using M-VAVE's official updater or by selecting "Return to official V15" in the web installer.

## Credits & License

* *Developer:* Developed and modified by Ellic Studio / jazzfolderPARLA.
* *Based on:* Modified from [Melodee](https://github.com) by Kerem Kilic and [Felucca](https://github.com) by Leo Kuroshita (Hügelton Instruments).
* *License:* Free software under the *GPL-3.0-only* license.

---
M-VAVE and FM-1 are trademarks of their respective owners. VintageS is an independent open-source project and is not affiliated with or endorsed by them.
