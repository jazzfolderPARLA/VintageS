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
#include <stdint.h>
#include <string.h>

enum { ATMOS_SINGLE, ATMOS_DUAL, ATMOS_SPLIT, ATMOS_NMODE };
#define ATMOS_NBANK 4u
#define ATMOS_NPRE  128u
#define ATMOS_SPLIT_DEFAULT 60u          /* middle C */

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
    if (!memcmp(row + ATMOS_TAGPOS, ATMOS_TAG, sizeof ATMOS_TAG)) {
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
    memcpy(row + ATMOS_TAGPOS, ATMOS_TAG, sizeof ATMOS_TAG);
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
