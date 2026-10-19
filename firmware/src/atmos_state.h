/* SPDX-License-Identifier: GPL-3.0-only */
/* ATMOS: state kept across power-off (performance mode, split, banks, layer B).
 *
 * Where it is stored: favorites.factory[8], bytes 2..15 (row 8 is GRAIN, retired in engines.c: no engine
 * reaches it; its old stars use bytes 0..1 and the FX key map bytes 16..25 and 28..31 stay untouched).
 * No size or magic change in persist_t (604 bytes), so settings already saved in the device stay valid.
 *
 *   byte  2 mode        never restored: power-on is always SINGLE (0 is written here)
 *   byte  3 split_point MIDI note 0..127: notes below it play layer B
 *   byte  4 bank        active bank of layer A (0 DX7, 1 PROPHET-5, 2 JUNO-106, 3 CZ/SID)
 *   byte  5 preset      active preset of layer A, 0..127 (shown as 1..128)
 *   byte  6 b_bank      bank of layer B
 *   byte  7 b_preset    preset of layer B, 0..127
 *   bytes 8..11 last[]  last preset used in each bank
 *   bytes 12..15        tag 'A','T','M',1: without it, or with a bad byte, that value takes its default
 * Power-on always starts in SINGLE mode: DUAL and SPLIT are never restored. The split point and layer B's
 * sound are remembered, so they are ready when the mode is turned on again.
 * MIDI channel and clock sync are NOT here: Melodee already keeps them (ext.glo, "GLO_KEPT"). */
#pragma once
#include <stdint.h>                  /* (the firmware has no libc: no string.h, no memcmp / memcpy here) */

enum { ATMOS_SINGLE, ATMOS_DUAL, ATMOS_SPLIT, ATMOS_NMODE };
#define ATMOS_NBANK 4u
#define ATMOS_ENGI_SID 3u               /* the SID engine's number in engines.c ENGINES[] (no ENGI_ name): bank D with CZ-1 */
#define ATMOS_NPRE  128u
#define ATMOS_SPLIT_DEFAULT 60u          /* middle C */

/* MIDI messages accepted by each input (clock and active sensing are dropped before this): the header's activity dots
 * (ui_draw.c) light for a moment when either one changes. Written by the MIDI readers, read by the UI. */
static volatile uint32_t midi_rx_usb, midi_rx_trs;

typedef struct {
    uint8_t mode, split_point, bank, preset, b_bank, b_preset, last[ATMOS_NBANK];
} atmos_t;

static atmos_t atmos = {ATMOS_SINGLE, ATMOS_SPLIT_DEFAULT, 0, 0, 0, 0, {0, 0, 0, 0}};

#define ATMOS_ROW      8u                /* favorites.factory[ATMOS_ROW] */
#define ATMOS_DATA     2u
#define ATMOS_TAGPOS   12u
static const uint8_t ATMOS_TAG[4] = {'A', 'T', 'M', 1};

/* row: favorites.factory[ATMOS_ROW] of the stored record. Each value is checked on its own. */
static void atmos_load(const uint8_t *row)
{
    atmos_t a = {ATMOS_SINGLE, ATMOS_SPLIT_DEFAULT, 0, 0, 0, 0, {0, 0, 0, 0}};
    uint32_t tag_ok = 1;
    for (uint32_t i = 0; i < sizeof ATMOS_TAG; i++)
        if (row[ATMOS_TAGPOS + i] != ATMOS_TAG[i])
            tag_ok = 0;
    if (tag_ok) {
        const uint8_t *d = row + ATMOS_DATA;
        /* d[0], the mode, is ignored on purpose: power-on is always SINGLE */
        if (d[1] < 128u) a.split_point = d[1];
        if (d[2] < ATMOS_NBANK && d[3] < ATMOS_NPRE) { a.bank = d[2]; a.preset = d[3]; }
        if (d[4] < ATMOS_NBANK && d[5] < ATMOS_NPRE) { a.b_bank = d[4]; a.b_preset = d[5]; }
        for (uint32_t i = 0; i < ATMOS_NBANK; i++) a.last[i] = d[6 + i] < ATMOS_NPRE ? d[6 + i] : 0;
    }
    atmos = a;
}

/* Writes bytes 2..15 of the row and nothing else. */
static void atmos_save(uint8_t *row)
{
    uint8_t *d = row + ATMOS_DATA;
    d[0] = ATMOS_SINGLE; d[1] = atmos.split_point;   /* the mode is not kept */
    d[2] = atmos.bank;  d[3] = atmos.preset;
    d[4] = atmos.b_bank; d[5] = atmos.b_preset;
    for (uint32_t i = 0; i < ATMOS_NBANK; i++) d[6 + i] = atmos.last[i];
    for (uint32_t i = 0; i < sizeof ATMOS_TAG; i++)
        row[ATMOS_TAGPOS + i] = ATMOS_TAG[i];
}

/* Which layers a key plays: bit 0 = layer A (the selected track), bit 1 = layer B.
 * SINGLE: A. DUAL: A and B (each layer takes its own voices). SPLIT: below split_point B, from it up A. */
static inline uint32_t atmos_route(uint32_t note)
{
    switch (atmos.mode) {
    case ATMOS_DUAL:  return 3u;
    case ATMOS_SPLIT: return note < atmos.split_point ? 2u : 1u;
    default:          return 1u;
    }
}

/* Layer B plays on the track after the selected one (wraps over the n parts). */
static inline uint32_t atmos_b_track(uint32_t sel, uint32_t n) { return (sel + 1u) % n; }

/* SEQ short press: SINGLE -> DUAL -> SPLIT -> SINGLE */
static inline void atmos_mode_next(void) { atmos.mode = (uint8_t)((atmos.mode + 1u) % ATMOS_NMODE); }

/* PLAY: the view (tap) and the metronome (held). view: 0 the sounds (Stage as usual), 1 the monitor (the wave large, the output
 * level, the sounds with their bank, number and star, the mode, the voices). The metronome clicks from the audio block (mix.c atmos_click) at bpm, 4 beats to
 * the bar, the first one higher; tick counts the beats (the PLAY LED and the header flash on it). */
#define ATMOS_VIEW_N 2u
#define ATMOS_BPM_MIN 30u
#define ATMOS_BPM_MAX 250u
typedef struct {
    volatile uint8_t on;
    uint8_t view;
    volatile uint16_t bpm;
    volatile uint32_t tick;
    volatile uint32_t per;                              /* samples a beat: set with the tempo (the audio block cannot divide) */
    uint32_t pos, beat, ph;
    int32_t env;
} atmos_met_t;
static atmos_met_t atmos_met = {0, 0, 120, 0, 22050, 0, 0, 0, 0};   /* (22050: 120 bpm at 44.1 kHz) */


/* The output's peak since the UI last read it (mix.c fills it after the master, the monitor's VU meter empties it), and its
 * level in 20 segments of 3 dB: 0 dBFS at the top, -57 dB at the bottom. */
static volatile uint32_t atmos_vu_peak;
#define ATMOS_VU_N 20u
static const uint16_t ATMOS_VU_THR[ATMOS_VU_N] = {46, 65, 92, 130, 184, 260, 368, 519, 734, 1036, 1464, 2067, 2920, 4125, 5827, 8231, 11626, 16422, 23197, 32767};   /* 32767 * 10^(-3 (19 - i) / 20) */
static uint32_t atmos_vu_level(uint32_t peak)
{
    uint32_t i, n = 0;
    for (i = 0; i < ATMOS_VU_N; i++)
        if (peak >= ATMOS_VU_THR[i])
            n++;
    return n;
}
