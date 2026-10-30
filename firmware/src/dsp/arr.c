/* Streaming ChordBot patterns, PPQN 24. No allocation or full-song event list.
 * UI edits hold cb_lock; requests are consumed exclusively by the audio ISR. */

#include "arr.h"
#include "gm.h"

cb_song song            EN_POOL;

volatile uint8_t        cb_lock, arr_playing, cb_section_now, cb_chord_now, cb_loop, cb_midi_out;
volatile uint16_t       cb_tick_now;
static volatile uint8_t request, start_section, refresh;
static int              notes[CB_CHORDS][12], roots[CB_CHORDS][12];
static int              previous[12], previous_count;
static int              section_previous[12], section_previous_count;
static int16_t          held[CB_PARTS][CB_PATTERNS], deferred[CB_PARTS][CB_PATTERNS];
static uint8_t          refs[16][128];
static uint32_t         continuous[CB_PARTS], phase;
static uint8_t          pass;
static uint8_t          ch_for[CB_PARTS];
static uint16_t         midi_panic;
static uint8_t          midi_enabled;
static uint32_t         midi_link;

static int distance(int a, int b) {
    return a > b ? a - b : b - a;
}

void cb_voice(const cb_chord *c, int *out, int *root, const int *prev, int pn) {
    const cb_type *t = &cb_types[c->type];
    int            n = t->count, i, j, best = n;
    int            inv[36][12];

    if (!n)
        return;

    for (j = 0; j < n; j++)
        inv[0][j] = cb_root_value(c->root) + t->notes[j] - 12;

    for (i = 1; i < n * 3; i++) {
        for (j = 0; j < n - 1; j++)
            inv[i][j] = inv[i - 1][j + 1];
        inv[i][n - 1] = inv[i - 1][0] + 12;
    }

    if (pn) {
        int scores[36] = { 0 };
        best = 1;

        for (i = 0; i < n * 3; i++) {
            for (j = 0; j < n && j < pn; j++)
                scores[i] += distance(prev[j], inv[i][j]);

            if (scores[i] < scores[best])
                best = i;
        }
    }

    if (c->inversion >= 0)
        best = c->inversion < n * 3 ? c->inversion : n * 3 - 1;

    for (j = 0; j < n; j++) {
        out[j] = inv[best][j];
        root[j] = inv[n][j];
    }
}

int cb_note(const cb_track *t, const cb_pattern *p, const cb_chord *c, const int *v, const int *r, int n) {
    if (t->mode == CB_DRUMS)
        return p->source;

    if (p->chromatic)
        return cb_root_value(p->source % 12 == 0 && c->bass >= 0 ? c->bass : c->root) + p->source +
               t->transpose;

    int i = p->source, down = 0;

    if (t->mode == CB_GUITAR && n > 3 && i > 2)
        i++;

    if (i < 0) {
        i = -i;
        down = i < n ? 12 : 12 * (i / n) + 12;
    }

    const int *vv = (!t->invert && (c->inversion < 0 || t->mode == CB_BASS)) ? r : v;

    return vv[i % n] + 12 * (i / n) - down + t->transpose;
}

static void send(unsigned st, unsigned a, unsigned b, unsigned at) {
    gm_midi_now(st, a, b, at);

    if (cb_midi_out && !midi_panic) {
        if (plat_midi_out_room())
            plat_midi_out(st >> 4 | st << 8 | a << 16 | b << 24);
        else
            midi_panic = 0xffffu; /* Never silently lose an external note-off. */
    }
}

/* Original 120/121 kits use MIDI percussion pitches. Route to GM channel 10;
 * clamp the few non-GM percussion keys to the nearest supported kit note. */

static int drum_note(int n) {
    return n < 35 ? 35 : n > 81 ? 81 : n;
}

static void off(int p, int note, unsigned at) {
    if (note < 0)
        return;

    unsigned ch = ch_for[p];

    if (refs[ch][note] && --refs[ch][note] == 0)
        send(0x80 | ch, note, 0, at);
}

static void silence(unsigned at) {
    unsigned ch, n, p, k;

    for (ch = 0; ch < 16; ch++)
        for (n = 0; n < 128; n++)
            if (refs[ch][n]) {
                send(0x80 | ch, n, 0, at);
                refs[ch][n] = 0;
            }

    for (p = 0; p < CB_PARTS; p++)
        for (k = 0; k < CB_PATTERNS; k++)
            held[p][k] = deferred[p][k] = -1;
}

static void configure(int rebuild) {
    const cb_section *s = &song.sections[cb_section_now];
    unsigned          p;

    if (rebuild) {
        previous_count = section_previous_count;

        for (p = 0; p < 12; p++)
            previous[p] = section_previous[p];
    } else {
        section_previous_count = previous_count;

        for (p = 0; p < 12; p++)
            section_previous[p] = previous[p];
    }

    for (p = 0; p < s->nparts; p++) {
        const cb_track *t = cb_find_track(s->parts[p].track);

        /* Replacing/unmuting a continuous pattern keeps the song's phase. */

        unsigned ci, elapsed = 0;

        for (ci = 0; ci < cb_chord_now; ci++)
            if (cb_types[s->chords[ci].type].count)
                elapsed += s->chords[ci].duration * 24u / t->delta;

        if (cb_types[s->chords[cb_chord_now].type].count)
            elapsed += (cb_tick_now + t->delta - 1u) / t->delta;

        continuous[p] = elapsed;
        ch_for[p] = (uint8_t) (t->mode == CB_DRUMS ? 9 : p >= 9 ? p + 1
                                                                : p);
        unsigned ch = ch_for[p];

        if (ch != 9)
            send(0xc0 | ch, t->instrument, 0, 0);

        send(0xb0 | ch, 7, 100, 0);
        send(0xb0 | ch, 10, 64, 0);
        send(0xb0 | ch, 91, (s->reverb > 15 ? 127 : s->reverb * 8), 0);
        send(0xb0 | ch, 93, (s->chorus > 15 ? 127 : s->chorus * 8), 0);
    }

    gm_fx(s->reverb ? 4 : 0, (s->reverb > 15 ? 127 : s->reverb * 8), 1, (s->chorus > 15 ? 127 : s->chorus * 8));

    for (p = 0; p < s->count; p++) {
        cb_voice(s->chords + p, notes[p], roots[p], previous, previous_count);

        int n = cb_types[s->chords[p].type].count;

        if (n) {
            unsigned j;
            previous_count = n;

            for (j = 0; j < (unsigned) n; j++)
                previous[j] = notes[p][j];
        }
    }
}

void arr_init(void) {
    unsigned p;

    arr_playing = 0;
    request = refresh = 0;
    phase = 0;
    cb_section_now = cb_chord_now = 0;
    cb_tick_now = 0;
    previous_count = 0;
    pass = 0;
    midi_panic = 0;
    midi_enabled = 0;
    midi_link = plat_midi_out_link();

    for (p = 0; p < CB_PARTS; p++)
        continuous[p] = 0;

    silence(0);
}

void cb_start(unsigned s) {
    start_section = (uint8_t) (s < song.count ? s : 0);
    request = 2;
}

void arr_play(int on) {
    if (on)
        cb_start(0);
    else
        request = 1;
}

void cb_refresh(void) {
    refresh = 1;
}

void arr_late_offs(void) {
} /* MIDI ownership stays inside arr_block. */

static void tick(unsigned at) {
    cb_section *s = &song.sections[cb_section_now];
    unsigned    p, k;

    if (cb_tick_now >= s->chords[cb_chord_now].duration * 24u) {
        for (p = 0; p < s->nparts; p++)
            for (k = 0; k < CB_PATTERNS; k++) {
                off(p, deferred[p][k], at);
                deferred[p][k] = -1;
            }

        cb_tick_now = 0;

        if (++cb_chord_now >= s->count) {
            silence(at);
            cb_chord_now = 0;

            if (++pass >= s->repeat) {
                pass = 0;

                if (cb_loop != 1 && ++cb_section_now >= song.count) {
                    if (cb_loop == 2)
                        cb_section_now = 0;
                    else {
                        cb_section_now = song.count - 1;
                        arr_playing = 0;
                        return;
                    }
                }
            }

            for (p = 0; p < CB_PARTS; p++)
                continuous[p] = 0;

            configure(0);
            s = &song.sections[cb_section_now];
        }
    }

    int solo = 0;

    for (p = 0; p < s->nparts; p++)
        solo |= s->parts[p].flags & 2;

    const cb_chord *c = &s->chords[cb_chord_now];
    int             n = cb_types[c->type].count;

    for (p = 0; p < s->nparts; p++) {
        const cb_part  *part = &s->parts[p];
        const cb_track *t = cb_find_track(part->track);
        unsigned        step = cb_tick_now / t->delta;
        unsigned        index = t->continuous ? continuous[p] : step;

        if (n && t->continuous && cb_tick_now % t->delta == 0)
            continuous[p]++;

        if (!n || part->volume == 0 || (part->flags & 1) || (solo && !(part->flags & 2))) {
            for (k = 0; k < t->count; k++) {
                off(p, held[p][k], at);
                off(p, deferred[p][k], at);
                held[p][k] = deferred[p][k] = -1;
            }

            continue;
        }

        if (cb_tick_now % t->delta)
            continue;

        for (k = 0; k < t->count; k++) {
            const cb_pattern *pat = t->patterns + k;
            char              ch = pat->steps[index % pat->len];

            if (ch == '.')
                continue;

            int value = ch == '0' ? 0 : ch - '0' + t->volume_offset;

            if (value < 0)
                continue;

            if (held[p][k] >= 0) {
                off(p, deferred[p][k], at);
                deferred[p][k] = -1;

                if (t->sustain && step >= 1 && t->mode != CB_DRUMS && !value)
                    deferred[p][k] = held[p][k];
                else
                    off(p, held[p][k], at);

                held[p][k] = -1;
            }

            if (value) {
                off(p, deferred[p][k], at);
                deferred[p][k] = -1;

                unsigned ci = cb_chord_now;

                if (t->leading && c->duration * (24 / t->delta) < pat->len && step >= t->leading &&
                    ci + 1 < s->count && cb_types[s->chords[ci + 1].type].count)
                    ci++;

                int note = cb_note(t, pat, s->chords + ci, notes[ci], roots[ci], cb_types[s->chords[ci].type].count);

                if (t->mode == CB_DRUMS)
                    note = drum_note(note);
                if (note >= 0 && note < 128) {
                    /* exact rational version of floor(value * volume * 1.32) + 20 */
                    int vel = value * part->volume * 132 / 100 + 20;

                    if (vel > 127)
                        vel = 127;

                    refs[ch_for[p]][note]++;
                    held[p][k] = (int16_t) note;
                    send(0x90 | ch_for[p], note, vel, at);
                }
            }
        }
    }

    cb_tick_now++;
}

void arr_block(uint32_t frames) {
    unsigned p, i;
    uint32_t packet;
    uint32_t link = plat_midi_out_link();

    if (midi_enabled != cb_midi_out || midi_link != link) {
        midi_panic = 0xffffu;
        midi_enabled = cb_midi_out;
        midi_link = link;
    }

    if (midi_panic) {
        for (i = 0; i < 16; i++)
            if ((midi_panic >> i & 1) && plat_midi_out_room() >= 2) {
                unsigned st = 0xb0 | i;
                plat_midi_out(0xbu | st << 8 | 123u << 16);
                plat_midi_out(0xbu | st << 8 | 120u << 16);
                midi_panic &= (uint16_t) ~(1u << i);
            }

        if (!midi_panic && cb_midi_out)
            refresh = 1;
    }

    /* Hardware and USB MIDI are forwarded even with the sequencer stopped. */

    for (i = 0; i < 64 && plat_midi_in(&packet); i++) {
        unsigned st = packet >> 8 & 255;

        if (st >= 0x80 && st < 0xf0)
            gm_midi_now(st, packet >> 16 & 127, packet >> 24 & 127, 0);
    }

    if (cb_lock)
        return;

    if (request) {
        unsigned r = request;

        request = 0;
        silence(0);
        arr_playing = 0;

        if (r == 2) {
            cb_section_now = start_section;
            cb_chord_now = 0;
            cb_tick_now = 0;
            pass = 0;
            previous_count = 0;
            phase = 0;
            refresh = 0;

            for (p = 0; p < CB_PARTS; p++)
                continuous[p] = 0;

            configure(0);
            arr_playing = 1;
            tick(0);
        }
    }

    if (!arr_playing)
        return;

    if (refresh) {
        refresh = 0;
        silence(0);
        configure(1);
    }

    /* Integer phase accumulator: tempo edits keep the fractional beat. */

    for (i = 0; i < frames / 2 && arr_playing; i++) {
        phase += (uint32_t) song.tempo * 24u;

        if (phase >= 22050u * 60u) {
            phase -= 22050u * 60u;
            tick(i);
        }
    }
}
