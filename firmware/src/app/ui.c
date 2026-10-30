/* ChordBot's Android information layout adapted to a 240x240 physical panel.
 * Included after gfx.c in both the device and host unity builds. */

#include "app.h"

enum {
    P_MAIN,
    P_CHORD,
    P_INV,
    P_MIX,
    P_INSTR,
    P_SECT,
    P_PRESET,
    P_FX,
    P_LIB,
    P_MENU,
    P_NAME,
    P_CONFIRM,
    P_HELP
};

static struct {
    int         page, section, chord, part, preset, category;
    int         library, menu, back, namepos, namesong, confirm, legacy;
    int         octave, transpose;
    uint32_t    buttons, keys, notice_until;
    uint32_t    live_keys;
    uint8_t     live_notes[NKEYS];
    char        notice[48];
} ui;

static uint8_t           ui_redraw, undo_valid, redo_valid;
static cb_song undo_song EN_POOL;

static const uint16_t    BG = RGB(237, 238, 232), CELL = RGB(255, 255, 244), INK = RGB(70, 79, 75),
                         BAR = RGB(47, 79, 94), SELECTED = RGB(168, 191, 186),
                         RULE = RGB(197, 203, 194), DARK = RGB(41, 43, 42);

static const uint8_t pitch_roots[12] = { 1, 2, 4, 6, 7, 10, 11, 13, 15, 16, 18, 19 };

static int bounded(int x, int lo, int hi) {
    return x < lo ? lo : x > hi ? hi : x;
}

static void append(char *dst, const char *src, unsigned n) {
    unsigned len = str_len(dst);

    if (len < n)
        str_cpy(dst + len, src, n - len);
}

static void number(char *dst, int n, unsigned max) {
    char b[16];

    fmt_int(b, n);
    append(dst, b, max);
}

static cb_section *current(void) {
    return &song.sections[ui.section];
}

static cb_chord *chord(void) {
    return &current()->chords[ui.chord];
}

static void fix_cursor(void) {
    ui.section = bounded(ui.section, 0, song.count - 1);
    ui.chord = bounded(ui.chord, 0, current()->count - 1);
    ui.part = bounded(ui.part, 0, current()->nparts ? current()->nparts - 1 : 0);
}

static void edit_begin(int structural) {
    cb_lock = 1;
    __asm__ volatile("" ::: "memory");
    undo_song = song;
    undo_valid = 1;
    redo_valid = 0;

    if (structural)
        arr_play(0);
}

static void edit_end(void) {
    project_dirty = 1;
    cb_refresh();
    __asm__ volatile("" ::: "memory");
    cb_lock = 0;
    fix_cursor();
    ui_redraw = 1;
}

static void undo(int redo) {
    if (redo ? !redo_valid : !undo_valid) {
        ui_say("Nothing to undo / redo", 0);
        return;
    }

    cb_lock = 1;
    arr_play(0);

    unsigned i;
    uint8_t *a = (uint8_t *) &song, *b = (uint8_t *) &undo_song;

    for (i = 0; i < sizeof(song); i++) {
        uint8_t t = a[i];
        a[i] = b[i];
        b[i] = t;
    }

    undo_valid = (uint8_t) redo;
    redo_valid = (uint8_t) !redo;
    project_dirty = 1;
    cb_lock = 0;
    fix_cursor();
    ui_redraw = 1;
}

void ui_say(const char *a, const char *b) {
    str_cpy(ui.notice, a, sizeof(ui.notice));

    if (b) {
        append(ui.notice, " ", sizeof(ui.notice));
        append(ui.notice, b, sizeof(ui.notice));
    }

    ui.notice_until = plat_ms() + 2000;
    ui_redraw = 1;
}

static void keyboard_release(void) {
    for (int i = 0; i < NKEYS; i++)
        if (ui.live_keys >> i & 1)
            gm_midi(0x8f, ui.live_notes[i], 0);
    ui.live_keys = 0;
}

void ui_init(void) {
    keyboard_release();
    memset(&ui, 0, sizeof(ui));
    ui_redraw = 1;
    undo_valid = redo_valid = 0;
}

int ui_dirty(void) {
    return ui_redraw;
}

static void setpage(int page) {
    ui.page = page;
    ui_redraw = 1;
}

static int track_index(unsigned id) {
    unsigned i;

    for (i = 0; i < cb_ntracks; i++)
        if (cb_tracks[i].id == id)
            return (int) i;

    return 0;
}

static void track_move(int delta) {
    cb_section *s = current();

    if (!s->nparts)
        return;

    int i = track_index(s->parts[ui.part].track), dir = delta < 0 ? -1 : 1;

    while (delta) {
        int next = i;

        do {
            next += dir;
        } while (next >= 0 && next < (int) cb_ntracks && ui.category &&
                 cb_tracks[next].category != ui.category);

        if (next < 0 || next >= (int) cb_ntracks)
            break;

        i = next;
        delta -= dir;
    }

    s->parts[ui.part].track = cb_tracks[i].id;
}

static void insert_item(int copy) {
    cb_section *s = current();
    int         i;

    if (ui.page == P_SECT) {
        if (song.count == CB_SECTIONS) {
            ui_say("16 sections maximum", 0);
            return;
        }

        edit_begin(1);

        for (i = song.count; i > ui.section + 1; i--)
            song.sections[i] = song.sections[i - 1];

        song.sections[ui.section + 1] = *s;
        song.count++;
        ui.section++;

        if (!copy) {
            s = current();
            str_cpy(s->name, "Section", 16);
            s->count = 1;
            s->chords[0] = (cb_chord) { 1, 0, 4, -1, -1 };
            s->repeat = 1;
        }
    } else if (ui.page == P_MIX || ui.page == P_INSTR) {
        if (s->nparts == CB_PARTS) {
            ui_say("12 parts maximum", 0);
            return;
        }

        edit_begin(0);
        s->parts[s->nparts] = (cb_part) { cb_tracks[0].id, 5, 0 };
        ui.part = s->nparts++;
        ui.category = 0;
        setpage(P_INSTR);
    } else {
        if (s->count == CB_CHORDS) {
            ui_say("32 chords maximum", 0);
            return;
        }

        edit_begin(1);

        for (i = s->count; i > ui.chord + 1; i--)
            s->chords[i] = s->chords[i - 1];

        s->chords[ui.chord + 1] = s->chords[ui.chord];
        s->count++;
        ui.chord++;
    }

    edit_end();
}

static void delete_item(void) {
    cb_section *s = current();
    int         i;

    if (ui.page == P_SECT) {
        if (song.count == 1) {
            ui_say("Keep one section", 0);
            return;
        }

        edit_begin(1);

        for (i = ui.section; i < song.count - 1; i++)
            song.sections[i] = song.sections[i + 1];

        song.count--;
    } else if (ui.page == P_MIX || ui.page == P_INSTR) {
        if (!s->nparts)
            return;

        edit_begin(0);

        for (i = ui.part; i < s->nparts - 1; i++)
            s->parts[i] = s->parts[i + 1];

        s->nparts--;
    } else {
        if (s->count == 1) {
            ui_say("Keep one chord", 0);
            return;
        }

        edit_begin(1);

        for (i = ui.chord; i < s->count - 1; i++)
            s->chords[i] = s->chords[i + 1];

        s->count--;
    }

    edit_end();
}

static void save(void) {
    if (arr_playing) {
        ui_say("Stop before saving", 0);
        return;
    }

    ui_say(project_save() ? "Save failed" : "Song saved", 0);
}

static void load_choice(void) {
    if (arr_playing) {
        ui_say("Stop before loading", 0);
        return;
    }

    if (ui.library == 0) {
        edit_begin(1);
        cb_default(&song);
        edit_end();
    } else if (ui.library <= CB_SLOTS) {
        cb_song before = song;

        if (song_load_slot(ui.library - 1)) {
            ui_say("Empty or invalid slot", 0);
            setpage(P_LIB);
            return;
        }

        undo_song = before;
        undo_valid = 1;
        redo_valid = 0;
    } else {
        edit_begin(1);
        song = cb_demos[ui.library - CB_SLOTS - 1];
        edit_end();
    }

    ui.section = ui.chord = ui.part = 0;
    setpage(P_MAIN);
}

static void apply_preset(int i) {
    ui.preset = bounded(i, 0, (int) cb_npresets - 1);
    edit_begin(0);
    cb_preset_apply(current(), ui.preset);
    edit_end();
}

static void name_start(int whole) {
    ui.namesong = whole;
    ui.namepos = 0;
    ui.back = ui.page;
    edit_begin(1);
    edit_end();
    setpage(P_NAME);
}

static const char *const actions[] = { "Insert",
                                       "Copy",
                                       "Remove",
                                       "Rename section",
                                       "Rename song",
                                       "Undo",
                                       "Redo",
                                       "Loop section / song",
                                       "Library / demos",
                                       "Help / controls",
                                       "Mixer to all sections" };

static const char *const mixer_actions[] = {
    "Select instrument", "Add part", "Remove part", "Undo", "Redo", "Mixer to all sections"
};
static const int mixer_action_ids[] = { 11, 0, 2, 5, 6, 10 };

static int menu_count(void) {
    return ui.back == P_MIX ? sizeof(mixer_actions) / sizeof(mixer_actions[0])
                            : sizeof(actions) / sizeof(actions[0]);
}

static void menu_action(void) {
    int a = ui.back == P_MIX ? mixer_action_ids[ui.menu] : ui.menu;

    setpage(ui.back);

    if (a == 0)
        insert_item(0);
    else if (a == 1)
        insert_item(1);
    else if (a == 2)
        delete_item();
    else if (a == 3 || a == 4)
        name_start(a == 4);
    else if (a == 5 || a == 6)
        undo(a == 6);
    else if (a == 7)
        cb_loop = (cb_loop + 1) % 3;
    else if (a == 8)
        setpage(P_LIB);
    else if (a == 9)
        setpage(P_HELP);
    else if (a == 11) {
        if (!current()->nparts)
            insert_item(0);
        else
            setpage(P_INSTR);
    } else {
        unsigned    i;
        cb_section *s = current();
        edit_begin(0);

        for (i = 0; i < song.count; i++) {
            song.sections[i].nparts = s->nparts;
            memcpy(song.sections[i].parts, s->parts, sizeof(s->parts));
            song.sections[i].reverb = s->reverb;
            song.sections[i].chorus = s->chorus;
        }

        edit_end();

        ui_say("Mixer applied to whole song", 0);
    }
}

static void select_action(void) {
    if (ui.page == P_INSTR)
        setpage(P_MIX);
    else if (ui.page == P_PRESET) {
        apply_preset(ui.preset);
        setpage(P_MIX);
    } else if (ui.page == P_MENU)
        menu_action();
    else if (ui.page == P_NAME)
        setpage(ui.back);
    else if (ui.page == P_CONFIRM)
        load_choice();
    else if (ui.page == P_LIB) {
        if (project_dirty)
            setpage(P_CONFIRM);
        else
            load_choice();
    } else {
        ui.back = ui.page;
        ui.menu = 0;
        setpage(P_MENU);
    }
}

static void knob(int role, int d) {
    if (!d)
        return;

    ui_redraw = 1;

    if (role == EN_ALGO) {
        int next = bounded(proj.keyboard + d, 0, CB_KEYBOARD_VOICES - 1);
        if (next != proj.keyboard) {
            keyboard_release();
            proj.keyboard = (uint8_t) next;
            keyboard_apply();
            project_dirty = 1;
        }
        return;
    }

    if (role == EN_PRESET && ui.page != P_LIB && ui.page != P_NAME && ui.page != P_CONFIRM) {
        int i = ui.preset, dir = d < 0 ? -1 : 1;

        while (d) {
            int next = i + dir;

            while (next >= 0 && next < (int) cb_npresets && !ui.legacy && cb_presets[next].legacy)
                next += dir;

            if (next < 0 || next >= (int) cb_npresets)
                break;

            i = next;
            d -= dir;
        }

        ui.preset = i;

        if (ui.page != P_PRESET)
            apply_preset(i);

        return;
    }

    if (role == EN_SELECT) {
        switch (ui.page) {
            case P_MIX:
            case P_INSTR:
                ui.part = bounded(ui.part + d, 0, current()->nparts ? current()->nparts - 1 : 0);
                break;

            case P_SECT:
                ui.section = bounded(ui.section + d, 0, song.count - 1);
                fix_cursor();
                break;

            case P_PRESET:
                knob(EN_PRESET, d);
                break;

            case P_MENU:
                ui.menu = bounded(ui.menu + d, 0, menu_count() - 1);
                break;

            case P_LIB:
                ui.library = bounded(ui.library + d, 0, CB_SLOTS + (int) cb_ndemos);
                break;

            case P_NAME:
                ui.namepos = bounded(ui.namepos + d, 0, ui.namesong ? 22 : 20);
                break;

            default:
                ui.chord = bounded(ui.chord + d, 0, current()->count - 1);
                break;
        }
        return;
    }

    if (role < EN_K1)
        return;

    int         k = role - EN_K1;
    cb_section *s = current();
    cb_chord   *c = chord();

    if (ui.page == P_LIB) {
        if (k == 0) {
            proj.slot = bounded(proj.slot + d, 0, 15);
            project_dirty = 1;
        }
        return;
    }

    if (ui.page == P_PRESET) {
        if (k == 3)
            ui.legacy = d > 0;
        else
            knob(EN_PRESET, d);
        return;
    }

    if (ui.page == P_MENU || ui.page == P_CONFIRM || ui.page == P_HELP)
        return;

    if (ui.page == P_SECT && k == 1) {
        cb_loop = bounded((int) cb_loop + d, 0, 2);
        return;
    }

    if (ui.page == P_FX && k == 3) {
        proj.midi_out = d > 0;
        cb_midi_out = proj.midi_out;
        project_dirty = 1;
        return;
    }

    /* Harmony uses the same live refresh as slash bass. Only duration changes
     * alter the chord timeline; keep those edits stopped. */
    edit_begin(ui.page == P_NAME || (ui.page <= P_INV && k == 2));

    if (ui.page == P_NAME) {
        char    *name = ui.namesong ? song.name : s->name;
        unsigned len = str_len(name), j;

        for (j = len; j < (unsigned) ui.namepos; j++)
            name[j] = ' ';

        char old = name[ui.namepos] ? name[ui.namepos] : ' ';

        name[ui.namepos] = (char) bounded(old + d, 32, 126);

        if ((unsigned) ui.namepos >= len)
            name[ui.namepos + 1] = 0;
    } else if (ui.page == P_SECT) {
        if (k == 0)
            s->repeat = bounded(s->repeat + d, 1, 16);

        if (k == 2)
            song.tempo = bounded(song.tempo + d, 30, 300);
    } else if (ui.page == P_MIX || ui.page == P_INSTR) {
        if (s->nparts) {
            cb_part *p = &s->parts[ui.part];

            if (ui.page == P_INSTR) {
                if (k == 0) {
                    ui.category = bounded(ui.category + d, 0, 7);

                    if (ui.category && cb_find_track(p->track)->category != ui.category) {
                        unsigned i;

                        for (i = 0; i < cb_ntracks; i++)
                            if (cb_tracks[i].category == ui.category) {
                                p->track = cb_tracks[i].id;
                                break;
                            }
                    }
                }

                if (k == 1)
                    track_move(d);

                if (k == 2)
                    p->volume = bounded(p->volume + d, 0, 10);

                if (k == 3)
                    p->flags = (p->flags & 1) | (d > 0 ? 2 : 0);
            } else {
                if (k == 0)
                    p->volume = bounded(p->volume + d, 0, 10);

                if (k == 1)
                    p->flags = (p->flags & 2) | (d > 0 ? 1 : 0);

                if (k == 2)
                    p->flags = (p->flags & 1) | (d > 0 ? 2 : 0);

                if (k == 3)
                    song.tempo = bounded(song.tempo + d, 30, 300);
            }
        }
    } else if (ui.page == P_FX) {
        if (k == 0)
            s->reverb = bounded(s->reverb + d, 0, 127);

        if (k == 1)
            s->chorus = bounded(s->chorus + d, 0, 127);

        if (k == 2) {
            unsigned i, j;

            d = bounded(d, -24, 24);
            arr_play(0);

            for (i = 0; i < song.count; i++)
                for (j = 0; j < song.sections[i].count; j++) {
                    cb_chord *cc = &song.sections[i].chords[j];
                    cc->root = pitch_roots[(cb_root_value(cc->root) + d + 120) % 12];

                    if (cc->bass >= 0)
                        cc->bass = pitch_roots[(cb_root_value(cc->bass) + d + 120) % 12];
                }
            ui.transpose += d;
        }
    } else if (ui.page == P_INV) {
        if (k == 0)
            c->inversion = bounded(c->inversion + d, -1, cb_types[c->type].count ? cb_types[c->type].count * 3 - 1 : -1);

        if (k == 1)
            c->bass = bounded(c->bass + d, -1, 20);

        if (k == 2)
            c->duration = bounded(c->duration + d, 1, 32);

        if (k == 3)
            song.tempo = bounded(song.tempo + d, 30, 300);
    } else {
        if (k == 0)
            c->root = bounded(c->root + d, 0, 20);

        if (k == 1) {
            c->type = bounded(c->type + d, 0, (int) cb_ntypes - 1);
            c->inversion = -1;
        }

        if (k == 2)
            c->duration = bounded(c->duration + d, 1, 32);

        if (k == 3 && ui.page == P_CHORD)
            c->bass = bounded(c->bass + d, -1, 20);
        else if (k == 3)
            song.tempo = bounded(song.tempo + d, 30, 300);
    }

    edit_end();
}

void ui_input_only(void) {
    uint32_t b = plat_buttons(), down = b & ~ui.buttons, keys = plat_keys();
    int      i;

    ui.buttons = b;

    if ((b >> B_HOME) & 1) {
        if (down >> B_REC & 1) {
            undo(0);
            down &= ~(1u << B_REC);
        }

        if (down >> B_PLAY & 1) {
            undo(1);
            down &= ~(1u << B_PLAY);
        }
    }

    if (down >> B_HOME & 1)
        setpage(P_MAIN);

    if (down >> B_EDIT & 1)
        setpage(ui.page == P_CHORD ? P_MAIN : P_CHORD);

    if (down >> B_ARP & 1)
        setpage(ui.page == P_INV ? P_CHORD : P_INV);

    if (down >> B_SEQ & 1)
        setpage(ui.page == P_SECT ? P_MAIN : P_SECT);

    if (down >> B_LFO & 1)
        setpage(ui.page == P_MIX ? P_MAIN : P_MIX);

    if (down >> B_ENV & 1)
        setpage(P_PRESET);

    if (down >> B_FX & 1)
        setpage(ui.page == P_FX ? P_MAIN : P_FX);

    if (down >> B_GLO & 1)
        setpage(ui.page == P_LIB ? P_HELP : P_LIB);

    if (down >> B_SEL & 1)
        select_action();

    if (down >> B_PLAY & 1) {
        if (arr_playing)
            arr_play(0);
        else
            cb_start(ui.section);
        ui_redraw = 1;
    }

    if (down >> B_REC & 1)
        insert_item(0);

    if (down >> B_SAVE & 1)
        save();

    if (down >> B_OCTDN & 1) {
        ui.section = bounded(ui.section - 1, 0, song.count - 1);
        ui.chord = 0;
        fix_cursor();
        ui_redraw = 1;
    }

    if (down >> B_OCTUP & 1) {
        ui.section = bounded(ui.section + 1, 0, song.count - 1);
        ui.chord = 0;
        fix_cursor();
        ui_redraw = 1;
    }

    for (i = 0; i < NE; i++)
        knob(i, plat_enc(i));

    for (i = 0; i < NKEYS; i++)
        if ((keys ^ ui.keys) >> i & 1) {
            int on = (keys >> i) & 1;

            if (on && (ui.page == P_CHORD || ui.page == P_INV)) {
                edit_begin(0);
                chord()->root = pitch_roots[(53 + i) % 12];
                edit_end();
            } else if (on) {
                /* Bass: F1-G3; other sounds: F2-G4. Chord root entry stays unchanged. */
                ui.live_notes[i] = (uint8_t) ((proj.keyboard == 3 ? 29 : 41) + i);
                ui.live_keys |= 1u << i;
                gm_midi(0x9f, ui.live_notes[i], 96);
            } else if (ui.live_keys >> i & 1) {
                gm_midi(0x8f, ui.live_notes[i], 0);
                ui.live_keys &= ~(1u << i);
            }
        }

    ui.keys = keys;
    plat_glow(1);
    plat_leds((arr_playing ? 1u << B_PLAY : 0) | (project_dirty ? 1u << B_SAVE : 0), keys);
}

static void text_fit(int x, int y, int w, const char *s, const felucca_font_t *f, uint16_t col) {
    char b[80];
    str_cpy(b, s, sizeof(b));

    unsigned n = str_len(b);

    while (n && text_w(f, b) > w)
        b[--n] = 0;

    cv_text(x, y, f, b, col);
}

static void chord_label(char *b, const cb_chord *c) {
    str_cpy(b, cb_roots[c->root], 64);
    append(b, " ", 64);
    append(b, cb_types[c->type].name, 64);

    if (c->bass >= 0) {
        append(b, "/", 64);
        append(b, cb_roots[c->bass], 64);
    }
}

static void row(int y, int h, const char *left, const char *right, int selected) {
    cv_rect(0, y, 240, h - 1, selected ? SELECTED : CELL);
    cv_rect(0, y + h - 1, 240, 1, RULE);

    int w = right ? text_w(&FONT_XS, right) : 0;

    text_fit(7, y + 5, 222 - w, left, &FONT_S, INK);

    if (right)
        cv_text(234 - w, y + 7, &FONT_XS, right, INK);
}

static void footer(const char *a, const char *b, const char *c, const char *d) {
    const char *labels[] = { a, b, c, d };
    int         i;

    cv_rect(0, 204, 240, 36, DARK);

    for (i = 0; i < 4; i++) {
        text_fit(i * 60 + 4, 209, 54, labels[i], &FONT_XS, C_WHITE);

        if (i)
            cv_rect(i * 60, 211, 1, 21, RULE);
    }

    char label[32];
    str_cpy(label, "ALGO: ", sizeof(label));
    append(label, keyboard_names[proj.keyboard], sizeof(label));
    text_fit(4, 225, 119, label, &FONT_XS, RGB(175, 192, 185));
    text_fit(130, 225, 106, "SEL: actions", &FONT_XS, RGB(175, 192, 185));
}

static void draw_screen(void) {
    char        a[80], b[64];
    cb_section *s = current();
    int         i;

    cv_rect(0, 0, 240, 26, BAR);
    a[0] = 0;
    number(a, ui.section + 1, sizeof(a));
    append(a, "/", sizeof(a));
    number(a, song.count, sizeof(a));
    append(a, "  ", sizeof(a));
    append(a, song.name, sizeof(a));
    text_fit(6, 5, 163, a, &FONT_B, C_WHITE);
    b[0] = 0;
    number(b, song.tempo, sizeof(b));
    append(b, arr_playing ? " >" : " BPM", sizeof(b));
    text_fit(173, 6, 63, b, &FONT_XS, C_WHITE);

    if (ui.page == P_MAIN || ui.page == P_CHORD || ui.page == P_INV) {
        int count = ui.page == P_MAIN ? 12 : 4, base = ui.chord / count * count;

        text_fit(6, 29, 228, s->name, &FONT_XS, INK);

        for (i = 0; i < count; i++) {
            int index = base + i, x = i % 2 * 120, y = 48 + i / 2 * 26;

            if (index >= s->count)
                break;

            int active = arr_playing && cb_section_now == ui.section && cb_chord_now == index;

            cv_rect(x, y, 119, 25, index == ui.chord ? SELECTED : CELL);

            if (active)
                cv_rect(x, y, 3, 25, RGB(28, 156, 137));

            chord_label(b, s->chords + index);
            text_fit(x + 5, y + 4, 80, b, &FONT_S, INK);
            b[0] = 0;
            number(b, s->chords[index].duration, sizeof(b));
            append(b, "/4", sizeof(b));
            text_fit(x + 87, y + 6, 31, b, &FONT_XS, INK);
        }

        if (ui.page != P_MAIN) {
            cv_rect(0, 104, 240, 25, DARK);
            text_fit(7, 108, 226, ui.page == P_INV ? "Chord inversion" : "Edit chord", &FONT_B, C_WHITE);
            chord_label(b, chord());
            row(130, 28, b, 0, 1);
            b[0] = 0;
            append(b, "Bass: ", sizeof(b));
            append(b, chord()->bass < 0 ? "Root" : cb_roots[chord()->bass], sizeof(b));
            row(158, 24, b, 0, 0);
            str_cpy(b, "Inversion: ", sizeof(b));

            if (chord()->inversion < 0)
                append(b, "Auto", sizeof(b));
            else {
                int         n = cb_types[chord()->type].count;
                const char *reg[] = { "Low ", "Mid ", "High " };

                append(b, reg[n ? bounded(chord()->inversion / n, 0, 2) : 0], sizeof(b));
                number(b, n ? chord()->inversion % n : 0, sizeof(b));
            }

            text_fit(7, 184, 225, b, &FONT_XS, INK);
        }

        if (ui.page == P_INV)
            footer("Inv", "Bass", "Beats", "Tempo");
        else
            footer("Root", "Type", "Beats", ui.page == P_CHORD ? "Bass" : "Tempo");
    } else if (ui.page == P_MIX || ui.page == P_INSTR) {
        if (ui.page == P_INSTR && s->nparts) {
            str_cpy(b, "Vol ", sizeof(b));
            number(b, s->parts[ui.part].volume, sizeof(b));
            append(b, s->parts[ui.part].flags & 2 ? " Solo" : "", sizeof(b));
        } else
            str_cpy(b, s->name, sizeof(b));

        row(28, 28, ui.page == P_MIX ? "Instrument mixer" : "Select instrument", b, 0);

        if (ui.page == P_MIX) {
            int base = ui.part / 5 * 5;

            for (i = 0; i < 5; i++) {
                int p = base + i;

                if (p >= s->nparts) {
                    if (p == s->nparts)
                        row(57 + i * 29, 29, "Empty - REC to add", 0, 0);
                    break;
                }

                const cb_track *t = cb_find_track(s->parts[p].track);

                a[0] = 0;
                number(a, p + 1, sizeof(a));
                append(a, ". ", sizeof(a));
                append(a, cb_categories[t->category], sizeof(a));
                append(a, " ", sizeof(a));
                append(a, t->name, sizeof(a));
                str_cpy(b, s->parts[p].flags & 1 ? "M " : s->parts[p].flags & 2 ? "S " : "V ", sizeof(b));
                number(b, s->parts[p].volume, sizeof(b));
                row(57 + i * 29, 29, a, b, p == ui.part);
            }
            footer("Volume", "Mute", "Solo", "Tempo");
        } else {
            const cb_track *t = s->nparts ? cb_find_track(s->parts[ui.part].track) : cb_tracks;
            int             idx = track_index(t->id), shown[5], n = 0, start = idx - 2;

            while (start > 0 && ui.category && cb_tracks[start].category != ui.category)
                start--;

            for (i = start < 0 ? 0 : start; i < (int) cb_ntracks && n < 5; i++)
                if (!ui.category || cb_tracks[i].category == ui.category)
                    shown[n++] = i;

            for (i = 0; i < n; i++) {
                const cb_track *tt = cb_tracks + shown[i];

                row(57 + i * 29, 29, tt->name, cb_categories[tt->category], tt == t);
            }

            footer("Category", "Pattern", "Volume", "Solo");
        }
    } else if (ui.page == P_SECT) {
        row(28, 28, "Sections", cb_loop == 1 ? "Loop section" : cb_loop == 2 ? "Loop song" : "Loop off", 0);
        int base = ui.section / 5 * 5;

        for (i = 0; i < 5 && base + i < song.count; i++) {
            cb_section *sc = song.sections + base + i;
            a[0] = 0;
            number(a, base + i + 1, sizeof(a));
            append(a, ". ", sizeof(a));
            append(a, sc->name, sizeof(a));
            str_cpy(b, "x", sizeof(b));
            number(b, sc->repeat, sizeof(b));
            row(57 + i * 29, 29, a, b, base + i == ui.section);
        }

        footer("Repeat", "Loop", "Tempo", "-");
    } else if (ui.page == P_PRESET) {
        row(28, 28, "Style presets", ui.legacy ? "All" : "Current", 0);

        int list[96] = { 0 }, n = 0, pos = 0;

        for (i = 0; i < (int) cb_npresets; i++)
            if (ui.legacy || !cb_presets[i].legacy) {
                if (i <= ui.preset)
                    pos = n;
                list[n++] = i;
            }

        ui.preset = list[pos];
        int base = bounded(pos - 2, 0, n > 5 ? n - 5 : 0);

        for (i = 0; i < 5 && base + i < n; i++) {
            const cb_preset *p = cb_presets + list[base + i];

            row(57 + i * 29, 29, p->name, p->time, list[base + i] == ui.preset);
        }

        footer("Style", "Style", "Style", "Legacy");
    } else if (ui.page == P_FX) {
        row(28, 28, "Effects / song", 0, 0);
        str_cpy(b, "", sizeof(b));
        number(b, s->reverb, sizeof(b));
        row(57, 29, "Reverb", b, 0);
        b[0] = 0;
        number(b, s->chorus, sizeof(b));
        row(86, 29, "Chorus", b, 0);
        b[0] = 0;
        number(b, ui.transpose, sizeof(b));
        row(115, 29, "Transpose song", b, 0);
        row(144, 29, "USB MIDI out", cb_midi_out ? "On" : "Off", 0);
        b[0] = 0;
        number(b, plat_cpu_pct(), sizeof(b));
        append(b, "% CPU / ", sizeof(b));
        number(b, gm_voices, sizeof(b));
        append(b, " voices", sizeof(b));
        text_fit(7, 182, 226, b, &FONT_XS, INK);
        footer("Reverb", "Chorus", "Transpose", "MIDI out");
    } else if (ui.page == P_LIB) {
        str_cpy(b, "Save: ", sizeof(b));
        number(b, proj.slot + 1, sizeof(b));
        row(28, 28, "Songs / demos", b, 0);

        int base = bounded(ui.library - 2, 0, CB_SLOTS + (int) cb_ndemos - 4);

        for (i = 0; i < 5; i++) {
            int n = base + i;

            if (!n)
                str_cpy(a, "New song", sizeof(a));
            else if (n <= CB_SLOTS) {
                struct {
                    uint32_t magic;
                    uint16_t version, tempo;
                    char     name[24];
                } head;

                a[0] = 0;
                number(a, n, sizeof(a));
                append(a, ". ", sizeof(a));

                int ok =
                    plat_store_head(OBJ_PAT0 + n - 1, &head, sizeof(head)) == (int) sizeof(head) &&
                    head.magic == 0x43424631u && !head.name[23];
                append(a, ok ? head.name : "(empty)", sizeof(a));
            } else {
                str_cpy(a, "Demo: ", sizeof(a));
                append(a, cb_demos[n - CB_SLOTS - 1].name, sizeof(a));
            }

            row(57 + i * 29, 29, a, 0, n == ui.library);
        }
        footer("Save slot", "-", "-", "-");
    } else if (ui.page == P_MENU) {
        row(28, 28, "Actions", 0, 0);

        int base = bounded(ui.menu - 2, 0, menu_count() - 5);
        const char *const *items = ui.back == P_MIX ? mixer_actions : actions;

        for (i = 0; i < 5; i++)
            row(57 + i * 29, 29, items[base + i], 0, base + i == ui.menu);
        footer("-", "-", "-", "-");
    } else if (ui.page == P_NAME) {
        row(28, 28, ui.namesong ? "Rename song" : "Rename section", 0, 0);

        const char *name = ui.namesong ? song.name : s->name;

        text_fit(8, 81, 224, name, &FONT_B, INK);
        a[0] = 0;
        number(a, ui.namepos + 1, sizeof(a));
        row(119, 29, "Character", a, 0);
        text_fit(8, 158, 224, "SELECT position, K1 character", &FONT_XS, INK);
        text_fit(8, 180, 224, "SEL done", &FONT_XS, INK);
        footer("Character", "-", "-", "-");
    } else if (ui.page == P_CONFIRM) {
        row(28, 28, "Unsaved changes", 0, 0);
        text_fit(8, 80, 224, "Replace current song?", &FONT_B, INK);
        text_fit(8, 111, 224, "SEL load / HOME cancel", &FONT_S, INK);
        text_fit(8, 142, 224, "SAVE keeps it in chosen slot", &FONT_XS, INK);
        footer("-", "-", "-", "-");
    } else {
        static const char *const help[] = {
            "EDIT chords / ARP inversion",
            "LFO mixer / SEL actions",
            "SEQ sections / ENV presets",
            "REC insert / SEL actions",
            "FX effects / GLO songs",
            "HOME+REC undo / +PLAY redo",
            "OCT-/+ section / SAVE store",
            "ALGO keys / PRESET style"
        };

        for (i = 0; i < 8; i++)
            text_fit(6, 35 + i * 21, 228, help[i], &FONT_S, INK);

        footer("Root", "Type", "Beats", "Tempo");
    }

    if (ui.notice_until) {
        cv_rect(0, 178, 240, 26, BAR);
        text_fit(5, 183, 230, ui.notice, &FONT_S, C_WHITE);
    }
}

void ui_frame(void) {
    static uint8_t oldplaying, oldsection, oldchord;

    ui_input_only();

    if (ui.notice_until && (int32_t) (plat_ms() - ui.notice_until) >= 0) {
        ui.notice_until = 0;
        ui_redraw = 1;
    }

    if (oldplaying != arr_playing || oldsection != cb_section_now || oldchord != cb_chord_now) {
        oldplaying = arr_playing;
        oldsection = cb_section_now;
        oldchord = cb_chord_now;

        if (arr_playing && ui.page == P_MAIN) {
            ui.section = cb_section_now;
            ui.chord = cb_chord_now;
            fix_cursor();
        }

        ui_redraw = 1;
    }

    if (!ui_redraw)
        return;

    ui_redraw = 0;
    int y;

    for (y = 0; y < 240; y += 120) {
        cv_begin(240, 120, BG);
        cv_oy = -y;
        draw_screen();
        cv_blit(0, y);
    }
}
