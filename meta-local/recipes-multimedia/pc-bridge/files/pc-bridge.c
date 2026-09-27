/*
 * pc-bridge: CC 3 -> Program Change for FluidSynth, via ALSA sequencer.
 *
 * - Subscribes to every readable MIDI port (except Midi Through / FluidSynth)
 *   and follows hotplug via the System:Announce port.
 * - Connects its own output port to FluidSynth (client name containing
 *   "FLUID"); reconnects when FluidSynth restarts.
 * - CC PC_CC on channel N -> Program Change N <value>. Real Program Change
 *   from controllers is not touched (it goes to FluidSynth directly).
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

static snd_seq_t *seq;
static int me, in_port, out_port;
static int last[16];

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
    printf("pc-bridge: CC %d -> Program Change (client %d)\n", PC_CC, me);

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
            if (ev->data.control.param == PC_CC)
                send_pc(ev->data.control.channel, ev->data.control.value);
            break;
        default:
            break;
        }
    }
    return 0;
}
