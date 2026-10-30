/* ChordBot FM-1: compact catalog and streaming sequencer. */

#pragma once

#include "../app/plat.h"
#include <stdint.h>
#define CB_SECTIONS 16
#define CB_CHORDS   32
#define CB_PARTS    12
#define CB_PATTERNS 8
#define CB_SLOTS    16

enum { CB_CHORD,
       CB_BASS,
       CB_GUITAR,
       CB_DRUMS
};

typedef struct {
    int8_t      source;
    uint8_t     chromatic;
    uint16_t    len;
    const char *steps;
} cb_pattern;

typedef struct {
    const char       *name, *time;
    const cb_pattern *patterns;
    uint16_t          id, leading;
    uint8_t           category, instrument, delta, count, mode, invert, sustain, continuous;
    int8_t            transpose, volume_offset;
} cb_track;

typedef struct {
    uint16_t track;
    uint8_t  volume, flags;
} cb_part; /* bit 0 mute, bit 1 solo */

typedef struct {
    uint8_t root, type, duration;
    int8_t  bass, inversion;
} cb_chord;

typedef struct {
    char     name[22];
    uint8_t  count, nparts, reverb, chorus, repeat, reserved;
    cb_part  parts[CB_PARTS];
    cb_chord chords[CB_CHORDS];
} cb_section;

typedef struct {
    uint32_t   magic;
    uint16_t   version, tempo;
    char       name[24];
    uint8_t    count, reserved[3];
    cb_section sections[CB_SECTIONS];
} cb_song;

typedef struct {
    const char *name;
    uint8_t     count;
    int8_t      notes[12];
} cb_type;

typedef struct {
    const char *name, *time;
    uint16_t    id;
    uint8_t     count, reverb, chorus, legacy;
    cb_part     parts[CB_PARTS];
} cb_preset;

extern const cb_track    cb_tracks[];
extern const cb_type     cb_types[];
extern const cb_preset   cb_presets[];
extern const cb_song     cb_demos[];
extern const unsigned    cb_ntracks, cb_ntypes, cb_npresets, cb_ndemos;
extern const char *const cb_roots[21], *const cb_categories[8];
extern cb_song           song;
extern volatile uint8_t  cb_lock, arr_playing, cb_section_now, cb_chord_now;
extern volatile uint16_t cb_tick_now;
extern volatile uint8_t  cb_loop, cb_midi_out;

const cb_track          *cb_find_track(unsigned id);
int                      cb_root_value(int root);
int                      cb_note(const cb_track *, const cb_pattern *, const cb_chord *, const int *, const int *, int);
void                     cb_voice(const cb_chord *, int *, int *, const int *, int);
void                     cb_default(cb_song *);
int                      cb_validate(const cb_song *);
void                     cb_preset_apply(cb_section *, unsigned);
void                     arr_init(void);
void                     arr_play(int on);
void                     cb_start(unsigned section);
void                     cb_refresh(void);
void                     arr_late_offs(void);
void                     arr_block(uint32_t frames);
