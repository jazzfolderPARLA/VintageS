/* SPDX-License-Identifier: GPL-3.0-only */
/* OBERVAVE: the sound engine of OberVave (github.com/zvenson/obervave, GPL-3.0-only): two oscillators with sync and
 * X-MOD, a 12 dB state-variable or 24 dB ladder filter, two analog-curve envelopes, an LFO, vintage drift. Its DSP
 * (obv_dsp.h, obv_tables.h) and its parameters and factory sounds (obv_params.h, obv_patches.h) are OberVave's
 * own, renamed with an obv_ / OBV_ prefix; this file is the glue: a voice of this firmware is a card of the
 * OberVave. This firmware allocates the voices (POLY / MONO / LEGATO / UNISON, GLIDE and the pitch it hands over),
 * the OberVave's envelopes are the voice's amplitude (ownenv), the sound is mono (this firmware spreads and pans).
 * OberVave's arpeggiator, sequencer, mixer and effects are not part of it. OberVave runs its control every 16
 * samples; this firmware every CTL: obv_block and obv_render step the OberVave's 16 CTL / OBV_CTL times. */
#include "obv_params.h"
#include "obv_dsp.h"
#include "obv_patches.h"

#define OBV_NV 8u                               /* the voices (OberVave's eight cards) */
#define OBV_STEPS (CTL / OBV_CTL)               /* its control blocks per block of this firmware */
#if (CTL % OBV_CTL) != 0
#error "CTL must be a multiple of the OberVave's 16"
#endif
#define OBV_ENV_ONE (1 << 24)
#define OBV_LEVEL_FLOOR 2048                    /* -78 dB: a release below this frees the voice */
#define OBV_NCARD 8u

enum { OBV_E_IDLE, OBV_E_ATT, OBV_E_DEC, OBV_E_SUS, OBV_E_REL };
typedef struct { uint32_t stage; int32_t level; } obv_env_t;   /* level Q24 */

typedef struct {
    obv_osc_t o1, o2;
    uint32_t noise;
    obv_env_t fe, ae;
    obv_svf_state_t f1;
    obv_svf_coef_t c1;
    obv_lad_state_t lad;
    obv_lad_coef_t lc;
    uint32_t inc1, inc2, pw1, pw2;              /* this block's oscillator settings */
    int32_t amp0, amp1;                         /* Q15 at the block's start and end */
    int32_t drift;                              /* the vintage random walk */
    uint32_t lfo_blk;                           /* blocks since the note on (LFO delay / fade) */
    int32_t lfo_fade;                           /* Q15 */
} obv_voice_t;

/* the sound's derived numbers (once per OberVave block; the first OBV_STEPS of each of these are the block's) */
typedef struct {
    uint32_t lfo_ph, lfo_rnd_a, lfo_rnd_b, rnd;
    int32_t cut_sm, reso_sm;                    /* CUTOFF and RESO slewed (Q8) */
    int32_t lfo_val[OBV_STEPS];
    uint32_t lfo_phs[OBV_STEPS], lfo_ra[OBV_STEPS], lfo_rb[OBV_STEPS];   /* the LFO's phase and random pair at each step */
    int32_t cut[OBV_STEPS], reso_k[OBV_STEPS], lad_k[OBV_STEPS];
    int32_t fenv_amt, keytrk, velflt, lfo_pd, lfo_fd, lfo_pwd, lfo_ad, wheel_q, env_o2, det_fine8;
    int32_t lvl1, lvl2, lvln, drive_q12, vint, sus_f, sus_a, gain_q12, morph_q;
    uint32_t pw1_base, pw2_base, w1, w2, ftype, sync_on, xmod_q;
    uint8_t ready;
    obv_voice_t voice[OBV_NV];
} obv_part_t;

static obv_part_t *obv_part(uint32_t part);

/* the sound of each track: OberVave's patch (a byte per parameter and the name); RAM, with the pool */
static obv_patch_t obv_patch[NPART] __attribute__((section(".pool")));
static uint8_t obv_patch_ready[NPART];
static obv_patch_t *obv_patch_of(const track_t *t)
{
    uint32_t k = (uint32_t)(t - trk) % NPART;
    if (!obv_patch_ready[k]) { obv_patch_defaults(&obv_patch[k]); obv_patch_ready[k] = 1; }
    return &obv_patch[k];
}
static inline obv_voice_t *obv_voice(track_t *t, voice_t *v)
{
    obv_part_t *part = obv_part((uint32_t)(t - trk));
    return part ? &part->voice[(uint32_t)(v - t->v) % OBV_NV] : 0;
}

/* ---- envelopes: analog RC curves per block, level Q24 */
static inline void obv_env_tick(obv_env_t *e, uint32_t a, uint32_t d, int32_t sus, uint32_t r, int32_t gate, int32_t vint)
{
    int32_t k, target;
    if (gate && e->stage >= OBV_E_REL) e->stage = OBV_E_ATT;
    if (!gate && e->stage && e->stage < OBV_E_REL) e->stage = OBV_E_REL;
    switch (e->stage) {
    case OBV_E_ATT:
        k = (int32_t)OBV_ENV_K_ATT[a];
        k += (int32_t)(((int64_t)k * vint) >> 7);
        target = OBV_ENV_ONE + (OBV_ENV_ONE * 3 >> 3);   /* aims at 1.3, stops at 1.0 */
        e->level += (int32_t)(((int64_t)(target - e->level) * k) >> 24);
        if (e->level >= OBV_ENV_ONE) { e->level = OBV_ENV_ONE; e->stage = OBV_E_DEC; }
        break;
    case OBV_E_DEC:
        k = (int32_t)OBV_ENV_K_DEC[d];
        k += (int32_t)(((int64_t)k * vint) >> 7);
        e->level += (int32_t)(((int64_t)(sus - e->level) * k) >> 24) - 16;
        if (e->level <= sus + 64) { e->level = sus; e->stage = OBV_E_SUS; }
        break;
    case OBV_E_SUS:
        e->level = sus;
        break;
    case OBV_E_REL:
        k = (int32_t)OBV_ENV_K_DEC[r];
        k += (int32_t)(((int64_t)k * vint) >> 7);
        e->level += (int32_t)(((int64_t)(0 - e->level) * k) >> 24) - 32;
        if (e->level < OBV_LEVEL_FLOOR) { e->level = 0; e->stage = OBV_E_IDLE; }
        break;
    default:
        e->level = 0;
    }
}

/* ---- the LFO (per sound; per voice a phase offset in SPREAD and a delay / fade) */
static int32_t obv_lfo_shape(uint32_t rnd_a, uint32_t rnd_b, uint32_t ph, uint32_t shape)
{
    int32_t p;
    switch (shape) {
    case OBV_L_TRI:
        p = (int32_t)((ph + 0x40000000u) >> 15);
        return p < 65536 ? p - 32768 : 98303 - p;
    case OBV_L_SAW: return 32767 - (int32_t)(ph >> 16);
    case OBV_L_RAMP: return (int32_t)(ph >> 16) - 32768;
    case OBV_L_SQR: return ph < 0x80000000u ? 32767 : -32767;
    case OBV_L_SH: return (int32_t)(rnd_b >> 16) - 32768;
    default: {                                  /* smooth random: a line from one random value to the next */
        int32_t a = (int32_t)(rnd_a >> 16) - 32768, b = (int32_t)(rnd_b >> 16) - 32768;
        return a + (int32_t)(((int64_t)(b - a) * (ph >> 8)) >> 24);
    }
    }
}

/* The track's live offsets on the sound (the EDIT knobs): CUT, RES, ENV AMT and RELEASE, the oscillator 2 SEMI, PWM
 * of both, X-MOD and DRIVE. Added to the patch's bytes, clamped to their ranges. */
static inline int32_t obv_pv(const track_t *t, const uint8_t *pv, uint32_t id, int32_t off)
{
    return obv_clampi((int32_t)pv[id] + off, 0, (int32_t)OBV_PDEF[id].max);
}
#define OBV_OFF(t, k) ((int32_t)(t)->p[P_E0 + (k)])

/* once per block of this firmware and sound (engine_t.block): the LFO, the slewed cutoff and resonance, and the
 * numbers derived from the patch. */
static void obv_block(track_t *t)
{
    obv_part_t *g = obv_part((uint32_t)(t - trk));
    const uint8_t *pv = obv_patch_of(t)->v;
    uint32_t i;
    int32_t d, cut_t, reso_t, reso_k;
    if (!g) return;
    if (!g->ready) {                            /* the first block of this sound */
        uint32_t k;
        g->ready = 1;
        g->rnd = 0x9E3779B9u ^ ((uint32_t)(t - trk) * 0x85EBCA6Bu);
        g->lfo_rnd_a = obv_xorshift(&g->rnd);
        g->lfo_rnd_b = obv_xorshift(&g->rnd);
        g->cut_sm = (int32_t)pv[OBV_P_CUTOFF] << 8;
        g->reso_sm = (int32_t)pv[OBV_P_RESO] << 8;
        for (k = 0; k < OBV_NV; k++) {
            obv_voice_t *s = &g->voice[k];
            s->noise = 0x12345678u * (k + 1u) + 0x9E3779B9u;
            s->o1.ph = obv_xorshift(&s->noise);     /* free-running oscillators start anywhere */
            s->o2.ph = obv_xorshift(&s->noise);
        }
    }
    cut_t = obv_pv(t, pv, OBV_P_CUTOFF, OBV_OFF(t, 0)) << 8;
    reso_t = obv_pv(t, pv, OBV_P_RESO, OBV_OFF(t, 1)) << 8;
    for (i = 0; i < OBV_STEPS; i++) {
        uint32_t inc = OBV_LFO_INC[pv[OBV_P_LFO_RATE]], old = g->lfo_ph;
        g->lfo_ph += inc;
        if (g->lfo_ph < old) { g->lfo_rnd_a = g->lfo_rnd_b; g->lfo_rnd_b = obv_xorshift(&g->rnd); }
        g->lfo_phs[i] = g->lfo_ph;
        g->lfo_ra[i] = g->lfo_rnd_a;
        g->lfo_rb[i] = g->lfo_rnd_b;
        g->lfo_val[i] = obv_lfo_shape(g->lfo_rnd_a, g->lfo_rnd_b, g->lfo_ph, pv[OBV_P_LFO_SHAPE]);
        g->cut_sm += (cut_t - g->cut_sm) >> 6;  /* tau = 64 blocks = 23 ms */
        g->reso_sm += (reso_t - g->reso_sm) >> 6;
        reso_k = 8192 - g->reso_sm * 8400 / (127 * 256);
        g->cut[i] = g->cut_sm;
        g->reso_k[i] = reso_k;
        g->lad_k[i] = (8192 - reso_k) >> 1;     /* the ladder's feedback, Q10: 0 .. 4.1 */
    }
    g->fenv_amt = obv_pv(t, pv, OBV_P_FENV_AMT, OBV_OFF(t, 2)) - 64;
    g->keytrk = pv[OBV_P_KEYTRK];
    g->velflt = pv[OBV_P_VEL_FLT];
    d = pv[OBV_P_LFO_PITCH]; g->lfo_pd = (d * d) >> 7;
    d = pv[OBV_P_LFO_FLT]; g->lfo_fd = (d * d) >> 7;
    d = obv_pv(t, pv, OBV_P_LFO_PWM, 0); g->lfo_pwd = (d * d) >> 7;
    g->lfo_ad = pv[OBV_P_LFO_AMP];
    g->wheel_q = pv[OBV_P_MOD_DEPTH];
    g->env_o2 = (int32_t)pv[OBV_P_ENV_O2] - 64;
    g->det_fine8 = ((int32_t)pv[OBV_P_O2_FINE] - 64) * 2;
    d = pv[OBV_P_O1_LVL]; g->lvl1 = (d * d) >> 7;
    d = pv[OBV_P_O2_LVL]; g->lvl2 = (d * d) >> 7;
    d = pv[OBV_P_NOISE]; g->lvln = (d * d) >> 7;
    d = obv_pv(t, pv, OBV_P_DRIVE, OBV_OFF(t, 7)); g->drive_q12 = 4096 + (d * d * 15 * 4096 / 16129);   /* 1 .. 16 */
    g->vint = pv[OBV_P_VINTAGE];
    g->sus_f = pv[OBV_P_FS] * (OBV_ENV_ONE / 127);
    g->sus_a = pv[OBV_P_AS] * (OBV_ENV_ONE / 127);
    d = pv[OBV_P_VOLUME];
    g->gain_q12 = (d * d) >> 2;
    g->pw1_base = ((uint32_t)obv_pv(t, pv, OBV_P_O1_PW, OBV_OFF(t, 5)) << 25) + (1u << 24);
    g->pw2_base = ((uint32_t)obv_pv(t, pv, OBV_P_O2_PW, OBV_OFF(t, 5)) << 25) + (1u << 24);
    g->w1 = pv[OBV_P_O1_WAVE];
    g->w2 = pv[OBV_P_O2_WAVE];
    g->ftype = pv[OBV_P_FTYPE];
    g->morph_q = pv[OBV_P_FMORPH];
    g->sync_on = pv[OBV_P_SYNC];
    d = obv_pv(t, pv, OBV_P_XMOD, OBV_OFF(t, 6)); g->xmod_q = (uint32_t)((d * d) >> 7);
}

static inline uint32_t obv_pw_mod(uint32_t base, int32_t mod)
{
    int64_t p = (int64_t)base + ((int64_t)mod << 8);
    if (p < (1 << 26)) p = 1 << 26;
    if (p > (int64_t)0xFFFFFFFFu - (1 << 26)) p = (int64_t)0xFFFFFFFFu - (1 << 26);
    return (uint32_t)p;
}

/* vintage: each voice card has its own small offsets, scaled by the knob */
static const int16_t OBV_VINT_TUNE8[OBV_NCARD] = {-20, 12, 7, -15, 18, -9, 3, -17};     /* +-8 cents */
static const int16_t OBV_VINT_CUT8[OBV_NCARD] = {-300, 250, 120, -380, 60, 330, -180, 220};   /* +-1.5 semitones */
static const int8_t OBV_VINT_ENV[OBV_NCARD] = {10, -6, 14, -12, 3, -16, 8, -4};         /* /128: +-12 % on the times */

/* one control block (16 samples) for a voice: pitch, cutoff, envelopes, amp. pitch8: Q8 semitones, this firmware's
 * pitch for the voice (glide, tuning, bend, unison detune); step: which of the block's OBV_STEPS */
static void obv_voice_block(track_t *t, const obv_part_t *g, obv_voice_t *v, uint32_t card, uint32_t note, uint32_t vel,
                            int32_t gate, int32_t pitch8, uint32_t step, const int32_t *rt_lfo)
{
    const uint8_t *pv = obv_patch_of(t)->v;
    int32_t lfo, p8, p1, p2, cut, fenv, tmp, velg, trem, vint = g->vint, vt = (vint * OBV_VINT_ENV[card % OBV_NCARD]) >> 7, lfo_to, a;
    uint32_t ph;
    (void)rt_lfo;
    /* envelopes (times: the track's offset on RELEASE) */
    obv_env_tick(&v->fe, pv[OBV_P_FA], pv[OBV_P_FD], g->sus_f, pv[OBV_P_FR], gate, vt);
    obv_env_tick(&v->ae, pv[OBV_P_AA], pv[OBV_P_AD], g->sus_a, (uint32_t)obv_pv(t, pv, OBV_P_AR, OBV_OFF(t, 3)), gate, vt);
    fenv = v->fe.level >> 9;                    /* Q15 */
    /* the LFO as this voice sees it */
    ph = g->lfo_phs[step];
    if (pv[OBV_P_LFO_PHASE] == OBV_LP_SPREAD) ph += card << 29;
    lfo = pv[OBV_P_LFO_PHASE] == OBV_LP_SPREAD ? obv_lfo_shape(g->lfo_ra[step], g->lfo_rb[step], ph, pv[OBV_P_LFO_SHAPE]) : g->lfo_val[step];
    if (v->lfo_fade < 32767) {
        uint32_t dl = OBV_LFO_DELAY_BLK[pv[OBV_P_LFO_DELAY]];
        if (v->lfo_blk >= dl) { v->lfo_fade += 32767 / (int32_t)(dl / 2u + 1u) + 1; if (v->lfo_fade > 32767) v->lfo_fade = 32767; }
        v->lfo_blk++;
    }
    lfo = (lfo * v->lfo_fade) >> 15;
    /* vintage: a slow random walk on the pitch */
    v->drift += (int32_t)(obv_xorshift(&v->noise) & 31u) - 16;
    v->drift -= v->drift >> 11;
    /* pitch, Q8 semitones */
    p8 = pitch8 + ((OBV_VINT_TUNE8[card % OBV_NCARD] * vint) >> 7) + ((v->drift * vint) >> 10);
    p8 += (lfo * g->wheel_q * 4) >> 15;         /* the wheel's vibrato */
    lfo_to = (lfo * g->lfo_pd * 24) >> 15;
    p1 = p8 + (pv[OBV_P_LFO_PITCH_TO] != OBV_TO_O2 ? lfo_to : 0);
    p2 = p8 + (pv[OBV_P_LFO_PITCH_TO] != OBV_TO_O1 ? lfo_to : 0) + ((int32_t)pv[OBV_P_O2_SEMI] - 24 + OBV_OFF(t, 4)) * 256 + g->det_fine8
       + ((fenv * g->env_o2 * 97) >> 15);
    v->inc1 = obv_pitch_inc(p1);
    v->inc2 = obv_pitch_inc(p2);
    tmp = (lfo * g->lfo_pwd);
    v->pw1 = obv_pw_mod(g->pw1_base, pv[OBV_P_LFO_PWM_TO] != OBV_TO_O2 ? tmp : 0);
    v->pw2 = obv_pw_mod(g->pw2_base, pv[OBV_P_LFO_PWM_TO] != OBV_TO_O1 ? tmp : 0);
    /* cutoff, Q8 semitones */
    cut = g->cut[step] + 8 * 256 + ((fenv * g->fenv_amt * 390) >> 15)
        + ((int32_t)note - 60) * g->keytrk * 2 + (((int32_t)vel - 64) * g->velflt >> 1)
        + ((lfo * g->lfo_fd * 97) >> 15) + ((lfo * g->wheel_q * 48) >> 15) + ((OBV_VINT_CUT8[card % OBV_NCARD] * vint) >> 7)
        + ((v->drift * vint) >> 7);
    if (g->ftype == OBV_F_LP24) obv_lad_coef(&v->lc, cut, g->lad_k[step]);
    else obv_svf_coef(&v->c1, cut, g->reso_k[step]);
    /* amp */
    velg = 32767 - ((((127 - (int32_t)vel) * (int32_t)pv[OBV_P_VEL_AMP]) >> 7) * 258);
    trem = 32767 - (((32767 - lfo) * g->lfo_ad) >> 8);
    v->amp0 = v->amp1;
    a = (int32_t)(((int64_t)(v->ae.level >> 9) * velg >> 15) * trem >> 15);
    v->amp1 = (a * g->gain_q12) >> 12;          /* the sound's VOLUME */
}

/* the samples of one voice's control block (OBV_CTL = 16): oscillators -> mix -> input stage (drive) -> filter -> amp.
 * Mono, added to out at this firmware's scale (Q16: the OberVave's Q15 doubled; its pan law's centre is its voice) */
static void obv_voice_render(const obv_part_t *g, obv_voice_t *v, int32_t *out)
{
    uint32_t i, inc1 = v->inc1, inc2 = v->inc2, pw1 = v->pw1, pw2 = v->pw2, d1 = inc1 >> 15, d2 = inc2 >> 15;
    int32_t amp0 = v->amp0, da = v->amp1 - v->amp0, o2n = v->o2.prev;
    int32_t l1 = g->lvl1, l2 = g->lvl2, ln = g->lvln, drv = g->drive_q12, morph_q = g->morph_q;
    uint32_t ft = g->ftype, xq = g->xmod_q, sy = g->sync_on, ww1 = g->w1, ww2 = g->w2;
    int32_t hw1 = (ww1 & OBV_OSC_SAW ? -65536 : 0) + (ww1 & OBV_OSC_PLS ? -65536 : 0);
    int32_t hw2 = (ww2 & OBV_OSC_SAW ? -65536 : 0) + (ww2 & OBV_OSC_PLS ? -65536 : 0);
    for (i = 0; i < OBV_CTL; i++) {
        int32_t s1, s2, mix, lp, bp, hp, y, amp, rt = -1;
        uint32_t ie = inc1;
        if (xq) {                               /* OSC2 -> OSC1 linear FM, the step never below inc/16 */
            int32_t m = (int32_t)(inc1 >> 8 > 0x200000u ? 0x200000u : inc1 >> 8) * (((o2n >> 8) * (int32_t)xq) >> 6);
            m += (int32_t)inc1;
            ie = m < (int32_t)(inc1 >> 4) ? inc1 >> 4 : (uint32_t)m;
            d1 = ie >> 15;
        }
        s1 = obv_osc_step(&v->o1, ie, d1, pw1, ww1, hw1, -1);
        if (sy && v->o1.ph < ie && d1)
            rt = (int32_t)(v->o1.ph / d1);
        s2 = obv_osc_step(&v->o2, inc2, d2, pw2, ww2, hw2, rt);
        o2n = v->o2.prev;
        mix = s1 * l1 + s2 * l2;
        if (ln) mix += ((int32_t)(obv_xorshift(&v->noise) >> 16) - 32768) * ln;
        mix >>= 7;
        mix = obv_sat_in(((mix >> 3) * drv) >> 9);   /* the input stage: DRIVE's gain, then a tanh (never a hard clip) */
        if (ft == OBV_F_LP24)
            y = obv_lad_step(&v->lc, &v->lad, mix);
        else {
            obv_svf_step(&v->c1, &v->f1, mix, &lp, &bp, &hp);
            switch (ft) {
            case OBV_F_HP12: y = hp; break;
            case OBV_F_BP12: y = bp; break;
            case OBV_F_NOTCH: y = hp + lp; break;
            case OBV_F_SEM: y = lp + (((hp - lp) * morph_q) >> 7); break;
            default: y = lp; break;
            }
        }
        y = obv_sat_knee(y);
        amp = amp0 + ((da * (int32_t)i) >> 4);
        y = (y * amp) >> 16;
        out[i] += y * 2;
    }
}

/* ---- the engine */
static int obv_done(track_t *t, voice_t *v)
{
    obv_voice_t *s = obv_voice(t, v);
    return !s || s->ae.stage == OBV_E_IDLE;
}
static void obv_note_on(track_t *t, voice_t *v)
{
    obv_voice_t *s = obv_voice(t, v);
    const uint8_t *pv = obv_patch_of(t)->v;
    if (!s) return;
    s->fe.stage = OBV_E_ATT;                    /* the levels go on: an analog retrigger */
    s->ae.stage = OBV_E_ATT;
    s->lfo_blk = 0;
    s->lfo_fade = pv[OBV_P_LFO_DELAY] ? 0 : 32767;
}
static void obv_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    obv_voice_t *s = obv_voice(t, v);
    obv_part_t *g = obv_part((uint32_t)(t - trk));
    uint32_t step, card = (uint32_t)(v - t->v) % OBV_NV, done = 0;
    int32_t pitch8;
    if (!s || !g) return;
    /* this firmware's pitch (1/16 semitone, with glide, tuning, bend, LFO and the unison detune), Q8 semitones, and
     * its residual below 1/16 (1/4096 of the increment: 1.08 per unit in Q8 semitones) */
    pitch8 = m->pitch16 * 16 + ((m->fine * 1109) >> 10);
    for (step = 0; done + OBV_CTL <= n && step < OBV_STEPS; step++, done += OBV_CTL) {
        obv_voice_block(t, g, s, card, v->note, v->mvel, v->gate, pitch8, step, 0);
        obv_voice_render(g, s, out + done);
    }
}

/* the sound a preset names: 0 INIT, 1.. the factory ones (OberVave's 1..64) */
static void obv_factory_patch(uint32_t k, obv_patch_t *p)
{
    if (k >= 1u && k <= OBV_NFACT) obv_patch_from_factory(p, &OBV_FACTORY[k - 1u]);
    else obv_patch_from_factory(p, &OBV_FACT_INIT);
}
static void obv_preset_make(uint32_t k, preset_t *pr, char *name)
{
    obv_patch_t p;
    uint32_t i;
    pr->env[2] = 127;
    obv_factory_patch(k, &p);
    for (i = 0; i < OBV_PATCH_NAME && p.v[OBV_NPARAM + i] != ' ' && p.v[OBV_NPARAM + i]; i++)
        name[i] = (char)p.v[OBV_NPARAM + i];
    name[i] = 0;
    if (!i) str_cpy(name, k ? "OBERVAVE" : "INIT OBERVAVE", 21);
}
/* a preset was loaded into the track (ui.c): its patch is the sound */
static void obv_preset_loaded(track_t *t, uint32_t pi)
{
    obv_factory_patch(pi, obv_patch_of(t));
}

static const engine_t ENG_OBV = {
    .name = "OBERVAVE", .page_title = {"FILTER", "OSC/ENV"},
    .edit = {{"CUT", F_INT, -63, 63, 0, 0, 0}, {"RES", F_INT, -63, 63, 0, 0, 0},
             {"ENVAMT", F_INT, -63, 63, 0, 0, 0}, {"REL", F_INT, -63, 63, 0, 0, 0},
             {"SEMI2", F_SEMI, -24, 24, 0, 0, 0}, {"PW", F_INT, -63, 63, 0, 0, 0},
             {"XMOD", F_INT, -63, 63, 0, 0, 0}, {"DRIVE", F_INT, -63, 63, 0, 0, 0}},
    .preset_make = obv_preset_make, .npresets = OBV_NFACT + 1u, .poly = OBV_NV, .ownenv = 1,
    .note_on = obv_note_on, .render = obv_render, .done = obv_done, .block = obv_block,
    .knob = {P_E0, P_E1, P_E2, P_E3}
};
