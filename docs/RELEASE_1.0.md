**LEMONADE SANDWICH 1.0** is the first release of LEMONADE SANDWICH, generative firmware for the M-VAVE FM-1 with a one-button piece machine. It is built on [Tessera 3.7.1](https://github.com/Rastapopolix/tessera/releases/tag/v3.7.1): everything Tessera does is still here, and the **REC button rolls a whole new piece** across all four tracks in one press.

### 🎲 The rolls
Pick one in **MENU > REC BUTTON** (the top row: hold **HOME** for the menu, **KNOB 2** picks the roll, hold **HOME** again to leave), then press **REC**. Press it again for another; hold **REC** to go back to how things were before the roll, and hold again to get the roll back. Rolls are in key with the white keys, and dry, ready for your own reverb and delay.

| Roll | What it makes |
|---|---|
| **FALL** | Warm ambient on TRIO's triangles: a drone, chords and two falling melodies, 64 to 80 BPM |
| **TIDE** | A slow sub groove, falling leaves, an ORACLE melody, a triangle bed and a quiet noise |
| **WOZZA** | Minimal piano and synth, slow, off the grid, tape-worn |
| **ORBIT** | Bouncing, orbiting patterns, fast and musical, 100 to 120 BPM |
| **LOOPS** | Four sparse loops of different lengths drifting apart |
| **HALO** | Warm and hazy, after Arvo Pärt: a slow line, a soft voice on its triad, a canon below |
| **MIRROR** | HALO's mirror: glassy and digital, microtonal now and then |
| **PHASE** | After Steve Reich: one bright cell on two tracks, one moving a step ahead every few bars |
| **CRATE** | An old funk or soul record to sample, no drums: keys, bass, a hook, strings or a choir |
| **COSMIC** | Synthwave at 108 to 118 BPM: a composed minor phrase, driving bass, wide pads, a lead, an arp |
| **STATIC** | A wall of atonal noise in four held layers over a brooding Reese bass |
| **FORGE** | Dark polyrhythmic techno at about 130 BPM: a deep kick, never four on the floor |
| **CHAOS** | FORGE noisier and more synthetic: crushed FM kicks, metal and noise |
| **FRACTURED** | Glitchy, broken beats after Autechre under a warm pad, 130 to 150 BPM |
| **RND** | A whole new piece, random and strange |

**MENU > DRIFT** (the row under REC BUTTON) set to **SLOW** or **FAST** keeps a roll changing while it plays: new riffs and melodies, parts dropping out and coming back. **OFF** keeps it exactly as rolled. A roll is an ordinary Tessera project: edit any part of it, and **hold SAVE** to keep it.

### 🎛️ Sound
* Three engines: **TRIO** (three oscillators and a filter), **ORACLE** (after the Prophet-5) and **MACRO**, Mutable Instruments Braids: textures, voices, FM, noise, analog shapes (CSAW, FOLD, SWARM, MORPH, SYNC, SAW3) and drums (KICK, DRUM, CYMB).
* Effects: TAPE, SPIT, ECHO, REVERB, FILTER and DRIVE.

### 📦 Installation
1. Download **`lemonade-sandwich-1.0.fwsc`** below (**`lemonade-sandwich-1.0-1core.fwsc`** is a one-core fallback, for a unit the normal one doesn't run well on).
2. Connect the FM-1 by USB and open the [Tessera web installer](https://rastapopolix.github.io/tessera/webapp/installer/) in Chrome or Edge.
3. Open **Test build from this computer**, choose the file and press **Install test build**. Keep the tab open and the FM-1 connected until it says it's done.

It installs over M-VAVE's official firmware, Tessera, or an earlier LEMONADE SANDWICH build. The installer can also put the official M-VAVE firmware back.

### ⚠️ Good to know
* **To fit the FM-1's flash**, this build leaves out the FM6, PHYS and DRUM engines, MACRO's wavetables, the PHASER, WIDTH, FORMANT, ROOM and CHORUS effects, and Tessera's screen reader. Projects that use them still load: a track on a missing engine plays TRIO's first sound, and a missing effect plays as off.
* **Newer FM-1s** (some white units with the M-VAVE logo) have a different flash chip, the UCUN UC25HQ80, which this release supports. A unit running an older LEMONADE SANDWICH or Tessera build that doesn't know that chip won't save settings or start an update ("the device stopped after 0 requests"). It can be brought up to date with Tessera's USB recovery tool, [tools/fm1_recover.py](https://github.com/Rastapopolix/tessera/blob/main/tools/fm1_recover.py), using `--write --allow-untested-flash` (see Tessera's [recovery guide](https://github.com/Rastapopolix/tessera/blob/main/docs/RECOVERY.md)).
* The full manual is [MANUAL.md](https://github.com/TheGrizard/LemonadeSandwich/blob/main/MANUAL.md), with a section on every roll; [CHEATSHEET.md](https://github.com/TheGrizard/LemonadeSandwich/blob/main/CHEATSHEET.md) lists every button and knob.

### 🙏 Credits
Built on [Tessera](https://github.com/Rastapopolix/tessera) by Theo Dainis, a fork of Melodee (Kerem Kilic, Ellic Studio), with portions from Felucca (Leo Kuroshita, Hügelton Instruments). MACRO uses Mutable Instruments Braids by Émilie Gillet (MIT). LEMONADE SANDWICH is by Thomas Bradley ([TheGrizard](https://github.com/TheGrizard)) and is GPL-3.0-only, like Tessera.
