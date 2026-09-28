/*
 * pc-bridge: CC 3 -> Program Change for FluidSynth, via ALSA sequencer.
 *
 * - Subscribes to every readable MIDI port (except Midi Through / FluidSynth)
 *   and follows hotplug via the System:Announce port.
 * - Connects its own output port to FluidSynth (client name containing
 *   "FLUID"); reconnects when FluidSynth restarts.
 * - CC PC_CC on channel N -> Program Change N <value>. Real Program Change
 *   from controllers is not touched (it goes to FluidSynth directly).
 * - CCs listed in nrpn_map (ADSR, filter, vibrato rate, reverb/chorus send)
 *   -> SoundFont 2.01 NRPN on the same channel: CC 99 = 120, CC 98 =
 *   generator, CC 38 / CC 6 = 14-bit data. FluidSynth adds
 *   (data - 8192) * nrpn_scale to the instrument's own generator value.
 *
 * Build: $CC pc-bridge.c -o pc-bridge -lasound
 */
#define _GNU_SOURCE
#include <alsa/asoundlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifndef PC_CC
#define PC_CC 3            /* CC number translated to Program Change */
#endif
#ifndef STEP
#define STEP 1             /* 1 = 128 programs, 8 = 16 steps */
#endif
#ifndef FS_MATCH
#define FS_MATCH "FLUID"   /* substring of FluidSynth's ALSA client name */
#endif

/*
 * Knob 0..127 -> offset lo..hi in the generator's own units; scale is the
 * generator's nrpn_scale from fluid_gen.c (FluidSynth 2.3.4).
 */
struct nrpn_map {
    int cc, gen, scale, lo, hi;
};

static const struct nrpn_map nrpn_map[] = {
    { 73, 34, 2,     0, 14400 },  /* attack: timecents, 0 = original, up to ~x4096 */
    { 75, 36, 2, -7200,  7200 },  /* decay: timecents, 64 = original */
    { 79, 37, 1,   400,     0 },  /* sustain: cB of attenuation, 127 = original, 0 = -40 dB */
    { 72, 38, 2, -7200,  7200 },  /* release: timecents, 64 = original */
    { 74,  8, 2, -9600,     0 },  /* cutoff: cents, 127 = original (usually open) */
    { 71,  9, 1,     0,   240 },  /* resonance: cB, 0 = original */
    { 76, 24, 4, -2400,  2400 },  /* vibrato LFO rate: cents, 64 = original */
    /* reverb/chorus: 0 dries instruments with up to 20 % own send; a lower
     * lo leaves a dead zone on instruments that have 0 % of their own */
    { 91, 16, 1,  -200,  1000 },  /* reverb send: 0.1 % */
    { 93, 15, 1,  -200,  1000 },  /* chorus send: 0.1 % */
};

static snd_seq_t *seq;
static int me, in_port, out_port;
static int last[16];

static void send_nrpn(int ch, const struct nrpn_map *m, int value);

/*
 * Start dry: put reverb and chorus send on every channel where knob 0
 * would put them, so instruments with their own send in the SF2 are dry
 * too and the first knob move does not jump.
 */
static void send_dry_effects(void)
{
    size_t i;
    int ch;

    for (i = 0; i < sizeof(nrpn_map) / sizeof(nrpn_map[0]); i++) {
        if (nrpn_map[i].cc != 91 && nrpn_map[i].cc != 93)
            continue;
        for (ch = 0; ch < 16; ch++)
            send_nrpn(ch, &nrpn_map[i], 0);
    }
    printf("reverb/chorus send -> knob 0 on all channels\n");
}

static void handle_port(int c, int p)
{
    snd_seq_client_info_t *ci;
    snd_seq_port_info_t *pi;
    const char *cname;
    unsigned int cap;

    if (c == me || c == SND_SEQ_CLIENT_SYSTEM)
        return;

    snd_seq_client_info_alloca(&ci);
    snd_seq_port_info_alloca(&pi);
    if (snd_seq_get_any_client_info(seq, c, ci) < 0 ||
        snd_seq_get_any_port_info(seq, c, p, pi) < 0)
        return;

    cname = snd_seq_client_info_get_name(ci);
    cap = snd_seq_port_info_get_capability(pi);
    if (cap & SND_SEQ_PORT_CAP_NO_EXPORT)
        return;

    if (strcasestr(cname, FS_MATCH)) {
        const unsigned int w = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
        if ((cap & w) == w && snd_seq_connect_to(seq, out_port, c, p) >= 0) {
            memset(last, -1, sizeof(last));   /* new synth -> forget state */
            printf("out -> %s (%d:%d)\n", cname, c, p);
            send_dry_effects();
        }
        return;
    }

    if (strcasestr(cname, "Midi Through"))
        return;

    {
        const unsigned int r = SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
        if ((cap & r) == r && snd_seq_connect_from(seq, in_port, c, p) >= 0)
            printf("in  <- %s (%d:%d)\n", cname, c, p);
    }
}

static void scan_all(void)
{
    snd_seq_client_info_t *ci;
    snd_seq_port_info_t *pi;

    snd_seq_client_info_alloca(&ci);
    snd_seq_port_info_alloca(&pi);
    snd_seq_client_info_set_client(ci, -1);
    while (snd_seq_query_next_client(seq, ci) >= 0) {
        int c = snd_seq_client_info_get_client(ci);
        snd_seq_port_info_set_client(pi, c);
        snd_seq_port_info_set_port(pi, -1);
        while (snd_seq_query_next_port(seq, pi) >= 0)
            handle_port(c, snd_seq_port_info_get_port(pi));
    }
}

static void send_pc(int ch, int value)
{
    snd_seq_event_t ev;
    int prog = (STEP > 1) ? (value / STEP) * STEP : value;

    if (prog < 0) prog = 0;
    if (prog > 127) prog = 127;
    ch &= 15;
    if (last[ch] == prog)
        return;

    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, out_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_ev_set_pgmchange(&ev, ch, prog);
    if (snd_seq_event_output_direct(seq, &ev) >= 0) {
        last[ch] = prog;
        printf("ch %d -> program %d\n", ch + 1, prog);
    }
}

static void send_cc(int ch, int param, int value)
{
    snd_seq_event_t ev;

    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, out_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_ev_set_controller(&ev, ch, param, value);
    snd_seq_event_output_direct(seq, &ev);
}

/*
 * The full sequence goes out on every knob move: FluidSynth resets the
 * selected generator after each data entry. CC 6 must come last, it is
 * the one that applies (MSB << 7) + the stored CC 38.
 * No log line here: a knob sweep would flood the RAM log.
 */
static void send_nrpn(int ch, const struct nrpn_map *m, int value)
{
    int offset = m->lo + value * (m->hi - m->lo) / 127;
    int data = 8192 + offset / m->scale;

    if (data < 0) data = 0;
    if (data > 16383) data = 16383;
    ch &= 15;

    send_cc(ch, 99, 120);            /* NRPN MSB: SoundFont generator */
    send_cc(ch, 98, m->gen);         /* NRPN LSB: generator number (< 100) */
    send_cc(ch, 38, data & 127);     /* data entry LSB */
    send_cc(ch, 6, data >> 7);       /* data entry MSB: applies the value */
}

static void handle_cc(int ch, int param, int value)
{
    size_t i;

    if (param == PC_CC) {
        send_pc(ch, value);
        return;
    }
    for (i = 0; i < sizeof(nrpn_map) / sizeof(nrpn_map[0]); i++) {
        if (nrpn_map[i].cc == param) {
            send_nrpn(ch, &nrpn_map[i], value);
            return;
        }
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    memset(last, -1, sizeof(last));

    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        fprintf(stderr, "cannot open ALSA sequencer\n");
        return 1;
    }
    snd_seq_set_client_name(seq, "pc-bridge");
    me = snd_seq_client_id(seq);

    in_port = snd_seq_create_simple_port(seq, "in",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    /* NO_EXPORT: other auto-connect scripts should leave this port alone */
    out_port = snd_seq_create_simple_port(seq, "out",
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_NO_EXPORT,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (in_port < 0 || out_port < 0) {
        fprintf(stderr, "cannot create ports\n");
        return 1;
    }

    /* hotplug notifications */
    snd_seq_connect_from(seq, in_port, SND_SEQ_CLIENT_SYSTEM,
                         SND_SEQ_PORT_SYSTEM_ANNOUNCE);
    scan_all();
    printf("pc-bridge: CC %d -> Program Change, %d CCs -> NRPN (client %d)\n",
           PC_CC, (int)(sizeof(nrpn_map) / sizeof(nrpn_map[0])), me);

    for (;;) {
        snd_seq_event_t *ev;
        int r = snd_seq_event_input(seq, &ev);

        if (r == -ENOSPC || r == -EAGAIN)   /* input overrun / nothing */
            continue;
        if (r < 0) {
            fprintf(stderr, "event_input: %s\n", snd_strerror(r));
            continue;
        }
        switch (ev->type) {
        case SND_SEQ_EVENT_PORT_START:
            handle_port(ev->data.addr.client, ev->data.addr.port);
            break;
        case SND_SEQ_EVENT_CONTROLLER:
            handle_cc(ev->data.control.channel, ev->data.control.param,
                      ev->data.control.value);
            break;
        default:
            break;
        }
    }
    return 0;
}
