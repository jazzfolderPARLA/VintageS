/* SPDX-License-Identifier: GPL-3.0-only */
/* JUPITER-8 MINI: the voice of the Jupiter-8 port for the FM-1 (a port of jupiter8_esp32's engine.cpp to fixed point;
 * GPL-3.0-only), renamed j8_ / J8_ and cut to the voice: two VCOs (triangle / saw / pulse / square and sine / saw /
 * pulse / noise) with polyBLEP, hard sync and cross modulation, a one-pole HPF, the four-pole OTA ladder with its tanh
 * per stage, two RC envelopes, the LFO, the VCA, the analog tolerances and drift. The key assigner, layers, portamento,
 * bender and mod wheel, arpeggiator, sequencer, effects and master stage of the port are not part of it: this
 * firmware allocates the voices and hands over the pitch. Plain C (no firmware types), so the host tests can run it.
 *
 * Formats:  audio and filter state Q24 (1.0 = 1 << 24); envelopes Q30; phases and VCO increments full turn = 2^32;
 * octaves Q16. Control every J8_BLOCK = 32 samples (this firmware's CTL); the per-block values are ramped sample by
 * sample. Output: ADDED to the buffer as (voice * J8_GAIN) >> J8_OUT_SHIFT: Q16 for this firmware (1.0 = 65536, the
 * port's default VOLUME), Q24 as the port itself with J8_GAIN 65536 and J8_OUT_SHIFT 23 (the host test). */
#pragma once
#include <stdint.h>
#include "ju8_params.h"
#include "ju8_tables.h"

#define J8_NV 8
#define J8_BLOCK 32
#define J8_ONE24 (1 << 24)
#define J8_ONE30 (1 << 30)
#define J8_DRV(d) ((((d) >> 3) * 9) >> 1)                  /* x 0.5625: the OTA drive before the tanh */
#define J8_MAXINC 1932735283u                       /* 0.45 * 2^32 */
#define J8_C(x) ((int32_t)((x) * 65536.0))
#define J8_VINTAGE 128                              /* the port's VINTAGE at its default: the tolerances at 1.0 */
#ifndef J8_GAIN
#define J8_GAIN 30840                               /* 0.47 (Q16): the port's VOLUME 200 x its 0.6 master factor */
#endif
#ifndef J8_OUT_SHIFT
#define J8_OUT_SHIFT 31                             /* the 23 of the port's output, 8 more for Q24 -> Q16 */
#endif

typedef struct {
    uint8_t active, fresh, st1, st2;
    uint32_t ph1, ph2;
    int32_t nxt2;                                   /* VCO-2's output, one sample late (hard sync corrections) */
    int32_t env1, env2;                             /* Q30 */
    int32_t lfoGain;                                /* Q16 */
    int32_t z[4], hpz;
    int32_t gPrev, invPrev, kPrev, compPrev, hpPrev, vcaPrev;
    uint32_t inc1Prev, inc2Prev, pwPrev;
    int32_t o2Last;
    int32_t drift1, drift2, driftCut;               /* cents, Q16 */
    int32_t tolP1, tolP2, tolCut;                   /* cents, Q16 */
    int32_t invTolEnv;                              /* Q16 */
    uint32_t rng;
    int32_t vel;                                    /* Q15 */
} j8_voice_t;

/* a sound's part: the LFO and the slewed controls (once per block), the parameters as the track's EDIT knobs leave them,
 * and its voices */
typedef struct {
    int32_t lfo;                                    /* Q24 */
    uint32_t lfoPhase;
    int32_t lfoHold;
    int32_t cutS, hpfS, mixS, lvlS, xmS, pwS;       /* Q16 (cutS, hpfS in parameter units << 16) */
    int32_t resS;                                   /* Q24 */
    uint32_t lfo_rng;
    uint8_t first, ready;
    uint8_t pe[J8_NP];                              /* the sound's parameters with the EDIT offsets */
    j8_voice_t voice[J8_NV];
} j8_part_t;

static inline void j8_clear(void *p, unsigned n) { uint8_t *b = (uint8_t *)p; while (n--) *b++ = 0; }

/* ------------------------------------------------------------------ maths */
static inline int32_t j8_mulq(int32_t a, int32_t b, int sh) { return (int32_t)(((int64_t)a * b) >> sh); }
static inline int32_t j8_clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline uint32_t j8_rand(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *s = x;
}
static inline int32_t j8_rand_bip24(uint32_t *s) { return (int32_t)j8_rand(s) >> 7; }       /* +-1.0, Q24 */

/* tanh(x), Q24 in and out, table with linear interpolation */
static inline int32_t j8_sat(int32_t x)
{
    uint32_t p, i;
    int32_t a, b;
    if (x >= (4 << 24)) return J8_ONE24;
    if (x <= -(4 << 24)) return -J8_ONE24;
    p = (uint32_t)(x + (4 << 24));
    i = p >> 18;
    a = J8_SAT[i]; b = J8_SAT[i + 1];
    return a + (int32_t)(((int64_t)(b - a) * (int32_t)(p & 0x3FFFFu)) >> 18);
}

/* 2^x for x in octaves Q16 -> Q16 (saturates) */
static inline uint32_t j8_exp2_q16(int32_t x)
{
    int32_t n = x >> 16;
    uint32_t fr = (uint32_t)x & 0xFFFFu, i = fr >> 8, f = fr & 255u;
    uint32_t a = J8_EXP2P[i], b = J8_EXP2P[i + 1], v = a + (((b - a) * f) >> 8);
    if (n >= 15) return 0x7FFFFFFFu;
    if (n >= 0) return v << n;
    return n <= -31 ? 0u : v >> -n;
}

/* 2^-m for m = octaves Q24 (>= 0) -> Q30: the per-block coefficient of an exponential segment */
static inline int32_t j8_exp2n_q30(uint32_t m)
{
    uint32_t n = m >> 24, fr = m & 0xFFFFFFu, i = fr >> 16;
    int32_t a = (int32_t)J8_EXP2N[i], b = (int32_t)J8_EXP2N[i + 1];
    int32_t v = a + (int32_t)(((int64_t)(b - a) * (int32_t)(fr & 0xFFFFu)) >> 16);
    return n >= 31 ? 0 : v >> n;
}

/* phase increment of 440 Hz * 2^o (o octaves Q16) */
static inline uint32_t j8_inc(int32_t o)
{
    int32_t n = o >> 16;
    uint32_t fr = (uint32_t)o & 0xFFFFu, i = fr >> 8, f = fr & 255u;
    uint32_t a = J8_INC[i], b = J8_INC[i + 1], v = a + (((b - a) * f) >> 8);
    if (n > 5) return J8_MAXINC;
    if (n >= 0) { uint64_t w = (uint64_t)v << n; return w > J8_MAXINC ? J8_MAXINC : (uint32_t)w; }
    v = n <= -31 ? 0u : v >> -n;
    return v < 1u ? 1u : v;
}

/* g = T / (1 + T), T = tan(pi fc / fs), for log2(fc) in Q16 (a one-pole's coefficient, Q30) */
static inline int32_t j8_g_of(int32_t lg)
{
    int32_t x = lg - (J8_G_L0 << 16);
    uint32_t i, f, a, b;
    if (x < 0) x = 0;
    if (x > (J8_G_N - 2) * 2048) x = (J8_G_N - 2) * 2048;
    i = (uint32_t)x >> 11; f = (uint32_t)x & 2047u;
    a = J8_G[i]; b = J8_G[i + 1];
    return (int32_t)(a + (uint32_t)(((int64_t)((int32_t)b - (int32_t)a) * (int32_t)f) >> 11));
}

static inline int32_t j8_sine_turn(uint32_t ph)       /* cos(2 pi ph / 2^32), Q24: the original's sinTurn(0) is +1 */
{
    uint32_t i;
    ph += 0x40000000u;
    i = ph >> 24;
    int32_t a = J8_SINE[i], b = J8_SINE[i + 1];
    return a + (int32_t)(((int64_t)(b - a) * (int32_t)((ph >> 8) & 0xFFFFu)) >> 16);
}

/* ------------------------------------------------------------- oscillators */
/* polyBLEP residual of a unit step around a wrap of the phase (Q24, -1..0 after the wrap, 0..1 before it) */
static inline int32_t j8_blep(uint32_t ph, uint32_t inc)
{
    uint32_t d = inc >> 15;
    int32_t x;
    if (!d) return 0;
    if (ph < inc) {
        x = (int32_t)(ph / d);                         /* t, Q15 (above 1.0 only for a VCO below ~1 Hz) */
        if (x > 32767) x = 32767;
        return (int32_t)((x + x - ((x * x) >> 15) - 32768)) * 512;
    }
    if (ph > 0xFFFFFFFFu - inc) {
        x = -(int32_t)((0xFFFFFFFFu - ph) / d);
        if (x < -32767) x = -32767;
        return (int32_t)(((x * x) >> 15) + x + x + 32768) * 512;
    }
    return 0;
}

static inline int32_t j8_saw(uint32_t ph, uint32_t inc)  { return (int32_t)(ph >> 7) - J8_ONE24 - j8_blep(ph, inc); }
static inline int32_t j8_saw_plain(uint32_t ph)          { return (int32_t)(ph >> 7) - J8_ONE24; }
static inline int32_t j8_tri(uint32_t ph)                /* +1 at 0, -1 at the half turn */
{
    int32_t p = (int32_t)(ph >> 8);
    return p < (1 << 23) ? J8_ONE24 - 4 * p : 4 * p - 3 * J8_ONE24;
}
static inline int32_t j8_sine_shaped(uint32_t ph)        /* the original's analog-style sine: t (1.5 - 0.5 t^2) */
{
    int32_t t = j8_tri(ph), t2 = j8_mulq(t, t, 24);
    return (int32_t)(((int64_t)t * ((3 << 24) - t2)) >> 25);
}
enum { J8W_TRI, J8W_SAW, J8W_PULSE, J8W_SINE, J8W_NOISE };
static inline int32_t j8_wave(int w, uint32_t ph, uint32_t inc, uint32_t pw, int blep)
{
    switch (w) {
    case J8W_TRI:  return j8_tri(ph);
    case J8W_SAW:  return blep ? j8_saw(ph, inc) : j8_saw_plain(ph);
    case J8W_PULSE:
        return blep ? j8_saw(ph - pw, inc) - j8_saw(ph, inc) : j8_saw_plain(ph - pw) - j8_saw_plain(ph);   /* high for the first pw of the cycle */
    default:      return j8_sine_shaped(ph);
    }
}

/* ---------------------------------------------------------------- envelope */
/* RC-style exponential segments, one step per block (see engine.cpp stepEnv). f: Q16 rate factor
 * (> 1 shortens the times: key follow and the voice's tolerance) */
static inline int32_t j8_env_coef(uint32_t l, uint32_t f)
{
    return j8_exp2n_q30((uint32_t)(((uint64_t)l * f) >> 16));
}
static int32_t j8_env_step(int32_t e, uint8_t *st, const uint8_t *P, int a, int d, int s, int r, uint32_t f)
{
    const int32_t sus = (int32_t)P[s] * 4210752;
    switch (*st) {
    case 0: {
        const int32_t top = 0x50000000;                /* 1.25: the RC charges towards it and stops at 1.0 */
        e = top + j8_mulq(e - top, j8_env_coef(J8_ATT[P[a]], f), 30);
        if (e >= J8_ONE30) { e = J8_ONE30; *st = 1; }
        break;
    }
    case 1:
        e = sus + j8_mulq(e - sus, j8_env_coef(J8_DEC[P[d]], f), 30);
        if (e - sus < 536871) *st = 2;                 /* 0.0005 */
        break;
    case 2:
        e += j8_mulq(sus - e, 3277, 16);                /* 0.05 */
        break;
    default:
        e = j8_mulq(e, j8_env_coef(J8_DEC[P[r]], f), 30);
        break;
    }
    return e;
}


/* ------------------------------------------------------------- per block */
#define J8_SMOOTH(x, t) ((x) += (((t) - (x)) * 5) >> 6)       /* 0.08: about 8 ms */
/* the part's first block: the voices' tolerances (the port's per-card offsets), noise seeds and start phases */
static void j8_part_init(j8_part_t *g)
{
    static const int32_t kP1[8] = {J8_C(0.6), J8_C(-1.1), J8_C(0.3), J8_C(1.4), J8_C(-0.5), J8_C(0.9), J8_C(-1.3), J8_C(0.2)};
    static const int32_t kP2[8] = {J8_C(-0.8), J8_C(0.5), J8_C(1.2), J8_C(-0.2), J8_C(1.0), J8_C(-1.4), J8_C(0.4), J8_C(-0.6)};
    static const int32_t kC[8] = {J8_C(4), J8_C(-7), J8_C(2), J8_C(9), J8_C(-5), J8_C(6), J8_C(-9), J8_C(1)};   /* cents */
    static const int32_t kE[8] = {J8_C(1.0 / 1.01), J8_C(1.0 / 0.98), J8_C(1.0 / 1.02), J8_C(1.0 / 0.99),
                                  J8_C(1.0 / 1.03), J8_C(1.0 / 0.97), J8_C(1.0), J8_C(1.0 / 1.02)};
    int i;
    j8_clear(g, sizeof *g);
    g->lfo_rng = 0x9E3779B9u;
    g->first = 1;
    g->ready = 1;
    for (i = 0; i < J8_NV; i++) {
        j8_voice_t *v = &g->voice[i];
        v->tolP1 = kP1[i & 7]; v->tolP2 = kP2[i & 7]; v->tolCut = kC[i & 7]; v->invTolEnv = kE[i & 7];
        v->rng = 0x9E3779B1u * (uint32_t)(i + 1) + 77u;
        v->ph1 = (uint32_t)i * (uint32_t)(0.137 * 4294967296.0);
        v->ph2 = (uint32_t)i * (uint32_t)(0.291 * 4294967296.0);
        v->st1 = v->st2 = 3;
        v->lfoGain = 65536;
    }
}

/* once per block, on g->pe: the LFO and the slewed controls (the port's ju_update_layer without its bender section) */
static void j8_update(j8_part_t *c)
{
    const uint8_t *v = c->pe;
    uint32_t prev = c->lfoPhase;
    int32_t tg, lvl;

    c->lfoPhase += J8_LFO_INC[v[J8_P_LFO_RATE]];
    switch (v[J8_P_LFO_WAVE]) {
    case J8_LFO_SINE: c->lfo = j8_sine_turn(c->lfoPhase); break;
    case J8_LFO_SAW: c->lfo = J8_ONE24 - (int32_t)(c->lfoPhase >> 7); break;
    case J8_LFO_SQUARE: c->lfo = c->lfoPhase < 0x80000000u ? J8_ONE24 : -J8_ONE24; break;
    default:
        if (c->lfoPhase < prev) c->lfoHold = j8_rand_bip24(&c->lfo_rng);
        c->lfo = c->lfoHold;
        break;
    }

    tg = (int32_t)J8_RES[v[J8_P_VCF_RES]];
    lvl = (int32_t)v[J8_P_VCA_LEVEL] * 257;
    lvl = j8_mulq(lvl, lvl, 16);
    if (c->first) {
        c->cutS = (int32_t)v[J8_P_VCF_CUT] << 16; c->resS = tg; c->hpfS = (int32_t)v[J8_P_HPF] << 16;
        c->mixS = (int32_t)v[J8_P_MIX] * 257; c->lvlS = lvl; c->xmS = J8_XMOD[v[J8_P_V1_XMOD]];
        c->pwS = (int32_t)v[J8_P_VM_PW] * 257;
        c->first = 0;
    } else {
        int32_t t;
        t = (int32_t)v[J8_P_VCF_CUT] << 16; J8_SMOOTH(c->cutS, t);
        J8_SMOOTH(c->resS, tg);
        t = (int32_t)v[J8_P_HPF] << 16; J8_SMOOTH(c->hpfS, t);
        t = (int32_t)v[J8_P_MIX] * 257; J8_SMOOTH(c->mixS, t);
        J8_SMOOTH(c->lvlS, lvl);
        t = J8_XMOD[v[J8_P_V1_XMOD]]; J8_SMOOTH(c->xmS, t);
        t = (int32_t)v[J8_P_VM_PW] * 257; J8_SMOOTH(c->pwS, t);
    }
}

/* a note on a voice (the port's ju_trigger, its envelopes restart from where they are: an analog retrigger). vel: Q15 */
static void j8_trigger(j8_voice_t *v, int32_t vel)
{
    v->st1 = 0; v->st2 = 0;
    v->vel = vel;
    v->lfoGain = 0;
    if (!v->active) {
        v->active = 1;
        v->env1 = v->env2 = 0;
        v->z[0] = v->z[1] = v->z[2] = v->z[3] = 0;
        v->hpz = 0; v->vcaPrev = 0; v->nxt2 = 0;
        v->fresh = 1;
    }
}

/* ------------------------------------------------------------- one voice */
/* One control block (J8_BLOCK samples) of one voice, ADDED to out. pitch: the note's pitch in semitones, Q16 (this
 * firmware's: glide, tuning, bend, LFO and unison detune are in it); note: the key (key follow of the filter and the
 * envelope times); gate: the key is down. */
static void j8_render_voice(const j8_part_t *c, j8_voice_t *v, int note, int32_t pitch, int gate, int32_t *out)
{
    const uint8_t *P = c->pe;
    const int32_t vintage = J8_VINTAGE;                              /* /128 */
    uint32_t f1 = v->invTolEnv, f2 = v->invTolEnv;
    int32_t e1, lfo, delayStep, base, o1, o2, lfoO, envO, m1, m2, cutO, lg, gNew, kNew, g2, g4, den, invNew, compNew;
    int32_t hpNew, vcaNew, trem, pwv;
    uint32_t inc1, inc2, pwNew;
    int dest, n;

    /* key follow on the envelope times: longer below C4, shorter above (2^(keyRel / 2)) */
    if (P[J8_P_E1_KEY] || P[J8_P_E2_KEY]) {
        uint32_t kt = j8_exp2_q16((J8_NOTE_OCT[note] + 49152) >> 1);   /* ((note - 60) / 12) / 2: (n-69)/12 + 0.75 */
        if (P[J8_P_E1_KEY]) f1 = (uint32_t)(((uint64_t)f1 * kt) >> 16);
        if (P[J8_P_E2_KEY]) f2 = (uint32_t)(((uint64_t)f2 * kt) >> 16);
    }
    if (!gate) { v->st1 = 3; v->st2 = 3; }                          /* the key is up: both envelopes release */
    v->env1 = j8_env_step(v->env1, &v->st1, P, J8_P_E1_A, J8_P_E1_D, J8_P_E1_S, J8_P_E1_R, f1);
    v->env2 = j8_env_step(v->env2, &v->st2, P, J8_P_E2_A, J8_P_E2_D, J8_P_E2_S, J8_P_E2_R, f2);
    if (!gate && v->st2 == 3 && v->env2 < 53687) {              /* 0.00005 */
        v->active = 0; v->env1 = v->env2 = 0;
        return;
    }
    e1 = P[J8_P_E1_POL] ? -v->env1 : v->env1;

    /* LFO delay: the amplitude ramps up after key-on */
    delayStep = (int32_t)J8_LFO_DLY[P[J8_P_LFO_DELAY]];
    v->lfoGain += delayStep;
    if (v->lfoGain > 65536) v->lfoGain = 65536;
    lfo = j8_mulq(c->lfo, v->lfoGain, 16);

    /* slow pitch drift: a random walk with a leak, in cents (Q16) */
    v->drift1 += (j8_rand_bip24(&v->rng) >> 8) * 1966 / 65536 - j8_mulq(v->drift1, 39, 16);       /* 0.03 */
    v->drift2 += (j8_rand_bip24(&v->rng) >> 8) * 1966 / 65536 - j8_mulq(v->drift2, 39, 16);
    v->driftCut += (j8_rand_bip24(&v->rng) >> 8) * 3277 / 65536 - j8_mulq(v->driftCut, 39, 16);    /* 0.05 */

    /* pitch, in octaves relative to A4 (Q16) */
    base = (int32_t)(((int64_t)(pitch - (69 << 16)) * 87381) >> 20);
    dest = P[J8_P_VM_DEST];
    lfoO = j8_mulq(lfo, J8_VCO_LFO[P[J8_P_VM_LFO]], 24);
    envO = j8_mulq(e1, J8_VCO_ENV[P[J8_P_VM_ENV]], 30);
    m1 = dest == J8_DEST_VCO2 ? 0 : lfoO + envO;
    m2 = dest == J8_DEST_VCO1 ? 0 : lfoO + envO;
#define J8_CENTS_OCT(cq) ((int32_t)(((int64_t)(cq) * 3495) >> 22))
    o1 = base + (((int32_t)P[J8_P_V1_RANGE] - 1) * 65536) + m1 +
         J8_CENTS_OCT(j8_mulq(v->tolP1 + v->drift1, vintage, 7));
    if (P[J8_P_V2_LOW])                                                  /* LOW FREQ: fixed rate, ignores the key */
        o2 = (int32_t)(-10.7814 * 65536.0) + (2 << 16) * (int32_t)P[J8_P_V2_RANGE] + J8_V2TUNE[P[J8_P_V2_TUNE]] * 4 + m2;
    else
        o2 = base + (((int32_t)P[J8_P_V2_RANGE] - 1) * 65536) + m2 + J8_V2TUNE[P[J8_P_V2_TUNE]] +
             J8_CENTS_OCT(j8_mulq(v->tolP2 + v->drift2, vintage, 7));
    inc1 = j8_inc(o1);
    inc2 = j8_inc(o2);

    /* VCF cutoff: log2(fc) = 4 + 10.2877 cut / 255 + modulation (Q16 octaves) */
    {
        int32_t env4f = P[J8_P_VCF_ENVSEL] ? v->env2 : e1;
        int32_t keyAmt = ((int32_t)P[J8_P_VCF_KEY] * 19738) >> 6;      /* 1.2 * key / 255, Q16 */
        lg = (4 << 16) + (int32_t)(((int64_t)c->cutS * 2644) >> 16);
        cutO = j8_mulq(env4f, J8_VCF_ENV[P[J8_P_VCF_ENV]], 30) + j8_mulq(lfo, J8_VCF_LFO[P[J8_P_VCF_LFO]], 24) +
               j8_mulq(J8_NOTE_OCT[note] + 49152, keyAmt, 16) +
               J8_CENTS_OCT(j8_mulq(v->tolCut + v->driftCut, vintage, 7));
        lg += cutO;
    }
    gNew = j8_g_of(lg);                                                  /* Q30 */
    kNew = c->resS;                                                      /* Q24 */
    g2 = j8_mulq(gNew, gNew, 30);
    g4 = j8_mulq(g2, g2, 30);
    den = J8_ONE24 + j8_mulq(kNew, g4, 30);                              /* Q24 */
    invNew = (int32_t)(((1u << 31) / (uint32_t)(den >> 9)) << 14);       /* 1 / (1 + k g^4), Q30 */
    compNew = J8_ONE24 + j8_mulq(kNew, 358, 10);                           /* 1 + 0.35 k */

    /* HPF: one pole, no resonance. 6 Hz at 0, then 16 Hz * 2^(7.6439 hpf / 255) */
    if (c->hpfS < (1 << 15)) hpNew = j8_g_of((int32_t)(2.585 * 65536.0));
    else hpNew = j8_g_of((4 << 16) + (int32_t)(((int64_t)c->hpfS * 1965) >> 16));

    /* VCA */
    {
        static const int32_t kTrem[4] = {0, J8_C(0.3), J8_C(0.6), J8_C(1.0)};
        trem = 65536 - j8_mulq(kTrem[P[J8_P_VCA_LFO] & 3], (32768 - (lfo >> 9)), 16);
        vcaNew = j8_mulq(v->env2 >> 6, c->lvlS, 16);                     /* Q24 */
        vcaNew = j8_mulq(vcaNew, trem, 16);
        vcaNew = (int32_t)(((int64_t)vcaNew * v->vel) >> 15);
        vcaNew = j8_mulq(vcaNew, J8_GAIN, 16);                           /* this firmware's level (Q16 out, not Q24) */
    }

    /* pulse width: the high part, a fraction of the cycle */
    pwv = c->pwS;                                                        /* Q16 0..1 */
    switch (P[J8_P_VM_PWMODE]) {
    case J8_PW_MAN: pwNew = (uint32_t)(((int64_t)(32768 - j8_mulq(pwv, 29491, 16)) << 16)); break;           /* 0.5 - 0.45 pw */
    case J8_PW_LFO: pwNew = (uint32_t)((int64_t)(32768 - j8_mulq(j8_mulq(pwv, 29491, 16), 32768 + (lfo >> 9), 16)) << 16); break;
    default: pwNew = (uint32_t)((int64_t)(32768 - j8_mulq(j8_mulq(pwv, 29491, 16), j8_clamp(v->env1 >> 14, 0, 65536), 16)) << 16); break;
    }

    if (v->fresh) {                                                      /* a voice starting from silence: no ramp from old values */
        v->fresh = 0;
        v->inc1Prev = inc1; v->inc2Prev = inc2; v->gPrev = gNew; v->invPrev = invNew;
        v->kPrev = kNew; v->compPrev = compNew; v->hpPrev = hpNew; v->pwPrev = pwNew;
    }

    {
        const int w1 = (const int[4]){J8W_TRI, J8W_SAW, J8W_PULSE, J8W_PULSE}[P[J8_P_V1_WAVE] & 3];
        const int w2 = (const int[4]){J8W_SINE, J8W_SAW, J8W_PULSE, J8W_NOISE}[P[J8_P_V2_WAVE] & 3];
        const int sq1 = (P[J8_P_V1_WAVE] & 3) == J8_V1_SQUARE;
        const int sync = P[J8_P_V2_SYNC] != 0 && w2 != J8W_NOISE;
        const int32_t xm = c->xmS;
        const int xmOn = xm > 33;                                        /* 0.0005 */
        const int32_t mix2 = c->mixS, mix1 = 65536 - mix2;
        const int tap2 = P[J8_P_VCF_SLOPE] == 0;
        int32_t dI1 = ((int32_t)inc1 - (int32_t)v->inc1Prev) >> 5, dI2 = ((int32_t)inc2 - (int32_t)v->inc2Prev) >> 5;
        int32_t dG = (gNew - v->gPrev) >> 5, dInv = (invNew - v->invPrev) >> 5, dK = (kNew - v->kPrev) >> 5;
        int32_t dC = (compNew - v->compPrev) >> 5, dH = (hpNew - v->hpPrev) >> 5, dV = (vcaNew - v->vcaPrev) >> 5;
        uint32_t dP = (uint32_t)((int32_t)(pwNew - v->pwPrev) >> 5);      /* unsigned: 50 % is 2^31 */
        int32_t i1 = (int32_t)v->inc1Prev, i2 = (int32_t)v->inc2Prev, G = v->gPrev, inv = v->invPrev, K = v->kPrev;
        int32_t comp = v->compPrev, hpG = v->hpPrev, vca = v->vcaPrev;
        uint32_t pw = v->pwPrev;
        int32_t o2Last = v->o2Last, z1 = v->z[0], z2 = v->z[1], z3 = v->z[2], z4 = v->z[3], hpz = v->hpz;
        int32_t Gd, dGd;
        uint32_t ph1 = v->ph1, ph2 = v->ph2;
        int32_t nxt2 = v->nxt2;
        /* G / drv with drv = 0.5625 (9 / 16): the OTA stage's input is scaled before the tanh and the output back */
        Gd = (int32_t)(((int64_t)G * 58254) >> 16);                      /* G x 8/9: Q30 / 0.5625 as Q29 */
        dGd = ((int32_t)(((int64_t)gNew * 58254) >> 16) - Gd) >> 5;

        for (n = 0; n < J8_BLOCK; n++) {
            int32_t s1, s2, x, hv, hl, S, u, y, y2 = 0, y4 = 0, vv;
            uint32_t incm = (uint32_t)i1, old;
            int wrapped;
            i1 += dI1; i2 += dI2; G += dG; Gd += dGd; inv += dInv; K += dK; comp += dC;
            hpG += dH; vca += dV; pw += dP;

            /* VCO-1, with cross mod from VCO-2's last output */
            incm = (uint32_t)i1;
            if (xmOn) {
                uint64_t m = ((uint64_t)incm * j8_exp2_q16(j8_mulq(xm, o2Last, 24))) >> 16;
                incm = m > J8_MAXINC ? J8_MAXINC : (uint32_t)m;
            }
            old = ph1; ph1 += incm; wrapped = ph1 < old;
            s1 = j8_wave(w1, ph1, incm, sq1 ? 0x80000000u : (uint32_t)pw, 1);

            /* VCO-2, hard-synced to VCO-1; its output is one sample late so the sync step can be corrected */
            if (w2 == J8W_NOISE) {
                s2 = j8_mulq(j8_rand_bip24(&v->rng), 179, 8);            /* 0.7 */
                ph2 += (uint32_t)i2;
            } else if (sync && wrapped) {
                uint32_t p = ph1, d = incm, sq15, ph2pre;
                int32_t before, after, dd, thisb, nextb;
                if (d == 0) d = 1;
                while (!(d & 0x80000000u)) { d <<= 1; p <<= 1; }
                sq15 = (p >> 1) / (d >> 16);                             /* time since the wrap, Q15 of a sample */
                if (sq15 > 32768u) sq15 = 32768u;
                ph2pre = ph2 + (uint32_t)(((uint64_t)(uint32_t)i2 * (32768u - sq15)) >> 15);
                before = j8_wave(w2, ph2pre, (uint32_t)i2, (uint32_t)pw, 0);
                after = j8_wave(w2, 0, (uint32_t)i2, (uint32_t)pw, 0);
                dd = after - before;
                thisb = (int32_t)(((int64_t)sq15 * sq15) >> 16);         /* 0.5 t^2, Q15 */
                nextb = -(int32_t)((((int64_t)(32768 - sq15) * (32768 - sq15))) >> 16);
                s2 = nxt2 + (int32_t)(((int64_t)dd * thisb) >> 15);
                ph2 = (uint32_t)(((uint64_t)(uint32_t)i2 * sq15) >> 15);
                nxt2 = j8_wave(w2, ph2, (uint32_t)i2, (uint32_t)pw, 0) + (int32_t)(((int64_t)dd * nextb) >> 15);
                goto s2done;
            } else {
                ph2 += (uint32_t)i2;
            }
            if (w2 != J8W_NOISE) {
                s2 = nxt2;
                nxt2 = j8_wave(w2, ph2, (uint32_t)i2, (uint32_t)pw, 1);
            }
        s2done:
            o2Last = s2;

            /* mixer into the HPF (one-pole TPT high-pass); the pulses carry no DC */
            x = (int32_t)(((int64_t)s1 * mix1 + (int64_t)s2 * mix2) >> 16);
            hv = j8_mulq(x - hpz, hpG, 30);
            hl = hv + hpz;
            hpz = hl + hv;
            x = (x - hl) >> 1;

            /* VCF: four OTA integrators in series, zero-delay feedback from stage 4 */
            S = j8_mulq((1 << 30) - G, z4 + j8_mulq(G, z3 + j8_mulq(G, z2 + j8_mulq(G, z1, 30), 30), 30), 30);
            u = j8_mulq((int32_t)(((int64_t)x * comp - (int64_t)K * S) >> 24), inv, 30);
            vv = j8_mulq(Gd, j8_sat(J8_DRV(u - z1)), 29); y = vv + z1; z1 = y + vv;
            vv = j8_mulq(Gd, j8_sat(J8_DRV(y - z2)), 29); y = vv + z2; z2 = y + vv; y2 = y;
            vv = j8_mulq(Gd, j8_sat(J8_DRV(y - z3)), 29); y = vv + z3; z3 = y + vv;
            vv = j8_mulq(Gd, j8_sat(J8_DRV(y - z4)), 29); y = vv + z4; z4 = y + vv; y4 = y;
            out[n] += (int32_t)(((int64_t)(tap2 ? y2 : y4) * vca) >> J8_OUT_SHIFT);     /* x 2: the makeup gain */
        }
        v->ph1 = ph1; v->ph2 = ph2; v->nxt2 = nxt2;
        v->o2Last = o2Last;
        v->z[0] = z1; v->z[1] = z2; v->z[2] = z3; v->z[3] = z4; v->hpz = hpz;
    }
    v->inc1Prev = inc1; v->inc2Prev = inc2; v->gPrev = gNew; v->invPrev = invNew;
    v->kPrev = kNew; v->compPrev = compNew; v->hpPrev = hpNew; v->vcaPrev = vcaNew; v->pwPrev = pwNew;
}
