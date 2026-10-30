/* Songs fit in one CRC-protected A/B flash object: a save is atomic. */

#include "app.h"

_Static_assert(sizeof(cb_song) <= PLAT_STORE_MAX, "song must fit one flash object");
_Static_assert(sizeof(cb_song) == 3812 && sizeof(cb_section) == 236, "song_file.py wire format");

project_t                  proj;
uint8_t                    project_dirty;
static uint8_t             hold_saves;
static cb_song load_buffer EN_POOL;

/* Programs match the instruments used by the pattern catalog. */
const char *const keyboard_names[CB_KEYBOARD_VOICES] = {
    "Piano", "Ac.Gtr", "El.Gtr", "Bass", "Strings"
};

void keyboard_apply(void) {
    static const uint8_t programs[CB_KEYBOARD_VOICES] = { 0, 25, 27, 33, 50 };
    gm_midi(0xbf, 0, 121); /* GM2 melodic bank on channel 16 */
    gm_midi(0xbf, 32, 0);
    gm_midi(0xcf, programs[proj.keyboard], 0);
}

void project_defaults(void) {
    proj = (project_t) { 0x43425031u, 0, 0, 0, 0 };
}

int project_load(void) {
    project_defaults();
    project_t p;

    if (plat_store_load(OBJ_PROJ, &p, sizeof(p)) != (int) sizeof(p) || p.magic != proj.magic ||
        p.slot >= CB_SLOTS || p.usb_full > 1 || p.midi_out > 1 || p.keyboard >= CB_KEYBOARD_VOICES)
        return -1;

    proj = p;

    return 0;
}

void project_apply(void) {
    cb_midi_out = proj.midi_out;
    keyboard_apply();
}

int song_load_slot(unsigned slot) {
    if (slot >= CB_SLOTS)
        return -1;

    if (plat_store_load(OBJ_PAT0 + slot, &load_buffer, sizeof(load_buffer)) !=
            (int) sizeof(load_buffer) ||
        !cb_validate(&load_buffer))
        return -1;

    cb_lock = 1;
    __asm__ volatile("" ::: "memory");
    arr_play(0);
    song = load_buffer;
    proj.slot = (uint8_t) slot;
    __asm__ volatile("" ::: "memory");
    cb_lock = 0;
    project_dirty = 0;

    return 0;
}

int project_save(void) {
    if (hold_saves || arr_playing || !cb_validate(&song))
        return -1;

    int rc = plat_store_save(OBJ_PAT0 + proj.slot, &song, sizeof(song));

    if (rc)
        return rc;

    rc = plat_store_save(OBJ_PROJ, &proj, sizeof(proj));

    if (!rc)
        project_dirty = 0;

    return rc;
}

void app_start(void) {
    if (song_load_slot(proj.slot)) {
        cb_default(&song);
        project_dirty = 1;
    }
}

void app_restored(void) {
    project_load();
    project_apply();
    app_start();
    ui_init();
}

void app_flush(void) {
    if (project_dirty && !arr_playing)
        project_save();
}

void app_hold_saves(int on) {
    hold_saves = (uint8_t) (on != 0);
}

int app_test(int on) {
    (void) on;
    return 0;
} /* test-session protocol is intentionally unsupported */

int app_testing(void) {
    return 0;
}
