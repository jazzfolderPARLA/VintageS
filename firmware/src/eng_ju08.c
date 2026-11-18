/* SPDX-License-Identifier: GPL-3.0-only */
/* JUPITER-8 MINI: the voice of the Jupiter-8 port for the FM-1 (a fixed-point port of jupiter8_esp32's engine,
 * GPL-3.0-only). Its DSP (ju8_dsp.h, ju8_tables.h) and its parameters and factory sounds (ju8_params.h, ju8_presets.h)
 * are the port's own, renamed j8_ / J8_; this file is the glue: a voice of this firmware is a voice of the Jupiter-8.
 * This firmware allocates the voices (POLY / MONO / LEGATO / UNISON, GLIDE) and hands over the pitch (glide, tuning,
 * bend, LFO, unison detune); the Jupiter's two envelopes are the voice (ownenv: the second one is the VCA), the sound is
 * mono (this firmware spreads and pans). The port's key assigner, layers, portamento, bender, arpeggiator, sequencer
 * and effects are not part of it. The port's control block is 32 samples: this firmware's CTL, one render call. */
#include "ju8_params.h"
#include "ju8_dsp.h"
#include "ju8_presets.h"

#if CTL != J8_BLOCK
#error "CTL must be the Jupiter-8's 32-sample control block"
#endif

static j8_part_t *j8_part(uint32_t part);

/* the sound of each track: the Jupiter-8's parameters (a byte each); RAM, with the pool */
static uint8_t j8_patch[NPART][J8_NP] __attribute__((section(".pool")));
static uint8_t j8_patch_ready[NPART];
static uint8_t *j8_patch_of(const track_t *t)
{
    uint32_t k = (uint32_t)(t - trk) % NPART;
    if (!j8_patch_ready[k]) { j8_patch_init(j8_patch[k]); j8_patch_ready[k] = 1; }
    return j8_patch[k];
}
static inline j8_voice_t *j8_voice(track_t *t, voice_t *v)
{
    j8_part_t *part = j8_part((uint32_t)(t - trk));
    if (!part) return 0;
    if (!part->ready) j8_part_init(part);
    return &part->voice[(uint32_t)(v - t->v) % J8_NV];
}

/* The track's live offsets on the sound (the EDIT knobs): CUT, RES, HPF and ENV AMT of the filter, the detune of
 * oscillator 2, the oscillator MIX, X-MOD and the amplitude RELEASE. Added to the patch's bytes (x4: the knob's
 * -63..63 reaches the whole 0..255), clamped. */
#define J8_OFF(t, k) ((int32_t)(t)->p[P_E0 + (k)] * 4)
static inline void j8_edit(uint8_t *pe, const uint8_t *pv, uint32_t id, int32_t off)
{
    int32_t x = (int32_t)pv[id] + off;
    pe[id] = (uint8_t)(x < 0 ? 0 : x > 255 ? 255 : x);
}

/* once per block of this firmware and sound (engine_t.block): the sound with the EDIT offsets, the LFO and the slewed
 * controls */
static void j8_block(track_t *t)
{
    j8_part_t *g = j8_part((uint32_t)(t - trk));
    const uint8_t *pv = j8_patch_of(t);
    uint32_t i;
    if (!g) return;
    if (!g->ready) j8_part_init(g);
    for (i = 0; i < J8_NP; i++) g->pe[i] = pv[i];
    j8_edit(g->pe, pv, J8_P_VCF_CUT, J8_OFF(t, 0));
    j8_edit(g->pe, pv, J8_P_VCF_RES, J8_OFF(t, 1));
    j8_edit(g->pe, pv, J8_P_HPF, J8_OFF(t, 2));
    j8_edit(g->pe, pv, J8_P_VCF_ENV, J8_OFF(t, 3));
    j8_edit(g->pe, pv, J8_P_V2_TUNE, J8_OFF(t, 4));
    j8_edit(g->pe, pv, J8_P_MIX, J8_OFF(t, 5));
    j8_edit(g->pe, pv, J8_P_V1_XMOD, J8_OFF(t, 6));
    j8_edit(g->pe, pv, J8_P_E2_R, J8_OFF(t, 7));
    j8_update(g);
}

/* ---- the engine */
static int j8_done(track_t *t, voice_t *v)
{
    j8_voice_t *s = j8_voice(t, v);
    return !s || !s->active;
}
static void j8_note_on(track_t *t, voice_t *v)
{
    j8_voice_t *s = j8_voice(t, v);
    uint32_t vel = v->mvel > 127u ? 127u : v->mvel;
    if (!s) return;
    /* the Jupiter-8 has no velocity: half of the range here (soft keys at -6 dB), so a played part breathes */
    j8_trigger(s, 32768 - (int32_t)(127u - vel) * 129);
}
static void j8_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    j8_voice_t *s = j8_voice(t, v);
    j8_part_t *g = j8_part((uint32_t)(t - trk));
    int32_t pitch;
    if (!s || !g || !s->active || n < J8_BLOCK) return;
    /* this firmware's pitch (1/16 semitone, with glide, tuning, bend, LFO and the unison detune) in Q16 semitones, and
     * its residual below 1/16 (1/4096 of the increment: 277 per unit in Q16 semitones) */
    pitch = m->pitch16 * 4096 + m->fine * 277;
    j8_render_voice(g, s, v->note > 127u ? 127 : (int)v->note, pitch, v->gate, out);
}

/* the sound a preset names: 0 INIT, 1.. the factory ones (the port's 11-88, in order) */
static void j8_factory_patch(uint32_t k, uint8_t *v)
{
    uint32_t i;
    if (k >= 1u && k <= J8_NFACT)
        for (i = 0; i < J8_NP; i++) v[i] = J8_FACT[k - 1u].v[i];
    else
        j8_patch_init(v);
}
static void j8_preset_make(uint32_t k, preset_t *pr, char *name)
{
    uint8_t v[J8_NP];
    uint32_t i;
    pr->env[2] = 127;
    j8_factory_patch(k, v);
    pr->mono = v[J8_P_ASSIGN] == J8_AS_SOLO || v[J8_P_ASSIGN] == J8_AS_UNISON;   /* (the port's SOLO and UNISON play one note) */
    if (k >= 1u && k <= J8_NFACT)
        for (i = 0; i < 11u && J8_FACT[k - 1u].name[i]; i++)
            name[i] = J8_FACT[k - 1u].name[i];
    else
        i = 0;
    name[i] = 0;
    if (!i) str_cpy(name, "INIT JUPITER", 21);
}
/* a preset was loaded into the track (ui.c): its patch is the sound */
static void j8_preset_loaded(track_t *t, uint32_t pi)
{
    j8_factory_patch(pi, j8_patch_of(t));
}
/* the budget units one voice takes: the four-pole ladder with a tanh per stage and two polyBLEP oscillators per sample
 * cost more than OBERVAVE's voice. Three, as the Prophet's measured on the serial hardware; to be checked on a unit. */
static uint32_t j8_units(const track_t *t) { (void)t; return 3u; }

static const engine_t ENG_JU08 = {
    .name = "JUPITER-8", .page_title = {"FILTER", "OSC/ENV"},
    .edit = {{"CUT", F_INT, -63, 63, 0, 0, 0}, {"RES", F_INT, -63, 63, 0, 0, 0},
             {"HPF", F_INT, -63, 63, 0, 0, 0}, {"ENVAMT", F_INT, -63, 63, 0, 0, 0},
             {"DETUNE", F_INT, -63, 63, 0, 0, 0}, {"MIX", F_INT, -63, 63, 0, 0, 0},
             {"XMOD", F_INT, -63, 63, 0, 0, 0}, {"REL", F_INT, -63, 63, 0, 0, 0}},
    .preset_make = j8_preset_make, .npresets = J8_NFACT + 1u, .poly = J8_NV, .ownenv = 1,
    .note_on = j8_note_on, .render = j8_render, .done = j8_done, .block = j8_block, .units = j8_units,
    .knob = {P_E0, P_E1, P_E2, P_E3}
};
