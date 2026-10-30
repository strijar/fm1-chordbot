# ChordBot for M-VAVE FM-1

fm1-chordbot is a standalone accompaniment sequencer built on the Polly/Felucca
hardware platform. It combines ChordBot's recovered chord and accompaniment
catalog with Polly's compact Sonivox EAS General MIDI sound engine.

The 240 × 240 interface follows the Android ChordBot layout: a light chord grid,
chord and inversion editors, sections, an instrument mixer, and pattern selection.
The same interface runs on the FM-1 and in the Linux simulator.

## Quick start: play a demo

1. Press **GLO** to open the song library.
2. Turn **SELECT** past **New song** and the 16 user slots to the **Demo: …** entries.
3. Choose a demo and press **SEL**.
4. If an unsaved-changes prompt appears, press **SEL** again to replace the current
   song, or **HOME** to cancel. Save your current work first if you want to keep it.
5. Press **PLAY** to start; press it again to stop.

The five demos are **Chord Lord**, **Infanthem**, **Lynchian Lounge**, **Machine**,
and **Teal Bossa**. Loading a demo makes an editable working copy. Use **SAVE** to
store your version in a user slot; the built-in demo remains available.

## Features and limits

| Feature | Available |
| --- | --- |
| Accompaniment patterns | 433 original ChordBot parts |
| Style presets | 76 total: 42 current presets and 34 legacy presets |
| Chord types | 78, including Silence |
| Chord duration | 1–32 quarter notes |
| Harmony | Separate bass note, automatic voice leading, explicit inversions |
| Sections per song | Up to 16 |
| Chords per section | Up to 32 |
| Instrument parts per section | Up to 12 |
| Section repeats | 1–16 passes |
| Tempo | 30–300 BPM |
| User song slots | 16 |
| Mixer | Per-part volume, mute, solo, section presets, reverb and chorus |
| Transport | Start from a selected section; loop a section or the entire song |
| Undo / Redo | One step |
| Connections | USB/TRS MIDI input, USB MIDI output of accompaniment, USB audio to a computer |
| Computer tools | Simulator, backups, screen capture, remote panel control, YAML song exchange |

A **song** contains **sections**. Each section has a chord sequence, an instrument
mix, effects, and a repeat count. A **part** is an accompaniment pattern with its
associated sound, such as a piano arpeggio or a bass line. A **preset** replaces
the current section's set of parts and effects.

## Controls at a glance

**SELECT** is the navigation knob. **SEL** is the button that opens an editor,
confirms a choice, or opens an action menu. They are different controls.

The top bar shows the section number, song name, and tempo. The bottom bar shows
the current assignments of **K1–K4** and the keyboard instrument (`ALGO: Piano`,
for example). Turn a knob clockwise to increase a value
or move forward; turn it counterclockwise to decrease a value or move backward.
For mute, solo, and MIDI output, clockwise means **On**, counterclockwise **Off**.

| Control | Action |
| --- | --- |
| **HOME** | Open the chord grid; leave a dialog |
| **EDIT** | Open the chord editor; press again to return to the grid |
| **ARP** | Open the inversion editor; press again to return to the chord editor |
| **LFO** | Open the mixer; press again to return to the grid |
| **SEQ** | Open sections; press again to return to the grid |
| **ENV** | Open style presets |
| **FX** | Open effects and song transpose; press again to return to the grid |
| **GLO** | Open songs and demos; press again from the library to show control help |
| **PLAY** | Start at the beginning of the selected section, or stop playback |
| **REC** | Insert a chord or section, or add a part, depending on the page |
| **SAVE** | Save the working song to the selected save slot; playback must be stopped |
| **SELECT** | Move the chord cursor, select a part, or browse a list |
| **ALGORITHM** | Select the keyboard instrument |
| **OCT− / OCT+** | Select the previous / next section |
| **PRESETS** | Apply another style to the selected section; on ENV, browse without applying until SEL |
| **HOME + REC** | Undo: hold HOME, then press REC |
| **HOME + PLAY** | Redo: hold HOME, then press PLAY |
| **MASTER** | Output volume |
| Keyboard on **EDIT / ARP** | Set the selected chord's root |
| Keyboard on other pages | Play the selected instrument on MIDI channel 16 |

The printed panel labels are reused for ChordBot functions: **ARP** edits chord
inversions, **LFO** opens the mixer, and **REC** inserts items rather than recording
an audio or melody track.

### Knob assignments by page

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| **HOME** — chord grid | Root | Chord type | Duration | Tempo |
| **EDIT** — chord editor | Root | Chord type | Duration | Bass note |
| **ARP** — inversion editor | Inversion / Auto | Bass note | Duration | Tempo |
| **LFO** — mixer | Volume | Mute | Solo | Tempo |
| Instrument selection, opened with **SEL → Select instrument → SEL** from the mixer | Category | Pattern | Volume | Solo |
| **SEQ** — sections | Repeat count | Loop mode | Tempo | — |
| **ENV** — presets | Browse presets | Browse presets | Browse presets | Include legacy presets |
| **FX** — effects | Reverb | Chorus | Transpose song | MIDI output |
| **GLO** — library | Save destination slot | — | — | — |
| Rename editor | Character | Character | Character | Character |

## Create and edit a song

### Start a new song

Stop playback, press **GLO**, select **New song** with **SELECT**, and press **SEL**.
Confirm replacement if prompted. A new song starts with a Verse section containing
C Maj, A Min, F Maj, and G 7, with a default piano accompaniment.

### Edit chords

1. Press **HOME** and turn **SELECT** to choose a chord.
2. Press **EDIT**.
3. Use **K1** for the root, **K2** for the chord type, **K3** for duration, and
   **K4** for a separate bass note. The keyboard can also enter the root.
4. Press **HOME** to return to the grid.

Durations are displayed in quarter notes: **4/4** means four quarter notes and
**8/4** means eight. This is the duration of that chord, not a song-wide time
signature setting. Choose **Silence** as the chord type for a rest.

Press **REC** on the grid or chord editor to insert a copy of the selected chord
immediately after it. Edit the new chord as needed. To remove a chord, press
**SEL**, choose **Remove** with **SELECT**, and press **SEL** again. A section must
retain at least one chord.

### Play the keyboard

Turn **ALGORITHM** to choose **Piano, Ac.Gtr, El.Gtr, Bass, or Strings**.
The bottom line always shows the selected sound. These use the same sound bank
as the instrument mixer; changing the keyboard sound does not change song parts
or stop playback. The selection is saved with the device settings by **SAVE**.

**Bass** uses F1–G3 (MIDI notes 29–55), two octaves below the physical key labels.
The other sounds use F2–G4 (MIDI notes 41–67), one octave below the labels.
Changing instruments releases held keyboard notes; release and press the keys
again to play the new sound. On **EDIT / ARP**, keys still enter chord roots.

### Set inversions and slash bass

Press **ARP** to open the inversion editor. Turn **K1** through **Auto** and the
available inversions in the Low, Mid, and High registers. The displayed inversion
number is zero-based: **0** is root position, **1** is first inversion, and so on.
The available choices depend on the number of notes in the chord.

**Auto** chooses a voicing using the preceding chord. **K2** selects the bass note;
**Root** uses the chord's normal root. Changing the chord type resets its inversion
to Auto.

### Work with sections

Press **SEQ** and use **SELECT** to choose a section.
**OCT− / OCT+** also select sections from other pages.

| Task | Steps on the SEQ page |
| --- | --- |
| Insert a section | Press **REC**, or **SEL → Insert → SEL** |
| Copy a section | **SEL → Copy → SEL** |
| Remove a section | **SEL → Remove → SEL** |
| Rename a section | **SEL → Rename section → SEL** |
| Set its repeat count | Turn **K1** |

New sections are inserted after the selected section. An inserted section inherits
the current mixer and starts with one C Maj chord. **Copy** duplicates the complete
section. A song must retain at least one section.

In a rename editor, **SELECT** chooses the character position, **K1** changes the
character, and **SEL** finishes. Song names support up to 23 printable ASCII
characters; section names support up to 21. To rename the song, open the action
menu from HOME or SEQ and choose **Rename song**.

## Playback, repeats, and loops

**PLAY** starts at the beginning of the selected section, not at the selected
chord. Pressing PLAY during playback stops it. To start the whole song from its
beginning, stop playback, select section 1, then press PLAY.

To choose a loop mode, press **SEQ** and turn **K2**:

| Mode | Behavior |
| --- | --- |
| **Loop off** | Play the remaining sections and stop at the end of the song |
| **Loop section** | Keep repeating the section being played |
| **Loop song** | Continue through the song, then return to section 1 |

The mode is shown above the section list. The **Loop section / song** action in
the SEL menu also cycles through these modes. Selecting a different section while
playback is running changes the editing selection; to start playback there, stop
and restart from that section.

**K1 on SEQ is a separate setting:** it determines how many times a section plays
before the sequencer advances. For example, **x3** plays that section three times.
This count is stored with the song. The global loop mode is a playback setting
and is not stored in the song file.

Tempo, mixer settings, chord roots, chord types, inversions, and bass notes can be
changed during playback. Root entry with the keyboard also keeps playback running.
Harmony edits use the same live refresh as bass edits: held notes are released,
and subsequent pattern notes use the updated harmony without restarting the
transport. Inserting or removing chords or sections, changing chord durations,
renaming, and transposing the song stop playback.

## Instruments, presets, and effects

### Choose a style preset

1. Press **ENV**.
2. Browse with **SELECT** or **PRESETS**.
3. Press **SEL** to apply the preset to the selected section and open the mixer.

The normal list contains 42 current presets. Turn **K4** clockwise to include the
34 legacy presets, or counterclockwise to hide them. Outside the preset browser,
turning **PRESETS** applies the selected style immediately on musical editing
pages.

### Change individual parts

Press **LFO** to open the mixer, then choose a part with **SELECT**.

- **K1:** volume, from 0 to 10.
- **K2:** mute on/off.
- **K3:** solo on/off.
- **K4:** song tempo.

If one or more parts are soloed, only those parts are heard. A muted part stays
silent even when soloed. Volume 0 also silences a part.

Press **SEL → Select instrument → SEL** to choose another pattern for that part. Use **K1** for the category,
**K2** for the pattern, **K3** for volume, and **K4** for solo. Categories are
acoustic guitar, electric guitar, bass, drums, keys, piano, and synth; **All**
browses across categories. Changing category can select a new pattern immediately.
Press **SEL** again to return to the mixer.

Press **REC** in the mixer or instrument selector to add a new part, up to 12.
To delete the selected part, press **SEL → Remove part → SEL** in the mixer.
The menu also offers **Add part**, **Undo**, and **Redo**. All parts can be removed;
use **REC** to add a part to an empty mixer.

### Use one mix throughout the song

Select the section whose mix you want to use. Press **HOME**, then
**SEL → Mixer to all sections → SEL**. This copies its parts, volumes, mute/solo
settings, reverb, and chorus to every section without replacing their chords.

### Effects and transposition

Press **FX**:

- **K1** adjusts the selected section's reverb.
- **K2** adjusts its chorus.
- **K3** transposes the roots and explicit bass notes of the entire song in
  semitone steps. This changes the chord data and stops playback.
- **K4** enables or disables USB MIDI output of the accompaniment.

The page also displays voice usage and CPU load on the device.

## Save, load, and undo

### Save a song

1. Stop playback.
2. Press **GLO**.
3. Turn **K1** to choose the destination shown as **Save: N**.
4. Press **SAVE** and check for **Song saved**.

**SELECT chooses a library entry to load; K1 chooses where SAVE writes.** Merely
highlighting a slot with SELECT does not change the save destination. SAVE replaces
an existing song in its destination slot without an additional overwrite prompt.
You can press SAVE from other pages once the destination is set.

The SAVE button is lit when there are unsaved changes. Saving is manual. The song
and selected save slot are stored; the device loads that saved slot at startup.
Flash storage uses two copies and CRC checks so an interrupted song write leaves
a complete old or new version. A failed save keeps the unsaved indicator active.

### Load a saved song

Stop playback, press **GLO**, choose one of slots 1–16 with **SELECT**, and press
**SEL**. If prompted about unsaved changes, **SEL** replaces the working song and
**HOME** cancels. Loading a saved slot also selects it as the save destination.

### Undo and redo

Hold **HOME** and press **REC** to undo, or hold **HOME** and press **PLAY** to redo.
The action menu also contains **Undo** and **Redo**. There is one undo step; a new
edit replaces it. Undo/Redo stops playback and does not automatically save the
result.

## Linux simulator

From the `fm1` directory:

```sh
sh host/build_host.sh
build/host/chordbot_host --interactive
```

Click panel buttons with the mouse. Turn an encoder using the mouse wheel over
its row, or click the left **−** / right **+** half.

| Computer key | FM-1 control |
| --- | --- |
| Space | PLAY |
| Enter | SEL |
| Escape | HOME |
| Up / Down | SELECT backward / forward |
| Left / Right | OCT− / OCT+: previous / next section |
| F1–F6 | FX, SEL, ENV, LFO, EDIT, GLO |
| F7–F12 | HOME, SAVE, ARP, SEQ, PLAY, REC |

The simulator uses the same UI, sequencer, and sound engine as the firmware. It
requires X11 and uses ALSA audio through `libasound.so.2` when available.

Its default flash file is `build/host/flash.bin`. To use another file:

```sh
build/host/chordbot_host --interactive /path/to/flash.bin
```

SAVE writes to the simulator's in-memory flash; closing the window normally writes
that flash image to disk. Close the simulator normally to retain saved slots
between sessions.

## Build and install the firmware

Run from the `fm1` directory:

```sh
./build.sh
```

The installable package is **`build/chordbot.fwsc`**. Other outputs include
`chordbot.bin`, `chordbot.elf`, and `loader/ota.bin`. The development identity is
`FM-1_700`, and the USB name is `ChordBot FM-1`.

Build requirements:

- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```
- Python 3 with Pillow and PyYAML.
- GCC and X11 development files for the simulator.

The build and test scripts use `python3` by default. Set `PYTHON` to choose a
specific interpreter, either by command name or by its executable path:

```sh
PYTHON=python3.11 ./build.sh
PYTHON=python3.11 sh host/build_host.sh
PYTHON=python3.11 sh tests/run_tests.sh
```

To keep that choice for subsequent commands in the same shell:

```sh
export PYTHON=python3.11
```

This is useful when the system `python3` is an older version, such as Python 3.6,
which cannot run all of these tools. Standalone Python commands below use
`"${PYTHON:-python3}"` to honor the same setting. Directly executing a `.py` file
uses its `python3` shebang; invoke it with the selected interpreter to override it.
Child Python tools launched by the firmware builder reuse that interpreter.

Override the firmware dependency paths with `JIELI_TOOLCHAIN` and `AC79_SDK`.
The build checks image and RAM sizes, flash-driver placement in RAM, register
access, and the absence of software double-precision arithmetic.

Building does not install anything on the FM-1. To install separately:

```sh
"${PYTHON:-python3}" tools/fm1_install.py build/chordbot.fwsc
```

The update loader and boot recovery come from Polly. Holding **OCT− and OCT+** at
power-on opens panel calibration. Holding both for five seconds during operation
enters update mode.

## MIDI, audio, and computer tools

USB and TRS MIDI input play the internal synth. Enable accompaniment MIDI output
with **FX → K4**. USB audio sends the sound to a computer for recording.
MIDI clock input does not control the sequencer; set its tempo on the FM-1.

### Backups and screen capture

The USB tools require `mido` and `python-rtmidi`. Stop playback and SAVE before
making a backup to include your latest edits.

```sh
"${PYTHON:-python3}" tools/chordbot_backup.py backup songs.cbfb
"${PYTHON:-python3}" tools/chordbot_backup.py restore songs.cbfb
"${PYTHON:-python3}" tools/chordbot_mirror.py screen.png
```

Restore replaces the saved slots and asks for confirmation with **SEL** on the
FM-1. ChordBot backups use the `CBFB` identifier and are distinct from Polly
backups.

`tools/chordbot_remote.py` provides panel control over USB. The inherited Polly
`--sim` and `--fresh` options are unsupported; use the standalone host simulator
for simulation.

### Exchange songs with the Linux ChordBot version

Download a current backup, replace one slot in a new backup file, then restore it:

```sh
"${PYTHON:-python3}" tools/chordbot_backup.py backup songs.cbfb
"${PYTHON:-python3}" tools/song_file.py put songs.cbfb ../linux/data/demos/machine.yaml updated.cbfb --slot 3
"${PYTHON:-python3}" tools/chordbot_backup.py restore updated.cbfb
```

Other slots in the backup are preserved. To extract a song as Linux-compatible
YAML, including expansion of section repeat counts:

```sh
"${PYTHON:-python3}" tools/song_file.py get updated.cbfb song.yaml --slot 3 --linux
```

The converter validates device limits and reports oversized songs instead of
silently truncating them. Single-song binary conversion is also available:

```sh
"${PYTHON:-python3}" tools/song_file.py pack input.yaml output.cbs
"${PYTHON:-python3}" tools/song_file.py unpack input.cbs output.yaml --linux
```

## Tests and current limitations

Run the checks from this directory:

```sh
sh tests/run_tests.sh
```

Tests cover editing, the library, Undo/Redo, saving and write failures, changes
during playback, all five demos, note release, MIDI output recovery, and pattern
phase after mute/unmute. Storage tests interrupt writes across all 16 slots.
The independent comparison checks note-on timing, pitch, and velocity against
`../linux/src/model.c` for 433 patterns × 84 chord positions, covering all 78
chord types, inversions, bass notes, and variable durations. Linux source files
are needed only for this comparison test.

For sanitizer checks:

```sh
SANITIZE=1 sh host/build_host.sh
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  build/host/chordbot_host_asan tests/scenarios/live.cb build/tests/sanitize-live
```

The new code is checked with ASan/UBSan. Sonivox uses ASan only because its inherited
fixed-point arithmetic relies on signed shifts. Scripted simulator runs produce
WAV and PPM files. After running the tests, generate PNG previews with:

```sh
"${PYTHON:-python3}" tools/previews.py
```

The images are written to `build/previews`.

Current limits beyond the song capacities listed above:

- Audio uses Polly's GM bank, 32 EAS voices, and 22.05 kHz synthesis converted to
  44.1 kHz output. It does not reproduce the Android instrument bank.
- Original drum maps 120/121 use GM channel 10. Notes 33 and 82 are mapped to the
  supported endpoints 35 and 81. Retriggering releases deferred sustain notes to
  prevent stuck notes.
- The keyboard does not record melody tracks, and individual accompaniment
  patterns cannot be edited on the device.
- Android JSON import and on-device MIDI/WAV file export are not implemented.
  Use USB audio to record the output and the YAML tools for song exchange.

The firmware has been built and tested on the computer. Audio quality, controls,
and CPU load with maximum arrangements still need verification on a physical
FM-1.

## Source layout and credits

| Path | Contents |
| --- | --- |
| `firmware/hal`, `firmware/loader`, platform files in `firmware/src` | Polly/Felucca hardware support |
| `firmware/src/dsp/arr.c` | Streaming ChordBot sequencer |
| `firmware/src/dsp/catalog.c`, `tools/gen_catalog.py`, `data` | Catalog compiled into flash tables |
| `firmware/src/app/ui.c` | Shared device and simulator interface |
| `firmware/src/app/project.c` | Song storage |
| `host/chordbot_host.c` | Simulator platform implementation |
| `tests` | Scenarios and compatibility checks |
