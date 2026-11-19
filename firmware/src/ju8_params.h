/* SPDX-License-Identifier: GPL-3.0-only */
/* JUPITER-8 MINI: the voice's parameters (the Jupiter-8 port's ju_params.h: same ids, same order, same selector
 * values, so the factory sounds carry over), without the effects, the arpeggiator, the sequencer and the global
 * panel settings. A sound is a byte per parameter. */
#pragma once
#include <stdint.h>

enum {
    J8_P_LFO_RATE, J8_P_LFO_DELAY, J8_P_LFO_WAVE,
    J8_P_VM_LFO, J8_P_VM_ENV, J8_P_VM_DEST, J8_P_VM_PW, J8_P_VM_PWMODE,
    J8_P_V1_XMOD, J8_P_V1_RANGE, J8_P_V1_WAVE,
    J8_P_V2_SYNC, J8_P_V2_RANGE, J8_P_V2_LOW, J8_P_V2_TUNE, J8_P_V2_WAVE,
    J8_P_MIX, J8_P_HPF,
    J8_P_VCF_CUT, J8_P_VCF_RES, J8_P_VCF_SLOPE, J8_P_VCF_ENV, J8_P_VCF_ENVSEL, J8_P_VCF_LFO, J8_P_VCF_KEY,
    J8_P_VCA_LEVEL, J8_P_VCA_LFO,
    J8_P_E1_A, J8_P_E1_D, J8_P_E1_S, J8_P_E1_R, J8_P_E1_KEY, J8_P_E1_POL,
    J8_P_E2_A, J8_P_E2_D, J8_P_E2_S, J8_P_E2_R, J8_P_E2_KEY,
    J8_P_ASSIGN,                                /* (kept so the sounds' bytes line up; only SOLO is read, by the preset's MONO flag) */
    J8_NP
};
typedef char j8_np_is_39[J8_NP == 39 ? 1 : -1];     /* the Jupiter-8 sounds are 39 parameters */

enum { J8_LFO_SINE, J8_LFO_SAW, J8_LFO_SQUARE, J8_LFO_RANDOM };
enum { J8_DEST_VCO1, J8_DEST_BOTH, J8_DEST_VCO2 };
enum { J8_PW_LFO, J8_PW_MAN, J8_PW_ENV1 };
enum { J8_V1_TRI, J8_V1_SAW, J8_V1_PULSE, J8_V1_SQUARE };
enum { J8_V2_SINE, J8_V2_SAW, J8_V2_PULSE, J8_V2_NOISE };
enum { J8_AS_SOLO, J8_AS_UNISON, J8_AS_POLY1, J8_AS_POLY2 };

/* INIT (the port's ju_patch_init) */
static void j8_patch_init(uint8_t *v)
{
    uint32_t i;
    for (i = 0; i < J8_NP; i++) v[i] = 0;
    v[J8_P_LFO_RATE] = 150; v[J8_P_LFO_WAVE] = J8_LFO_SINE;
    v[J8_P_VM_DEST] = J8_DEST_BOTH; v[J8_P_VM_PW] = 0; v[J8_P_VM_PWMODE] = J8_PW_MAN;
    v[J8_P_V1_RANGE] = 1; v[J8_P_V1_WAVE] = J8_V1_SAW;
    v[J8_P_V2_RANGE] = 1; v[J8_P_V2_TUNE] = 128; v[J8_P_V2_WAVE] = J8_V2_SAW;
    v[J8_P_MIX] = 128;
    v[J8_P_VCF_CUT] = 200; v[J8_P_VCF_SLOPE] = 1; v[J8_P_VCF_ENVSEL] = 1; v[J8_P_VCF_KEY] = 128;
    v[J8_P_VCA_LEVEL] = 220;
    v[J8_P_E1_A] = 0; v[J8_P_E1_D] = 120; v[J8_P_E1_S] = 128; v[J8_P_E1_R] = 120;
    v[J8_P_E2_A] = 0; v[J8_P_E2_D] = 120; v[J8_P_E2_S] = 255; v[J8_P_E2_R] = 110;
    v[J8_P_ASSIGN] = J8_AS_POLY1;
}
