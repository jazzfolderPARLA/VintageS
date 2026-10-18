/* SPDX-License-Identifier: GPL-3.0-only */
/* JUNO-106: the FM-106 firmware's Roland Juno-106 voice as a Melodee engine, bank C of ATMOS.
 * The voice is the FM-106's own, untouched: the DCO (PolyBLEP saw / pulse / sub / noise), the D7811G cutoff
 * DAC and IR3109 4-pole ladder, the uPD7811 envelope, the BA662 VCA curve (fm106_osc.h, fm106_vcf.h, fm106_adsr.h,
 * fm106_tables.h), and the 128 factory patches (fm106_presets.h). Melodee gives it the voices (its allocator and
 * steals, glide, bend, tune: m->inc, m->pitch16) and the part (its level, pan, FX bus); the engine keeps the Juno's
 * own envelope (ownenv), its LFO (rate, delay, DCO / PWM / VCF depth), and its HPF and master level after the voices.
 * The chorus is not here: the FX bus has its own.
 *
 * The patch is the factory preset t->preset (fm106_presets.h p[]); the eight EDIT values and the ADSR of the track
 * override what the knobs and sliders shape: E0 CUTOFF, E1 RESO, E2 ENV, E3 SUB, E4 LFO RATE, E5 LFO > DCO, E6 PWM,
 * E7 NOISE; ATK DEC SUS REL are the Juno's. A preset load sets them from the patch (jn_preset_make). */
#include "fm106_presets.h"
#include "fm106_osc.h"
#include "fm106_vcf.h"
#include "fm106_adsr.h"

#define JN_POLY 6u                                       /* the Juno-106's six voices */
typedef struct {
    fm106_dco_t dco;
    fm106_vcf_t vcf;
    fm106_adsr_t adsr;
    int64_t dc_osc, dc_vcf;                              /* the two coupling capacitors' DC blockers */
    int32_t vca_slew, vcf_slew;                          /* the VCA's and the cutoff CV's RC slews */
    uint8_t ready, gate;
} jn_voice_t;
typedef struct {
    jn_voice_t v[NPOLY];
    fm106_hpf_t hpf;
    uint32_t lfo_ph, lfo_timer;                          /* the LFO's phase; samples since the first key (its delay) */
} jn_part_t;
static jn_part_t *jn_part(uint32_t part);                /* engines.c: the shared runtime pool */
static jn_voice_t *jn_voice(track_t *t, voice_t *v)
{
    return &jn_part((uint32_t)(t - trk) % NPART)->v[(uint32_t)(v - t->v) % NPOLY];
}

/* what the voice and the part use of a patch: the factory's, with the track's EDIT and ADSR values over it */
typedef struct {
    uint8_t lfo_rate, lfo_delay, lfo_dco, pwm_depth, sub_level, noise, hpf;
    uint8_t cutoff, res, env_mod, lfo_vcf, kbd, vca_level;
    uint8_t a, d, s, r;
    uint8_t pulse, saw, sub, range, pwm_mode, env_pol, vca_mode;
} jn_pa_t;
static uint8_t jn_u7(int32_t v) { return (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v); }
static void jn_resolve(const track_t *t, jn_pa_t *a)
{
    const uint8_t *p = FM106_PRESETS[t->preset % FM106_NUM_FACTORY_PRESETS].p;
    a->lfo_rate = jn_u7(t->p[P_E4]);   a->lfo_delay = p[4];     a->lfo_dco = jn_u7(t->p[P_E5]);
    a->pwm_depth = jn_u7(t->p[P_E6]);  a->sub_level = jn_u7(t->p[P_E3]);  a->noise = jn_u7(t->p[P_E7]);
    a->hpf = p[9] > 3u ? 1u : p[9];
    a->cutoff = jn_u7(t->p[P_E0]);     a->res = jn_u7(t->p[P_E1]);        a->env_mod = jn_u7(t->p[P_E2]);
    a->lfo_vcf = p[13];                a->kbd = p[14];                    a->vca_level = p[15];
    a->a = jn_u7(t->p[P_ATK]);         a->d = jn_u7(t->p[P_DEC]);         a->s = jn_u7(t->p[P_SUS]);   a->r = jn_u7(t->p[P_REL]);
    a->pulse = p[23];                  a->saw = p[24];                    a->sub = p[25];
    a->range = p[29] > 2u ? 1u : p[29];
    a->pwm_mode = p[33];               a->env_pol = p[34];                a->vca_mode = p[35];
}
/* any voice of the part with its key down (the LFO's delay counts only then) */
static uint32_t jn_keys(const jn_part_t *p) { for (uint32_t i = 0; i < NPOLY; i++) if (p->v[i].gate) return 1u; return 0u; }

static void jn_note_on(track_t *t, voice_t *v)
{
    jn_part_t *p = jn_part((uint32_t)(t - trk) % NPART);
    jn_voice_t *c = jn_voice(t, v);
    jn_pa_t a;
    if (!c->ready || voice_was == 0u) {                  /* a free voice: all of it fresh (a steal keeps phase and filter) */
        uint32_t keys = jn_keys(p);
        fm106_dco_init(&c->dco, (uint32_t)(c - p->v));
        fm106_vcf_init(&c->vcf);
        fm106_adsr_init(&c->adsr);
        c->dc_osc = c->dc_vcf = 0;
        c->vca_slew = c->vcf_slew = 0;
        c->ready = 1;
        if (!keys)
            p->lfo_timer = 0;                            /* the LFO's delay starts with the first key */
    }
    jn_resolve(t, &a);
    fm106_adsr_params(&c->adsr, a.a, a.d, a.s, a.r);
    c->gate = 1;
    fm106_adsr_gate_on(&c->adsr);
    /* a patch of pure resonance (all waves off, high RESO): seed the ladder so it rings, as the FM-106 does */
    if (!a.saw && !a.pulse && !a.sub && !a.noise && a.res > 80u && c->vcf.z[0] == 0 && c->vcf.z[1] == 0)
        c->vcf.z[0] = 50 << 9;
}

static int jn_done(track_t *t, voice_t *v)
{
    jn_voice_t *c = jn_voice(t, v);
    return c->adsr.stage == FM106_ENV_IDLE && !c->gate && c->vca_slew <= 10;
}

/* the FM-106's fm106_render_voice, one voice for n samples; the LFO is the part's, computed here (every voice gets the
 * same samples: the part's phase and delay timer move once per block, in jn_post) */
static __attribute__((noinline)) void jn_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    jn_part_t *p = jn_part((uint32_t)(t - trk) % NPART);
    jn_voice_t *c = jn_voice(t, v);
    jn_pa_t a;
    uint32_t i, lfo_ph = p->lfo_ph, step, delay_samples, timer = p->lfo_timer, keys = jn_keys(p), base;
    uint16_t vcf_cutoff;
    uint8_t lfo_to_vcf, vcf_env_mod, vcf_key, env_pol;
    if (!n)
        return;
    jn_resolve(t, &a);
    if (!v->gate && c->gate) {                           /* released */
        c->gate = 0;
        fm106_adsr_gate_off(&c->adsr);
    }
    fm106_adsr_params(&c->adsr, a.a, a.d, a.s, a.r);
    c->dco.saw_on = a.saw;  c->dco.pulse_on = a.pulse;  c->dco.sub_on = a.sub;
    c->dco.sub_level = a.sub_level;  c->dco.noise_level = a.noise;
    base = m->inc;                                       /* Melodee's pitch (glide, bend, tune) for the 8' range */
    if (a.range == 0u) base >>= 1;                       /* 16' */
    else if (a.range == 2u) base <<= 1;                  /* 4' */
    step = (uint32_t)a.lfo_rate * 320u;
    delay_samples = (uint32_t)a.lfo_delay * 520u;
    vcf_cutoff = (uint16_t)(((uint32_t)a.cutoff * 0x3F80u) / 127u);
    lfo_to_vcf = (uint8_t)(((uint32_t)a.lfo_vcf * 254u) / 127u);
    vcf_env_mod = (uint8_t)(((uint32_t)a.env_mod * 254u) / 127u);
    vcf_key = (uint8_t)(((uint32_t)a.kbd * 254u) / 127u);
    env_pol = a.env_pol == 0u ? 1u : 0u;
    for (i = 0; i < n; i++) {
        int32_t lfo_s, lfo_tri, env_q15, vca_target, dco_s, vcf_s, out_s, g;
        uint32_t eff_inc = base, vca_idx;
        uint16_t lfo_sig = 0, dac, dac_sl, env14;
        uint8_t lfo_pol = 0;
        lfo_ph += step;                                  /* the LFO sample (fm106_audio_render) */
        lfo_tri = (int32_t)(lfo_ph >> 16) - 32768;
        if (lfo_tri < 0)
            lfo_tri = -lfo_tri;
        lfo_s = (int16_t)((lfo_tri - 16384) << 1);
        if (a.lfo_delay > 0u) {
            int32_t amp = 0;
            if (keys) {
                uint32_t tt = timer + i;
                amp = tt < delay_samples ? 0 : tt < delay_samples + 11025u ? (int32_t)(((tt - delay_samples) << 15) / 11025u) : 32767;
            }
            lfo_s = (int16_t)((lfo_s * amp) >> 15);
        }
        if (a.lfo_dco > 0u) {                            /* per-sample DCO vibrato */
            int32_t pm = (lfo_s * (int32_t)a.lfo_dco) >> 15;
            eff_inc = (uint32_t)((int32_t)eff_inc + ((pm * (int32_t)(eff_inc >> 8)) >> 12));
        }
        if (lfo_to_vcf > 0u) {
            uint16_t lv = (uint16_t)(((lfo_s < 0 ? -lfo_s : lfo_s) * 0x1FFFu) >> 15);
            lfo_sig = (uint16_t)(((uint32_t)lfo_to_vcf * lv) >> 9);
            lfo_pol = lfo_s < 0 ? 1u : 0u;
        }
        env_q15 = fm106_adsr_process(&c->adsr);
        vca_idx = (uint32_t)env_q15 >> 7;
        if (vca_idx > 255u)
            vca_idx = 255u;
        vca_target = a.vca_mode == 1u ? (c->gate ? 32767 : 0) : (int32_t)FM106_VCA_HW[vca_idx];
        c->vca_slew += ((vca_target - c->vca_slew) * 1064) >> 15;
        dco_s = fm106_dco_process(&c->dco, eff_inc, a.pwm_depth, a.pwm_mode, lfo_s, env_q15);
        dco_s = fm106_dc_block(dco_s, &c->dc_osc);
        env14 = (uint16_t)(env_q15 >> 1);
        dac = fm106_calc_vcf_freq(vcf_cutoff, lfo_sig, 0, vcf_env_mod, vcf_key, lfo_pol, 0, env_pol, env14,
                                  (uint16_t)((uint32_t)m->pitch16 << 4));
        if (c->vcf_slew == 0)
            c->vcf_slew = (int32_t)dac;
        else
            c->vcf_slew += (int32_t)(((int32_t)dac - c->vcf_slew) * 1393) >> 15;
        dac_sl = (uint16_t)c->vcf_slew;
        vcf_s = fm106_vcf_process(&c->vcf, dco_s, dac_sl, a.res);
        vcf_s = fm106_dc_block(vcf_s, &c->dc_vcf);
        out_s = (int32_t)(((int64_t)vcf_s * c->vca_slew) >> 15);
        g = m->amp0 + (int32_t)((int64_t)(m->amp1 - m->amp0) * (int64_t)i / (int64_t)n);   /* the part's ramp (fades) */
        out[i] += (int32_t)(((int64_t)out_s * g) >> 15) * 5 >> 4;   /* (the FM-106's voice sum is >> 4; x5 brings a voice to
                                                                      * the level of FM6 / PROPHET: measured, tests/juno_test.c) */
    }
}

/* the part's block after its voices: the HPF, the master level (the patch's VCA slider), then the LFO moves on */
static void jn_post(track_t *t, int32_t *out, uint32_t n, uint32_t nr)
{
    jn_part_t *p = jn_part((uint32_t)(t - trk) % NPART);
    jn_pa_t a;
    uint32_t i;
    int32_t law;
    if (!nr)
        return;
    jn_resolve(t, &a);
    p->hpf.mode = a.hpf;
    law = (int32_t)FM106_VCA_LAW[a.vca_level > 127u ? 127u : a.vca_level];
    for (i = 0; i < n; i++)
        out[i] = (int32_t)(((int64_t)fm106_hpf_process(&p->hpf, out[i]) * law) >> 12);
    p->lfo_ph += n * (uint32_t)a.lfo_rate * 320u;
    if (jn_keys(p) && p->lfo_timer < 0xFFFFFFFFu - n)
        p->lfo_timer += n;
}

/* factory patch k as a preset: the sound's name, and the values the EDIT pages, the knobs and the ADSR start from */
static void jn_preset_make(uint32_t k, preset_t *pr, char *name)
{
    const fm106_preset_t *f = &FM106_PRESETS[k % FM106_NUM_FACTORY_PRESETS];
    uint32_t i;
    for (i = 0; f->name[i] && i < 20u; i++)
        name[i] = f->name[i];
    name[i] = 0;
    pr->e[0] = (int8_t)f->p[10];                         /* CUTOFF */
    pr->e[1] = (int8_t)f->p[11];                         /* RESO */
    pr->e[2] = (int8_t)f->p[12];                         /* ENV */
    pr->e[3] = (int8_t)f->p[7];                          /* SUB */
    pr->e[4] = (int8_t)f->p[3];                          /* LFO RATE */
    pr->e[5] = (int8_t)f->p[5];                          /* LFO > DCO */
    pr->e[6] = (int8_t)f->p[6];                          /* PWM */
    pr->e[7] = (int8_t)f->p[8];                          /* NOISE */
    pr->env[0] = f->p[16]; pr->env[1] = f->p[17]; pr->env[2] = f->p[18]; pr->env[3] = f->p[19];
    pr->mono = 0;
}

static const engine_t ENG_JUNO = {
    .name = "JUNO-106", .page_title = {"JUNO", "JUNO 2"},
    .edit = {
        {"CUTOFF", F_INT, 0, 127, 64, 0, 0}, {"RESO", F_INT, 0, 127, 0, 0, 0},
        {"ENV", F_INT, 0, 127, 0, 0, 0},     {"SUB", F_INT, 0, 127, 0, 0, 0},
        {"LFO RT", F_INT, 0, 127, 0, 0, 0},  {"LFO>DCO", F_INT, 0, 127, 0, 0, 0},
        {"PWM", F_INT, 0, 127, 0, 0, 0},     {"NOISE", F_INT, 0, 127, 0, 0, 0},
    },
    .preset_make = jn_preset_make, .npresets = FM106_NUM_FACTORY_PRESETS,
    .poly = JN_POLY, .ownenv = 1, .done = jn_done,
    .note_on = jn_note_on, .render = jn_render, .post = jn_post,
    .knob = {P_E0, P_E1, P_E2, P_E3},
};
