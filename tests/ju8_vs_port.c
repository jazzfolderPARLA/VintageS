/* SPDX-License-Identifier: GPL-3.0-only */
/* Regression: the Jupiter-8 port's original engine (ju_dsp.c from src_jupiter_mini) against this firmware's
 * ju8_dsp.h, sample by sample: INIT and the 64 factory sounds, a note and a chord, 1500 blocks each, key up at 700.
 * Expected: every run identical, except the two UNISON sounds with a chord (the original's key assigner re-pitches
 * the voices; this firmware allocates them itself).
 *
 *   cd <src_jupiter_mini>/src
 *   gcc -O1 -w -DJ8_GAIN=65536 -DJ8_OUT_SHIFT=23 -I. -I<VintageS>/src -o ju8_vs_port <this file> ju_fxdef.c
 *   ./ju8_vs_port
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* the port's engine (original, as uploaded) */
#include "ju_dsp.c"
/* this firmware's port */
#include "ju8_dsp.h"
#include "ju8_presets.h"

static int run(const uint8_t *v, const int *notes, int nn, int blocks, int off_at, long *ndiff, int32_t *maxd, double *peak)
{
    static int32_t up[32], lo[32], mine[32];
    static j8_part_t g;
    int b, k, i, bad = 0, vn[J8_NV];
    for (k = 0; k < J8_NV; k++) vn[k] = -1;
    ju_init();
    ju_global[G_KEYMODE] = KM_WHOLE;                /* all eight voices, one layer */
    memcpy(ju_patch[0].v, v, J8_NP);
    j8_clear(&g, sizeof g); j8_part_init(&g);
    memcpy(g.pe, v, J8_NP);
    ju_prepare_block(); j8_update(&g);             /* the first block of the original starts the key mode: no notes yet */
    for (k = 0; k < nn; k++) ju_note_on(0, (uint8_t)notes[k]);
    for (k = 0; k < J8_NV; k++) if (ju_v[k].active) { vn[k] = ju_v[k].note; j8_trigger(&g.voice[k], 32768); }
    for (b = 0; b < blocks; b++) {
        int gate = b < off_at;
        if (b == off_at) for (k = 0; k < nn; k++) ju_note_off(0, (uint8_t)notes[k]);
        memset(up, 0, sizeof up); memset(lo, 0, sizeof lo); memset(mine, 0, sizeof mine);
        ju_prepare_block(); ju_render(up, lo);
        j8_update(&g);
        for (k = 0; k < J8_NV; k++) if (vn[k] >= 0 && g.voice[k].active) j8_render_voice(&g, &g.voice[k], vn[k], vn[k] << 16, gate, mine);
        for (i = 0; i < 32; i++) {
            int32_t d = up[i] - mine[i];
            if (d < 0) d = -d;
            if (d > *maxd) *maxd = d;
            if (d) { (*ndiff)++; bad = 1; }
            if ((double)abs(up[i]) > *peak) *peak = abs(up[i]);
        }
    }
    return bad;
}
int main(void)
{
    int sets[3][3] = {{60,0,0},{60,64,67},{72,0,0}};
    int nns[3] = {1,3,1}, p, s, nbad = 0;
    long total = 0;
    uint8_t init[J8_NP];
    long nd; int32_t md; double pk;
    j8_patch_init(init);
    for (p = -1; p < (int)J8_NFACT; p++) {
        const uint8_t *v = p < 0 ? init : J8_FACT[p].v;
        for (s = 0; s < 3; s++) {
            nd = 0; md = 0; pk = 0;
            if (run(v, sets[s], nns[s], 1500, 700, &nd, &md, &pk)) {
                nbad++; printf("DIFF patch %d set %d: %ld samples differ, max %d\n", p, s, nd, md);
            }
            total++;
        }
    }
    printf("%ld runs (INIT + 64 sounds x 3 note sets, 1500 blocks each), %d with a difference\n", total, nbad);
    return nbad != 0;
}
