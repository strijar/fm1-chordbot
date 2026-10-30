/* Same UI, synth and sequencer as the firmware, backed by a simulated NOR
 * flash.
 * --interactive [flash.bin]: X11 panel + ALSA audio (loaded dynamically).
 * SCRIPT OUTDIR: deterministic script runner, WAV/PPM output and assertions. */
#define _POSIX_C_SOURCE 200809L
#include "../firmware/src/app/app.h"
#include "../firmware/src/dsp/eas/eas_data.h"
#include "../firmware/src/dsp/eas/eas_synth.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <dlfcn.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

static uint16_t fb[240 * 240];
static unsigned frames_drawn;
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c) {
    unsigned i, j;
    for (j = y; j < y + h && j < 240; j++)
        for (i = x; i < x + w && i < 240; i++)
            fb[j * 240 + i] = (c >> 8) | (c << 8);
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p) {
    unsigned i, j;
    for (j = 0; j < h && y + j < 240; j++)
        for (i = 0; i < w && x + i < 240; i++)
            fb[(y + j) * 240 + x + i] = p[j * w + i];
    frames_drawn++;
}
#include "../firmware/src/gfx.c"
static unsigned str_len(const char *s) { return (unsigned)strlen(s); }
static void str_cpy(char *d, const char *s, unsigned max) {
    if (max)
        snprintf(d, max, "%s", s);
}
static void fmt_int(char *b, int32_t v) { snprintf(b, 16, "%d", v); }
static uint32_t now_ms, buttons, keys, master = 2048;
static int enc[NE];
uint32_t plat_ms(void) { return now_ms; }
uint32_t plat_buttons(void) { return buttons; }
uint32_t plat_keys(void) { return keys; }
int32_t plat_enc(int i) {
    int n = enc[i];
    enc[i] = 0;
    return n;
}
uint32_t plat_master(void) { return master; }
void plat_leds(uint32_t b, uint32_t k) {
    (void)b;
    (void)k;
}
void plat_glow(int v) { (void)v; }
void plat_keys_dim(uint32_t v) { (void)v; }
void plat_play_red(int v) { (void)v; }
static uint32_t midi_queue[256], mr, mw;
int plat_midi_in(uint32_t *p) {
    if (mr == mw)
        return 0;
    *p = midi_queue[mr++ % 256];
    return 1;
}
static uint32_t note_ons;
static FILE *midi_file;
void plat_midi_out(uint32_t p) {
    if ((p >> 8 & 0xf0) == 0x90 && (p >> 24 & 127))
        note_ons++;
    if (midi_file)
        fprintf(midi_file, "%u %02x %02x %02x\n", now_ms, p >> 8 & 255, p >> 16 & 127,
                p >> 24 & 127);
}
uint32_t plat_midi_out_room(void) { return 256; }
uint32_t plat_midi_out_link(void) { return 1; }
uint32_t plat_cpu_pct(void) { return 0; }
uint32_t plat_xruns(void) { return 0; }
static uint8_t flash[1024 * 1024];
static int fail_flash;
static int st_read(uint32_t off, void *p, uint32_t n) {
    if (off > sizeof(flash) || n > sizeof(flash) - off)
        return -1;
    memcpy(p, flash + off, n);
    return 0;
}
static int st_erase(uint32_t off) {
    if (fail_flash || off + 4096 > sizeof(flash))
        return -1;
    memset(flash + off, 255, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *p, uint32_t n) {
    if (fail_flash || off > sizeof(flash) || n > sizeof(flash) - off)
        return -1;
    const uint8_t *s = p;
    unsigned i;
    for (i = 0; i < n; i++) {
        if ((flash[off + i] & s[i]) != s[i])
            return -1;
        flash[off + i] &= s[i];
    }
    return 0;
}
#include "../firmware/src/storage.c"
int plat_store_load(uint32_t o, void *p, uint32_t n) { return st_load(o, p, n); }
int plat_store_head(uint32_t o, void *p, uint32_t n) { return st_load_head(o, p, n); }
int plat_store_save(uint32_t o, const void *p, uint32_t n) { return st_save(o, p, n); }
#include "../firmware/src/app/project.c"
#include "../firmware/src/app/ui.c"

static const char *const button_names[] = {"FX",   "SEL", "ENV", "LFO",  "EDIT", "GLO",  "HOME",
                                           "SAVE", "ARP", "SEQ", "PLAY", "REC",  "OCT-", "OCT+"};
static const char *const enc_names[] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};
static int lookup(const char *s, const char *const *names, int n) {
    int i;
    for (i = 0; i < n; i++)
        if (!strcasecmp(s, names[i]))
            return i;
    fprintf(stderr, "Unknown control: %s\n", s);
    exit(1);
}
static FILE *wav;
static uint32_t wav_bytes;
static int peak;
static void little32(FILE *f, uint32_t v) {
    unsigned char b[] = {v, v >> 8, v >> 16, v >> 24};
    fwrite(b, 1, 4, f);
}
static void wav_close(void) {
    if (!wav)
        return;
    fseek(wav, 4, SEEK_SET);
    little32(wav, 36 + wav_bytes);
    fseek(wav, 40, SEEK_SET);
    little32(wav, wav_bytes);
    fclose(wav);
    wav = 0;
}
static void wav_open(const char *p) {
    wav_close();
    wav = fopen(p, "wb");
    if (!wav) {
        perror(p);
        exit(1);
    }
    wav_bytes = 0;
    fwrite("RIFF", 1, 4, wav);
    little32(wav, 0);
    fwrite("WAVEfmt ", 1, 8, wav);
    little32(wav, 16);
    little32(wav, 0x00020001);
    little32(wav, 44100);
    little32(wav, 44100 * 4);
    little32(wav, 0x00100004);
    fwrite("data", 1, 4, wav);
    little32(wav, 0);
}
static void *pcm;
static long (*pcm_write)(void *, const void *, unsigned long);
static int (*pcm_prepare)(void *);
static void audio_open(void) {
    void *lib = dlopen("libasound.so.2", RTLD_NOW);
    if (!lib)
        return;
    int (*open_fn)(void **, const char *, int, int) = dlsym(lib, "snd_pcm_open");
    int (*params)(void *, int, int, unsigned, unsigned, int, unsigned) =
        dlsym(lib, "snd_pcm_set_params");
    pcm_write = dlsym(lib, "snd_pcm_writei");
    pcm_prepare = dlsym(lib, "snd_pcm_prepare");
    if (!open_fn || !params || !pcm_write || !pcm_prepare || open_fn(&pcm, "default", 0, 1) < 0) {
        pcm = 0;
        return;
    }
    if (params(pcm, 2, 3, 2, 44100, 1, 100000) < 0)
        pcm = 0;
}
static void render(void) {
    int32_t out[512];
    int16_t s[512];
    unsigned i;
    gm_render(out, 256, master);
    for (i = 0; i < 512; i++) {
        int n = out[i] / 256;
        if (n > 32767)
            n = 32767;
        if (n < -32768)
            n = -32768;
        s[i] = (int16_t)n;
        if (abs(n) > peak)
            peak = abs(n);
    }
    if (wav) {
        fwrite(s, 2, 512, wav);
        wav_bytes += 1024;
    }
    if (pcm) {
        long n = pcm_write(pcm, s, 256);
        if (n < 0 && n != -11)
            pcm_prepare(pcm);
    }
}
static uint32_t sample_acc;
static void run_ms(unsigned n) {
    while (n--) {
        now_ms++;
        sample_acc += 441;
        if (sample_acc >= 2560) {
            sample_acc -= 2560;
            render();
        }
        if (now_ms % 16 == 0)
            ui_frame();
        else
            ui_input_only();
    }
}
static void boot(void) {
    buttons = keys = mr = mw = 0;
    memset(enc, 0, sizeof(enc));
    cb_lock = 0;
    if (gm_init()) {
        fprintf(stderr, "synth init failed\n");
        exit(1);
    }
    arr_init();
    project_load();
    project_apply();
    ui_init();
    app_start();
    ui_frame();
}
static void shot(const char *path) {
    ui_redraw = 1;
    ui_frame();
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fprintf(f, "P6\n240 240\n255\n");
    unsigned i;
    for (i = 0; i < 240 * 240; i++) {
        unsigned v = swap16(fb[i]);
        uint8_t rgb[] = {((v >> 11) & 31) * 255 / 31, ((v >> 5) & 63) * 255 / 63,
                         (v & 31) * 255 / 31};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static void tap(int id) {
    buttons |= 1u << id;
    run_ms(24);
    buttons &= ~(1u << id);
    run_ms(24);
}
static long value(const char *s) {
    extern void *gm_host_eas(void);
    extern int16_t gm_host_heard[16];
    if (!strcmp(s, "keyboard"))
        return proj.keyboard;
    if (!strcmp(s, "keyboard_program"))
        return gm_program[15];
    if (!strcmp(s, "keyboard_note"))
        return gm_host_heard[15];
    if (!strcmp(s, "keyboard_drums")) {
        S_EAS_DATA *eas = gm_host_eas();
        return !!(eas->pVoiceMgr->pSynth[0]->channels[15].channelFlags & CHANNEL_FLAG_RHYTHM_CHANNEL);
    }
    if (!strcmp(s, "playing"))
        return arr_playing;
    if (!strcmp(s, "tempo"))
        return song.tempo;
    if (!strcmp(s, "sections"))
        return song.count;
    if (!strcmp(s, "section"))
        return ui.section;
    if (!strcmp(s, "chords"))
        return current()->count;
    if (!strcmp(s, "chord"))
        return ui.chord;
    if (!strcmp(s, "parts"))
        return current()->nparts;
    if (!strcmp(s, "root"))
        return chord()->root;
    if (!strcmp(s, "type"))
        return chord()->type;
    if (!strcmp(s, "duration"))
        return chord()->duration;
    if (!strcmp(s, "bass"))
        return chord()->bass;
    if (!strcmp(s, "inversion"))
        return chord()->inversion;
    if (!strcmp(s, "repeat"))
        return current()->repeat;
    if (!strcmp(s, "volume"))
        return current()->parts[ui.part].volume;
    if (!strcmp(s, "flags"))
        return current()->parts[ui.part].flags;
    if (!strcmp(s, "track"))
        return current()->parts[ui.part].track;
    if (!strcmp(s, "page"))
        return ui.page;
    if (!strcmp(s, "dirty"))
        return project_dirty;
    if (!strcmp(s, "slot"))
        return proj.slot;
    if (!strcmp(s, "loop"))
        return cb_loop;
    if (!strcmp(s, "peak"))
        return peak;
    if (!strcmp(s, "note_ons"))
        return note_ons;
    if (!strcmp(s, "voices"))
        return gm_voices;
    if (!strcmp(s, "valid"))
        return cb_validate(&song);
    fprintf(stderr, "Unknown assertion: %s\n", s);
    exit(1);
}
static void script(const char *path, const char *outdir) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        exit(1);
    }
    mkdir(outdir, 0755);
    char line[512], cmd[64], a[256], b[128], out[1024];
    int lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        a[0] = b[0] = 0;
        if (sscanf(line, "%63s %255s %127s", cmd, a, b) < 1 || cmd[0] == '#')
            continue;
        if (!strcmp(cmd, "wait"))
            run_ms(atoi(a));
        else if (!strcmp(cmd, "tap"))
            tap(lookup(a, button_names, NB));
        else if (!strcmp(cmd, "press")) {
            buttons |= 1u << lookup(a, button_names, NB);
            run_ms(24);
        } else if (!strcmp(cmd, "release")) {
            buttons &= ~(1u << lookup(a, button_names, NB));
            run_ms(24);
        } else if (!strcmp(cmd, "turn") || !strcmp(cmd, "spin")) {
            enc[lookup(a, enc_names, NE)] += atoi(b);
            run_ms(32);
        } else if (!strcmp(cmd, "key")) {
            int k = atoi(a);
            if (k < 0 || k >= NKEYS)
                exit(1);
            if (!strcmp(b, "down"))
                keys |= 1u << k;
            else
                keys &= ~(1u << k);
            run_ms(24);
        } else if (!strcmp(cmd, "wav")) {
            snprintf(out, sizeof(out), "%s/%s", outdir, a);
            wav_open(out);
        } else if (!strcmp(cmd, "wavstop"))
            wav_close();
        else if (!strcmp(cmd, "shot")) {
            snprintf(out, sizeof(out), "%s/%s", outdir, a);
            shot(out);
        } else if (!strcmp(cmd, "dump")) {
            snprintf(out, sizeof(out), "%s/%s", outdir, a);
            FILE *sf = fopen(out, "wb");
            if (!sf || fwrite(&song, 1, sizeof(song), sf) != sizeof(song)) {
                perror(out);
                exit(1);
            }
            fclose(sf);
        } else if (!strcmp(cmd, "midiout")) {
            cb_midi_out = atoi(a);
        } else if (!strcmp(cmd, "midilog")) {
            if (midi_file)
                fclose(midi_file);
            snprintf(out, sizeof(out), "%s/%s", outdir, a);
            midi_file = fopen(out, "w");
        } else if (!strcmp(cmd, "peakreset"))
            peak = 0;
        else if (!strcmp(cmd, "flash_fail"))
            fail_flash = atoi(a);
        else if (!strcmp(cmd, "reboot"))
            boot();
        else if (!strcmp(cmd, "demo")) {
            int d = atoi(a);
            if (d < 0 || d >= (int)cb_ndemos)
                exit(1);
            arr_play(0);
            run_ms(32);
            song = cb_demos[d];
            ui_init();
            project_dirty = 1;
            run_ms(32);
        } else if (!strcmp(cmd, "expect")) {
            long got = value(a), want = strtol(b + (b[0] == '>' || b[0] == '<'), 0, 10);
            int ok = b[0] == '>' ? got > want : b[0] == '<' ? got < want : got == want;
            if (!ok) {
                fprintf(stderr, "%s:%d: %s expected %s, got %ld\n", path, lineno, a, b, got);
                exit(1);
            }
        } else {
            fprintf(stderr, "%s:%d: unknown command %s\n", path, lineno, cmd);
            exit(1);
        }
    }
    fclose(f);
    wav_close();
    printf("PASS %s (peak %d, notes %u)\n", path, peak, note_ons);
}
static uint64_t milliseconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static void interactive(const char *store) {
    Display *d = XOpenDisplay(0);
    if (!d) {
        fprintf(stderr, "Cannot open X11 display\n");
        exit(1);
    }
    int screen = DefaultScreen(d);
    Window win = XCreateSimpleWindow(d, RootWindow(d, screen), 0, 0, 860, 520, 0, 0, 0x253e49);
    XStoreName(d, win, "ChordBot FM-1 - mouse wheel: encoders, Space: play, F1-F12: buttons");
    XSelectInput(d, win,
                 ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                     ButtonReleaseMask | StructureNotifyMask);
    Atom close = XInternAtom(d, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(d, win, &close, 1);
    XMapWindow(d, win);
    GC gc = XCreateGC(d, win, 0, 0);
    XImage *im = XCreateImage(d, DefaultVisual(d, screen), DefaultDepth(d, screen), ZPixmap, 0,
                              calloc(480 * 480, 4), 480, 480, 32, 0);
    audio_open();
    uint64_t last = milliseconds();
    unsigned drawn = ~frames_drawn;
    int quit = 0;
    while (!quit) {
        while (XPending(d)) {
            XEvent e;
            XNextEvent(d, &e);
            if (e.type == ClientMessage && (Atom)e.xclient.data.l[0] == close)
                quit = 1;
            if (e.type == Expose)
                drawn = ~frames_drawn;
            if (e.type == KeyPress || e.type == KeyRelease) {
                int on = e.type == KeyPress;
                KeySym sym = XLookupKeysym(&e.xkey, 0);
                int id = -1;
                if (sym == XK_space)
                    id = B_PLAY;
                if (sym >= XK_F1 && sym <= XK_F12)
                    id = sym - XK_F1;
                if (sym == XK_Escape)
                    id = B_HOME;
                if (sym == XK_Left)
                    id = B_OCTDN;
                if (sym == XK_Right)
                    id = B_OCTUP;
                if (on && sym == XK_Up)
                    enc[EN_SELECT]--;
                if (on && sym == XK_Down)
                    enc[EN_SELECT]++;
                if (sym == XK_Return)
                    id = B_SEL;
                if (id >= 0) {
                    if (on)
                        buttons |= 1u << id;
                    else
                        buttons &= ~(1u << id);
                }
            }
            if (e.type == ButtonPress || e.type == ButtonRelease) {
                int x = e.xbutton.x, y = e.xbutton.y, on = e.type == ButtonPress;
                if (x >= 510 && x < 846 && y >= 16 && y < 226) {
                    int id = ((y - 16) / 30) * 2 + (x - 510) / 168;
                    if (id < NB && e.xbutton.button == Button1) {
                        if (on)
                            buttons |= 1u << id;
                        else
                            buttons &= ~(1u << id);
                    }
                }
                if (on && x >= 510 && y >= 248 && y < 458) {
                    int id = (y - 248) / 30;
                    enc[id] += e.xbutton.button == Button4   ? 1
                               : e.xbutton.button == Button5 ? -1
                               : x < 678                     ? -1
                                                             : 1;
                }
            }
        }
        uint64_t now = milliseconds();
        unsigned elapsed = now - last;
        if (elapsed > 100)
            elapsed = 100;
        run_ms(elapsed);
        last = now;
        if (drawn != frames_drawn) {
            drawn = frames_drawn;
            unsigned x, y;
            for (y = 0; y < 480; y++)
                for (x = 0; x < 480; x++) {
                    unsigned v = swap16(fb[y / 2 * 240 + x / 2]);
                    XPutPixel(im, x, y,
                              ((v >> 11) * 255 / 31) << 16 | ((v >> 5 & 63) * 255 / 63) << 8 |
                                  (v & 31) * 255 / 31);
                }
            XPutImage(d, win, gc, im, 0, 0, 16, 16, 480, 480);
            XSetForeground(d, gc, 0xe0fff8);
            for (x = 0; x < NB; x++) {
                XDrawRectangle(d, win, gc, 510 + (x % 2) * 168, 16 + (x / 2) * 30, 160, 26);
                XDrawString(d, win, gc, 520 + (x % 2) * 168, 34 + (x / 2) * 30, button_names[x],
                            strlen(button_names[x]));
            }
            for (x = 0; x < NE; x++) {
                char label[80];
                snprintf(label, sizeof(label), "-      %s       +", enc_names[x]);
                XDrawRectangle(d, win, gc, 510, 248 + x * 30, 328, 26);
                XDrawString(d, win, gc, 520, 266 + x * 30, label, strlen(label));
            }
            const char *hint = "Wheel or click -/+ to turn a knob";
            XDrawString(d, win, gc, 510, 486, hint, strlen(hint));
            XFlush(d);
        }
        struct timespec delay = {0, 1000000};
        nanosleep(&delay, 0);
    }
    FILE *f = fopen(store, "wb");
    if (f) {
        fwrite(flash, 1, sizeof(flash), f);
        fclose(f);
    }
    XDestroyImage(im);
    XFreeGC(d, gc);
    XDestroyWindow(d, win);
    XCloseDisplay(d);
}
int main(int argc, char **argv) {
    memset(flash, 255, sizeof(flash));
    int live = argc > 1 && !strcmp(argv[1], "--interactive");
    const char *store = argc > 2 ? argv[2] : "build/host/flash.bin";
    if (live) {
        FILE *f = fopen(store, "rb");
        if (f) {
            (void)fread(flash, 1, sizeof(flash), f);
            fclose(f);
        }
    }
    boot();
    if (live)
        interactive(store);
    else if (argc == 3)
        script(argv[1], argv[2]);
    else {
        fprintf(stderr, "Usage: %s SCRIPT OUTDIR | --interactive [FLASH]\n", argv[0]);
        return 1;
    }
    return 0;
}
