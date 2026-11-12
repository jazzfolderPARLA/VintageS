/* SPDX-License-Identifier: GPL-3.0-only */
/* OBERVAVE's parameter table, trimmed to what this firmware's engine (eng_obv.c) reads. A patch is a byte per
 * parameter plus a 10-character name. Dropped from OberVave's own table: the arpeggiator, chorus, delay and
 * reverb (this firmware has no arpeggiator and its own effects), the voice mode / unison / glide / bend / spread
 * (this firmware allocates the voices), and the UI data (names, pages, MIDI CCs). Patches are never stored by
 * index, so the numbering is free: factory sounds name parameters by their OBV_P_ symbol. */
enum {
    OBV_P_O1_WAVE, OBV_P_O1_PW, OBV_P_O1_LVL, OBV_P_NOISE, OBV_P_O2_WAVE, OBV_P_O2_PW, OBV_P_O2_LVL, OBV_P_O2_SEMI,
    OBV_P_SYNC, OBV_P_XMOD, OBV_P_ENV_O2, OBV_P_O2_FINE, OBV_P_CUTOFF, OBV_P_RESO, OBV_P_FENV_AMT, OBV_P_KEYTRK,
    OBV_P_FTYPE, OBV_P_DRIVE, OBV_P_VEL_FLT, OBV_P_VEL_AMP, OBV_P_FA, OBV_P_FD, OBV_P_FS, OBV_P_FR, OBV_P_AA,
    OBV_P_AD, OBV_P_AS, OBV_P_AR, OBV_P_LFO_SHAPE, OBV_P_LFO_RATE, OBV_P_LFO_DELAY, OBV_P_LFO_PHASE, OBV_P_LFO_PITCH,
    OBV_P_LFO_PWM, OBV_P_LFO_FLT, OBV_P_LFO_AMP, OBV_P_LFO_PITCH_TO, OBV_P_LFO_PWM_TO, OBV_P_VINTAGE, OBV_P_VOLUME,
    OBV_P_MOD_DEPTH, OBV_P_FMORPH,
    OBV_NPARAM
};
#define OBV_PATCH_NAME 10u
#define OBV_PATCH_BYTES (OBV_NPARAM + OBV_PATCH_NAME)   /* the parameters, then the name */
#define OBV_NFACT 64u

enum { OBV_W_OFF, OBV_W_SAW, OBV_W_PLS, OBV_W_BOTH };
enum { OBV_F_LP12, OBV_F_HP12, OBV_F_BP12, OBV_F_NOTCH, OBV_F_LP24, OBV_F_SEM, OBV_NFTYPE };   /* SEM: LP .. notch .. HP by MORPH */
enum { OBV_L_TRI, OBV_L_SAW, OBV_L_RAMP, OBV_L_SQR, OBV_L_SH, OBV_L_RND };
enum { OBV_LP_SYNC, OBV_LP_FREE, OBV_LP_SPREAD };
enum { OBV_TO_O1, OBV_TO_O2, OBV_TO_BOTH };

typedef struct { uint8_t max, def; } obv_pdef_t;
static const obv_pdef_t OBV_PDEF[OBV_NPARAM] = {
    [OBV_P_O1_WAVE] = {3, OBV_W_SAW},
    [OBV_P_O1_PW] = {127, 64},
    [OBV_P_O1_LVL] = {127, 110},
    [OBV_P_NOISE] = {127, 0},
    [OBV_P_O2_WAVE] = {3, OBV_W_SAW},
    [OBV_P_O2_PW] = {127, 64},
    [OBV_P_O2_LVL] = {127, 100},
    [OBV_P_O2_SEMI] = {48, 24},
    [OBV_P_SYNC] = {1, 0},
    [OBV_P_XMOD] = {127, 0},
    [OBV_P_ENV_O2] = {127, 64},
    [OBV_P_O2_FINE] = {127, 70},
    [OBV_P_CUTOFF] = {127, 80},
    [OBV_P_RESO] = {127, 20},
    [OBV_P_FENV_AMT] = {127, 90},
    [OBV_P_KEYTRK] = {127, 64},
    [OBV_P_FTYPE] = {OBV_NFTYPE - 1, OBV_F_LP24},
    [OBV_P_DRIVE] = {127, 0},
    [OBV_P_VEL_FLT] = {127, 40},
    [OBV_P_VEL_AMP] = {127, 60},
    [OBV_P_FA] = {127, 20},
    [OBV_P_FD] = {127, 70},
    [OBV_P_FS] = {127, 40},
    [OBV_P_FR] = {127, 60},
    [OBV_P_AA] = {127, 10},
    [OBV_P_AD] = {127, 70},
    [OBV_P_AS] = {127, 100},
    [OBV_P_AR] = {127, 60},
    [OBV_P_LFO_SHAPE] = {5, OBV_L_TRI},
    [OBV_P_LFO_RATE] = {127, 70},
    [OBV_P_LFO_DELAY] = {127, 0},
    [OBV_P_LFO_PHASE] = {2, OBV_LP_FREE},
    [OBV_P_LFO_PITCH] = {127, 0},
    [OBV_P_LFO_PWM] = {127, 0},
    [OBV_P_LFO_FLT] = {127, 0},
    [OBV_P_LFO_AMP] = {127, 0},
    [OBV_P_LFO_PITCH_TO] = {2, OBV_TO_BOTH},
    [OBV_P_LFO_PWM_TO] = {2, OBV_TO_BOTH},
    [OBV_P_VINTAGE] = {127, 30},
    [OBV_P_VOLUME] = {127, 100},
    [OBV_P_MOD_DEPTH] = {127, 0},
    [OBV_P_FMORPH] = {127, 0},
};

typedef struct { uint8_t v[OBV_PATCH_BYTES]; } obv_patch_t;   /* v[0..OBV_NPARAM) the parameters, v[OBV_NPARAM..] the name */

static void obv_patch_defaults(obv_patch_t *p)
{
    uint32_t i;
    for (i = 0; i < OBV_NPARAM; i++) p->v[i] = OBV_PDEF[i].def;
    for (; i < OBV_PATCH_BYTES; i++) p->v[i] = ' ';
}

/* a factory patch: a name and a list of (parameter, value) pairs over the defaults, ended by 255 */
typedef struct { const char *name; const uint8_t *set; } obv_factory_t;
static void obv_patch_from_factory(obv_patch_t *p, const obv_factory_t *f)
{
    const uint8_t *s = f->set;
    uint32_t i;
    obv_patch_defaults(p);
    while (s[0] != 255u) { p->v[s[0]] = s[1]; s += 2; }
    for (i = 0; i < OBV_PATCH_NAME && f->name[i]; i++) p->v[OBV_NPARAM + i] = (uint8_t)f->name[i];
}
