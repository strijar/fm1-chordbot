#include "arr.h"
#include "catalog.inc"

const char *const cb_roots[21] = { "Cb", "C", "C#", "Db", "D", "D#", "Eb", "E", "E#", "Fb", "F", "F#", "Gb", "G", "G#", "Ab", "A", "A#", "Bb", "B", "B#" };
const char *const cb_categories[8] = { "All", "Ac.Gtr", "El.Gtr", "Bass", "Drums", "Keys", "Piano", "Synth" };

const cb_track   *cb_find_track(unsigned id) {
    unsigned i;

    for (i = 0; i < cb_ntracks; i++)
        if (cb_tracks[i].id == id)
            return cb_tracks + i;

    return 0;
}

int cb_root_value(int r) {
    static const int8_t values[21] = { 59, 60, 61, 61, 62, 63, 63, 52, 53, 52, 53, 54, 54, 55, 56, 56, 57, 58, 58, 59, 60 };

    return values[r >= 0 && r < 21 ? r : 1];
}

void cb_preset_apply(cb_section *s, unsigned i) {
    unsigned         k;
    const cb_preset *p = &cb_presets[i % cb_npresets];

    s->nparts = p->count;
    s->reverb = p->reverb;
    s->chorus = p->chorus;

    for (k = 0; k < CB_PARTS; k++)
        s->parts[k] = p->parts[k];
}

void cb_default(cb_song *s) {
    unsigned i;
    uint8_t *p = (uint8_t *) s;

    for (i = 0; i < sizeof(*s); i++)
        p[i] = 0;

    s->magic = 0x43424631u;
    s->version = 1;
    s->tempo = 110;
    s->count = 1;

    const char *n = "Untitled", *v = "Verse";

    for (i = 0; n[i]; i++)
        s->name[i] = n[i];

    for (i = 0; v[i]; i++)
        s->sections[0].name[i] = v[i];

    cb_section *sc = &s->sections[0];
    sc->count = 4;
    sc->repeat = 1;

    const uint8_t roots[] = { 1, 16, 10, 13 }, types[] = { 0, 1, 0, 5 };

    for (i = 0; i < 4; i++)
        sc->chords[i] = (cb_chord) { roots[i], types[i], 4, -1, -1 };

    for (i = 0; i < cb_npresets; i++)
        if (cb_presets[i].id == 2)
            cb_preset_apply(sc, i);
}

int cb_validate(const cb_song *s) {
    unsigned i, j;

    if (s->magic != 0x43424631u || s->version != 1 || !s->count || s->count > CB_SECTIONS ||
        s->tempo < 30 || s->tempo > 300 || s->name[23])
        return 0;

    for (i = 0; i < s->count; i++) {
        const cb_section *sc = &s->sections[i];

        if (!sc->count || sc->count > CB_CHORDS || sc->nparts > CB_PARTS || sc->name[21] ||
            sc->reverb > 127 || sc->chorus > 127 || !sc->repeat || sc->repeat > 16)
            return 0;

        for (j = 0; j < sc->count; j++) {
            const cb_chord *c = sc->chords + j;

            if (c->root > 20 || c->type >= cb_ntypes || !c->duration || c->duration > 32 ||
                c->bass < -1 || c->bass > 20 || c->inversion < -1 || c->inversion > 35)
                return 0;
        }

        for (j = 0; j < sc->nparts; j++)
            if (!cb_find_track(sc->parts[j].track) || sc->parts[j].volume > 10 ||
                sc->parts[j].flags > 3)
                return 0;
    }

    return 1;
}
