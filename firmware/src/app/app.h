#pragma once

#include "../dsp/arr.h"
#include "../dsp/gm.h"
#include "plat.h"

#ifndef GM_VERSION
#define GM_VERSION "1.0"
#endif

typedef struct {
    uint32_t magic;
    uint8_t  slot, usb_full, midi_out, keyboard;
} project_t;

#define CB_KEYBOARD_VOICES 5
extern const char *const keyboard_names[CB_KEYBOARD_VOICES];
void keyboard_apply(void);

extern project_t proj;
extern uint8_t   project_dirty;

void             project_defaults(void);
int              project_load(void);
int              project_save(void);
void             project_apply(void);
int              song_load_slot(unsigned slot);
void             app_start(void);
void             app_restored(void);
void             app_flush(void);
void             app_hold_saves(int on);
int              app_test(int on);
int              app_testing(void);
void             ui_init(void);
void             ui_frame(void);
void             ui_input_only(void);
int              ui_dirty(void);
void             ui_say(const char *, const char *);
