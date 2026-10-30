# Demo songs

Converted from the five demo records in `android/resources/assets/chordbot.db`.
The YAML files retain section order, tempo, chord spelling and duration, explicit
inversions and bass notes, track IDs, volumes, reverb and chorus values. Chord
Lord uses the first section's style throughout (`editMode=0` in the original).
The Linux SunVox project now contains samples from the original Android bank.
Envelope and effect behavior still differs; see ../../INSTRUMENTS.md.

Run from the repository root:

```sh
./linux/build/chordbot --song linux/data/demos/infanthem.yaml
./linux/build/chordbot --song linux/data/demos/machine.yaml
./linux/build/chordbot --song linux/data/demos/teal-bossa.yaml
./linux/build/chordbot --song linux/data/demos/lynchian-lounge.yaml
./linux/build/chordbot --song linux/data/demos/chord-lord.yaml
```

Press **Play** with **Song** selected to hear all sections. Disable **Loop** for
one playback. In an already running application, use **Open** and enter the
absolute path of a demo YAML file. Use **Save as** to keep an edited copy.

| Song | Tempo | Sections | Duration without effect tails |
| --- | ---: | ---: | ---: |
| Infanthem | 120 | 2 | 96.0 s |
| Machine | 80 | 2 | 78.0 s |
| Teal Bossa | 115 | 2 | 66.8 s |
| Lynchian Lounge | 74 | 3 | 77.8 s |
| Chord Lord | 115 | 3 | 18.8 s |

To regenerate the files (Python 3 and PyYAML required only for conversion):

```sh
python3 linux/tools/extract_demos.py
```

CTest loads and compiles each complete song and renders its first eight seconds
through SunVox to check for non-silent, unclipped output. These checks do not
establish audible equivalence with Android.
