/* SPDX-License-Identifier: GPL-3.0-only */
/* The parameter table: a patch is a byte per parameter plus a 10-character name. The UI, MIDI CC, the
 * randomizer and the storage all walk this table. */
enum {
    OBV_P_O1_WAVE, OBV_P_O1_PW, OBV_P_O1_LVL, OBV_P_NOISE,
    OBV_P_O2_WAVE, OBV_P_O2_PW, OBV_P_O2_LVL, OBV_P_O2_SEMI,
    OBV_P_SYNC, OBV_P_XMOD, OBV_P_ENV_O2, OBV_P_O2_FINE,
    OBV_P_CUTOFF, OBV_P_RESO, OBV_P_FENV_AMT, OBV_P_KEYTRK, OBV_P_FTYPE,
    OBV_P_DRIVE, OBV_P_VEL_FLT, OBV_P_VEL_AMP,
    OBV_P_FA, OBV_P_FD, OBV_P_FS, OBV_P_FR,
    OBV_P_AA, OBV_P_AD, OBV_P_AS, OBV_P_AR,
    OBV_P_LFO_SHAPE, OBV_P_LFO_RATE, OBV_P_LFO_DELAY, OBV_P_LFO_PHASE,
    OBV_P_LFO_PITCH, OBV_P_LFO_PWM, OBV_P_LFO_FLT, OBV_P_LFO_AMP, OBV_P_LFO_PITCH_TO, OBV_P_LFO_PWM_TO,
    OBV_P_MODE, OBV_P_UNI_N, OBV_P_UNI_DET, OBV_P_GLIDE,
    OBV_P_VINTAGE, OBV_P_SPREAD, OBV_P_BEND, OBV_P_VOLUME,
    OBV_P_MOD_DEPTH,
    OBV_P_ARP_MODE, OBV_P_ARP_DIV, OBV_P_ARP_OCT, OBV_P_ARP_GATE,
    OBV_P_FMORPH,                                   /* (appended: the MIDI CC numbers of the above stay) */
    OBV_P_CHO_RATE, OBV_P_CHO_DEPTH, OBV_P_CHO_MIX,
    OBV_P_DLY_TIME, OBV_P_DLY_FB, OBV_P_DLY_TONE, OBV_P_DLY_MIX,
    OBV_P_REV_SIZE, OBV_P_REV_DAMP, OBV_P_REV_PRE, OBV_P_REV_MIX,
    OBV_NPARAM
};
#define OBV_PATCH_NAME 10u
#define OBV_PATCH_BYTES 80u                         /* OBV_NPARAM values, the name at OBV_NPARAM, the rest spare */
#if OBV_NPARAM + OBV_PATCH_NAME > OBV_PATCH_BYTES
#error "the patch outgrew OBV_PATCH_BYTES"
#endif
#define OBV_NPROG 99u                               /* 1-64 factory sounds, 65-99 INIT; the user's saves over them */
#define OBV_NFACT 64u

enum { OBV_K_NUM, OBV_K_BIP, OBV_K_ONOFF, OBV_K_WAVE, OBV_K_FTYPE, OBV_K_MODE, OBV_K_LSHAPE, OBV_K_LPHASE, OBV_K_TO, OBV_K_ARP, OBV_K_DIV, OBV_K_SEMI, OBV_K_MS, OBV_K_HZ,
       OBV_K_PCT, OBV_K_CUT, OBV_K_OCT, OBV_K_MORPH, OBV_K_DDIV };
enum { OBV_W_OFF, OBV_W_SAW, OBV_W_PLS, OBV_W_BOTH };
enum { OBV_F_LP12, OBV_F_HP12, OBV_F_BP12, OBV_F_NOTCH, OBV_F_LP24, OBV_F_SEM, OBV_NFTYPE };   /* SEM: LP .. notch .. HP by MORPH */
enum { OBV_M_POLY, OBV_M_DUAL, OBV_M_UNISON, OBV_M_MONO };
enum { OBV_L_TRI, OBV_L_SAW, OBV_L_RAMP, OBV_L_SQR, OBV_L_SH, OBV_L_RND };
enum { OBV_LP_SYNC, OBV_LP_FREE, OBV_LP_SPREAD };
enum { OBV_TO_O1, OBV_TO_O2, OBV_TO_BOTH };
enum { OBV_A_OFF, OBV_A_UP, OBV_A_DOWN, OBV_A_UPDN, OBV_A_RND, OBV_A_PLAY, OBV_NARPMODE };
enum { OBV_D_4, OBV_D_8, OBV_D_8T, OBV_D_16, OBV_D_16T, OBV_D_32, OBV_D_32T, OBV_NDIV };
enum { OBV_DD_16, OBV_DD_8T, OBV_DD_8, OBV_DD_8D, OBV_DD_4, OBV_DD_4D, OBV_DD_2, OBV_NDDIV };   /* the delay's note values */

typedef struct {
    const char *name;                           /* 8 characters at most (the knob label) */
    uint8_t max, def, kind, cc;                 /* cc: a fixed MIDI CC, 0 = taken from 20 up in table order */
} obv_pdef_t;
static const obv_pdef_t OBV_PDEF[OBV_NPARAM] = {
    [OBV_P_O1_WAVE] = {"WAVE", 3, OBV_W_SAW, OBV_K_WAVE, 0},
    [OBV_P_O1_PW] = {"PW", 127, 64, OBV_K_PCT, 0},
    [OBV_P_O1_LVL] = {"LEVEL", 127, 110, OBV_K_NUM, 0},
    [OBV_P_NOISE] = {"NOISE", 127, 0, OBV_K_NUM, 0},
    [OBV_P_O2_WAVE] = {"WAVE", 3, OBV_W_SAW, OBV_K_WAVE, 0},
    [OBV_P_O2_PW] = {"PW", 127, 64, OBV_K_PCT, 0},
    [OBV_P_O2_LVL] = {"LEVEL", 127, 100, OBV_K_NUM, 0},
    [OBV_P_O2_SEMI] = {"SEMI", 48, 24, OBV_K_SEMI, 0},
    [OBV_P_SYNC] = {"SYNC", 1, 0, OBV_K_ONOFF, 0},
    [OBV_P_XMOD] = {"OBV_X-MOD", 127, 0, OBV_K_NUM, 0},
    [OBV_P_ENV_O2] = {"ENV>O2", 127, 64, OBV_K_BIP, 0},
    [OBV_P_O2_FINE] = {"FINE", 127, 70, OBV_K_BIP, 0},
    [OBV_P_CUTOFF] = {"CUTOFF", 127, 80, OBV_K_CUT, 74},
    [OBV_P_RESO] = {"RESO", 127, 20, OBV_K_NUM, 71},
    [OBV_P_FENV_AMT] = {"ENV AMT", 127, 90, OBV_K_BIP, 0},
    [OBV_P_KEYTRK] = {"KEY TRK", 127, 64, OBV_K_PCT, 0},
    [OBV_P_FTYPE] = {"TYPE", OBV_NFTYPE - 1, OBV_F_LP24, OBV_K_FTYPE, 0},
    [OBV_P_DRIVE] = {"DRIVE", 127, 0, OBV_K_NUM, 0},
    [OBV_P_VEL_FLT] = {"VEL>FLT", 127, 40, OBV_K_NUM, 0},
    [OBV_P_VEL_AMP] = {"VEL>AMP", 127, 60, OBV_K_NUM, 0},
    [OBV_P_FA] = {"ATTACK", 127, 20, OBV_K_MS, 0},
    [OBV_P_FD] = {"DECAY", 127, 70, OBV_K_MS, 0},
    [OBV_P_FS] = {"SUSTAIN", 127, 40, OBV_K_NUM, 0},
    [OBV_P_FR] = {"RELEASE", 127, 60, OBV_K_MS, 0},
    [OBV_P_AA] = {"ATTACK", 127, 10, OBV_K_MS, 73},
    [OBV_P_AD] = {"DECAY", 127, 70, OBV_K_MS, 0},
    [OBV_P_AS] = {"SUSTAIN", 127, 100, OBV_K_NUM, 0},
    [OBV_P_AR] = {"RELEASE", 127, 60, OBV_K_MS, 72},
    [OBV_P_LFO_SHAPE] = {"SHAPE", 5, OBV_L_TRI, OBV_K_LSHAPE, 0},
    [OBV_P_LFO_RATE] = {"RATE", 127, 70, OBV_K_HZ, 0},
    [OBV_P_LFO_DELAY] = {"DELAY", 127, 0, OBV_K_NUM, 0},
    [OBV_P_LFO_PHASE] = {"PHASE", 2, OBV_LP_FREE, OBV_K_LPHASE, 0},
    [OBV_P_LFO_PITCH] = {"PITCH", 127, 0, OBV_K_NUM, 0},
    [OBV_P_LFO_PWM] = {"PWM", 127, 0, OBV_K_NUM, 0},
    [OBV_P_LFO_FLT] = {"FILTER", 127, 0, OBV_K_NUM, 0},
    [OBV_P_LFO_AMP] = {"AMP", 127, 0, OBV_K_NUM, 0},
    [OBV_P_LFO_PITCH_TO] = {"PITCH>", 2, OBV_TO_BOTH, OBV_K_TO, 0},
    [OBV_P_LFO_PWM_TO] = {"PWM>", 2, OBV_TO_BOTH, OBV_K_TO, 0},
    [OBV_P_MODE] = {"MODE", 3, OBV_M_POLY, OBV_K_MODE, 0},
    [OBV_P_UNI_N] = {"VOICES", 8, 4, OBV_K_NUM, 0},
    [OBV_P_UNI_DET] = {"DETUNE", 127, 30, OBV_K_NUM, 0},
    [OBV_P_GLIDE] = {"GLIDE", 127, 0, OBV_K_MS, 5},
    [OBV_P_VINTAGE] = {"VINTAGE", 127, 30, OBV_K_NUM, 0},
    [OBV_P_SPREAD] = {"SPREAD", 127, 60, OBV_K_NUM, 0},
    [OBV_P_BEND] = {"BEND", 24, 2, OBV_K_NUM, 0},
    [OBV_P_VOLUME] = {"VOLUME", 127, 100, OBV_K_NUM, 0},
    [OBV_P_MOD_DEPTH] = {"MOD DPT", 127, 0, OBV_K_NUM, 1},
    [OBV_P_ARP_MODE] = {"ARP", 5, OBV_A_OFF, OBV_K_ARP, 0},
    [OBV_P_ARP_DIV] = {"DIV", 6, OBV_D_16, OBV_K_DIV, 0},
    [OBV_P_ARP_OCT] = {"OCTAVES", 4, 1, OBV_K_OCT, 0},
    [OBV_P_ARP_GATE] = {"GATE", 127, 64, OBV_K_PCT, 0},
    [OBV_P_FMORPH] = {"MORPH", 127, 0, OBV_K_MORPH, 0},
    [OBV_P_CHO_RATE] = {"RATE", 127, 40, OBV_K_NUM, 0},
    [OBV_P_CHO_DEPTH] = {"DEPTH", 127, 50, OBV_K_NUM, 0},
    [OBV_P_CHO_MIX] = {"MIX", 127, 0, OBV_K_NUM, 0},
    [OBV_P_DLY_TIME] = {"TIME", OBV_NDDIV - 1, OBV_DD_8D, OBV_K_DDIV, 0},
    [OBV_P_DLY_FB] = {"FEEDBK", 127, 50, OBV_K_NUM, 0},
    [OBV_P_DLY_TONE] = {"TONE", 127, 70, OBV_K_NUM, 0},
    [OBV_P_DLY_MIX] = {"MIX", 127, 0, OBV_K_NUM, 0},
    [OBV_P_REV_SIZE] = {"SIZE", 127, 80, OBV_K_NUM, 0},
    [OBV_P_REV_DAMP] = {"DAMP", 127, 60, OBV_K_NUM, 0},
    [OBV_P_REV_PRE] = {"PRE-DLY", 127, 20, OBV_K_NUM, 0},
    [OBV_P_REV_MIX] = {"MIX", 127, 0, OBV_K_NUM, 0},
};

static const char *const OBV_WAVE_NAME[4] = {"OFF", "SAW", "PULSE", "SAW+PLS"};
static const char *const OBV_FTYPE_NAME[OBV_NFTYPE] = {"LP 12", "HP 12", "BP 12", "NOTCH", "LP 24", "SEM"};
static const char *const OBV_MODE_NAME[4] = {"POLY", "DUAL", "UNISON", "MONO"};
static const char *const OBV_LSHAPE_NAME[6] = {"TRI", "SAW", "RAMP", "SQUARE", "S&H", "RANDOM"};
static const char *const OBV_LPHASE_NAME[3] = {"SYNC", "FREE", "SPREAD"};
static const char *const OBV_TO_NAME[3] = {"OSC1", "OSC2", "BOTH"};
static const char *const OBV_ARP_NAME[OBV_NARPMODE] = {"OFF", "UP", "DOWN", "UP/DN", "RANDOM", "PLAYED"};
static const char *const OBV_DIV_NAME[OBV_NDIV] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T"};
static const uint8_t OBV_DIV_PER_QUARTER[OBV_NDIV] = {1, 2, 3, 4, 6, 8, 12};
static const char *const OBV_DDIV_NAME[OBV_NDDIV] = {"1/16", "1/8T", "1/8", "1/8.", "1/4", "1/4.", "1/2"};
static const uint8_t OBV_DDIV_NUM[OBV_NDDIV] = {1, 1, 1, 3, 1, 3, 2}, DDIV_DEN[OBV_NDDIV] = {4, 3, 2, 4, 1, 2, 1};   /* of a quarter */

/* the pages: four knobs each (-1 = no knob) */
typedef struct { const char *title; int8_t p[4]; } obv_page_t;
enum { OBV_PG_PERFORM, OBV_PG_OSC1, OBV_PG_OSC2, OBV_PG_VOICE, OBV_PG_OSCMOD, OBV_PG_FILTER, OBV_PG_FILTER2, OBV_PG_FENV, OBV_PG_AENV, OBV_PG_LFO, OBV_PG_LFODEST,
       OBV_PG_LFO2, OBV_PG_ARP, OBV_PG_CHORUS, OBV_PG_DELAY, OBV_PG_REVERB, OBV_PG_DRIVE, OBV_PG_OUTPUT, OBV_PG_GLOBAL,   /* (3.3, as native 3.2) VOICE under EDIT, after OSC 2 */
       OBV_PG_MIXER, OBV_PG_SEQ, OBV_NPAGE };              /* the mixer (VIBE), the sequencer's step view: drawn by hand */
static const obv_page_t OBV_PAGE[OBV_NPAGE] = {
    [OBV_PG_PERFORM] = {"PERFORM", {OBV_P_CUTOFF, OBV_P_RESO, OBV_P_FENV_AMT, OBV_P_AR}},   /* (ALGORITHM picks the track now, as sloopDX) */
    [OBV_PG_OSC1] = {"OSC 1", {OBV_P_O1_WAVE, OBV_P_O1_PW, OBV_P_O1_LVL, OBV_P_NOISE}},
    [OBV_PG_OSC2] = {"OSC 2", {OBV_P_O2_WAVE, OBV_P_O2_PW, OBV_P_O2_LVL, OBV_P_O2_SEMI}},
    [OBV_PG_OSCMOD] = {"OSC MOD", {OBV_P_SYNC, OBV_P_XMOD, OBV_P_ENV_O2, OBV_P_O2_FINE}},
    [OBV_PG_FILTER] = {"FILTER", {OBV_P_RESO, OBV_P_FENV_AMT, OBV_P_KEYTRK, OBV_P_FTYPE}},
    [OBV_PG_FILTER2] = {"FILTER +", {OBV_P_DRIVE, OBV_P_VEL_FLT, OBV_P_VEL_AMP, OBV_P_FMORPH}},
    [OBV_PG_FENV] = {"FILTER ENV", {OBV_P_FA, OBV_P_FD, OBV_P_FS, OBV_P_FR}},
    [OBV_PG_AENV] = {"AMP ENV", {OBV_P_AA, OBV_P_AD, OBV_P_AS, OBV_P_AR}},
    [OBV_PG_LFO] = {"LFO", {OBV_P_LFO_SHAPE, OBV_P_LFO_RATE, OBV_P_LFO_DELAY, OBV_P_LFO_PHASE}},
    [OBV_PG_LFODEST] = {"LFO DEST", {OBV_P_LFO_PITCH, OBV_P_LFO_PWM, OBV_P_LFO_FLT, OBV_P_LFO_AMP}},
    [OBV_PG_LFO2] = {"LFO +", {OBV_P_LFO_PITCH_TO, OBV_P_LFO_PWM_TO, -1, -1}},
    [OBV_PG_VOICE] = {"VOICE", {OBV_P_MODE, OBV_P_UNI_N, OBV_P_UNI_DET, OBV_P_GLIDE}},
    [OBV_PG_OUTPUT] = {"OUTPUT", {OBV_P_VINTAGE, OBV_P_SPREAD, OBV_P_BEND, OBV_P_VOLUME}},
    [OBV_PG_ARP] = {"ARP", {OBV_P_ARP_MODE, OBV_P_ARP_DIV, OBV_P_ARP_OCT, OBV_P_ARP_GATE}},
    [OBV_PG_CHORUS] = {"CHORUS", {OBV_P_CHO_RATE, OBV_P_CHO_DEPTH, OBV_P_CHO_MIX, -1}},
    [OBV_PG_DELAY] = {"DELAY", {OBV_P_DLY_TIME, OBV_P_DLY_FB, OBV_P_DLY_TONE, OBV_P_DLY_MIX}},
    [OBV_PG_REVERB] = {"REVERB", {OBV_P_REV_SIZE, OBV_P_REV_DAMP, OBV_P_REV_PRE, OBV_P_REV_MIX}},
    [OBV_PG_DRIVE] = {"DRIVE", {-1, -1, -1, -1}},   /* (VIBE) the drive send: TYPE, DRIVE, TONE, MIX – the settings', drawn by hand */
    [OBV_PG_GLOBAL] = {"GLOBAL", {-1, -1, -1, -1}},   /* tempo, MIDI channel, tune, CPU: globals, drawn by hand */
    [OBV_PG_MIXER] = {"MIXER", {-1, -1, -1, -1}},   /* level, pan, send per track (VIBE): drawn by hand */
    [OBV_PG_SEQ] = {"SEQUENCER", {-1, -1, -1, -1}},   /* length, note value, swing, transpose; a held step: its own four */
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
