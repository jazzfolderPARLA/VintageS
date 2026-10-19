/* SPDX-License-Identifier: GPL-3.0-only */
/* Melodee's Stage (HOME) and the PATTERNS grid (SEQ > PATTERNS), drawn to the redesign's mockups (docs/design:
 * stage_columns, concept_screens).
 * Stage: the header (its chip: the engine, a kit's name); a column per track: a bar and its role (the sound's
 * category) in its colour, the sound in two lines, the engine, its pattern (the next one dashed beside it while it
 * waits for the bar), the bar playing as 4 x 4 steps, its level as a ring; the selected one lifted and outlined. Under
 * them the panel: the notes the selected track played and their chord in front of its waveform, smooth, dimmed while
 * held (a DRUM track: its lanes, lit as they hit). The first turn of a knob drops the track's four knobs (its engine's,
 * stage_page) down over the columns' tops (ST_DROP frames); while they are out the panel shows what the knob turning
 * shapes and its value; ST_KNOB_MS after the last turn they go and the columns are whole again.
 * Each part remembers what it drew: a column redraws when its sound, pattern, steps or level change, the playhead its
 * steps alone; the panel follows the scope every other frame, drawn last (the SPI, 12 MHz, sends it while the next
 * frame is worked out: a blit right after it would wait for it, lcd_window).
 */
#define ST_COL_Y Y_LABEL                                /* the columns: x CARD_X(k), y 22 .. 174, the cards' width */
#define ST_COL_H 152
#define ST_GRID_Y 84                                    /* .. their steps: 4 x 4, rows 84 .. 121 (a canvas of their own) */
#define ST_GRID_H 38
#define ST_PANEL_Y (ST_COL_Y + ST_COL_H + 4)            /* the panel: y 178 .. 238 */
#define ST_PANEL_H 60
#define ST_KNOB_MS 1500u                                /* the knobs stay this long after the last turn */
#define ST_DROP 4u                                      /* .. and drop in over this many frames */

static struct {
    uint32_t col[NTRK], grid[NTRK];                     /* each column's drawn state, its steps' */
    uint8_t hit_t[NLANE];                               /* frames a DRUM lane's name stays lit */
    uint8_t hit_trk;                                    /* .. of this track */
    uint32_t knob_ms;                                   /* the last knob turn on Stage (| 1), 0 none */
    uint8_t out, drop;                                  /* the knobs out; frames still dropping */
} stage;

static void stage_knob_touch(void) { stage.knob_ms = fm1_ms | 1u; }   /* (ui_input.c: a knob on Stage) */
static int stage_knobs_out(void) { return stage.knob_ms && fm1_ms - (stage.knob_ms & ~1u) < ST_KNOB_MS; }

/* ------------------------------------------------------- the knobs --- */
/* a knob's label: the panel's, plain words for CZ-1's (its panel says R1, L1 ..) */
static const char *stage_label(uint32_t k, const param_desc_t *d)
{
    static const char *const CZ_L[4] = {"WAVE", "DCW", "DETUNE", "VIBRATO"};
    return stage_pg.scope == SC_CZ1 ? CZ_L[k & 3u] : d->label;
}
/* a knob's value as Stage shows it: the Prophet's 0..127 panel values in % (the mockup's CUTOFF 62 %) */
static void stage_value(const page_t *sp, const param_desc_t *d, int32_t v, char *val, const char **unit)
{
    if (sp->scope == SC_P5 && d->fmt == F_INT && d->min == 0 && d->max == 127) {
        fmt_int(val, (v * 100 + 63) / 127);
        *unit = "%";
        return;
    }
    param_format(d, v, val, unit);
}
/* the four cards (while the knobs are out): a value without effect on the sound as it is drawn dim */
static void stage_columns(void)
{
    const page_t *sp = stage_page();
    uint32_t c;
    for (c = 0; c < 4u; c++) {
        int16_t *vp;
        const char *unit;
        char val[12];
        const param_desc_t *d = page_desc(sp, c, &vp);
        int dim = (sp->scope == SC_CZ1 && !cz_ed_active(cz_patch[song.sel % NTRK].raw, sp->id[c])) ||
                  (sp->scope == SC_P5 && !p5_ed_active(TSEL, sp->id[c]));
        if (!d || !vp || !d->label || d->label[0] == '-') {
            draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);
            continue;
        }
        stage_value(sp, d, *vp, val, &unit);
        draw_column(c, stage_label(c, d), val, unit, dim ? T_DIM : VAL(c), d->fmt == F_ENUM && d->max < 2 ? -1 : RATIO(d, *vp),
                    ICON_NONE);
    }
}

/* ------------------------------------------------------- the panel --- */
/* the output's waveform over the panel's width, smooth: triggered on a rising zero crossing, auto-scaled, each sample
 * the mean of three, 2 px anti-aliased (gfx.c cv_vspan_aa); rows top .. top + h */
static int32_t wv_peak, wv_trig;                         /* the snapshot's peak and trigger (stage_wave_snap) */
static void stage_wave_snap(void)
{
    int16_t *snap = scope_snap;
    uint32_t w = scope_w, i, trig = 1;
    int32_t peak = 1500;
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak) peak = snap[i];
        else if (-snap[i] > peak) peak = -snap[i];
    }
    for (i = 2; i < SCOPE_N - 240u; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) { trig = i; break; }
    wv_peak = peak;
    wv_trig = (int32_t)trig;
}
static void stage_wave_draw_x(uint16_t c, int32_t x0, int32_t w, int32_t top, int32_t h)
{
    int16_t *snap = scope_snap;
    uint32_t trig = (uint32_t)wv_trig;
    int32_t peak = wv_peak, x, py = 0, cy = (top + h / 2) * 16 - 16;
    for (x = 0; x < w; x++) {
        uint32_t k = trig + (uint32_t)x;
        int32_t v = ((int32_t)snap[k - 1u] + 2 * snap[k] + snap[k + 1u]) / 4;
        int32_t y = cy - v * (h / 2 - 4) * 16 / peak;
        cv_vspan_aa(x0 + x, x ? py : y, y, c);
        py = y;
    }
}
static void stage_wave_draw(uint16_t c, int32_t top, int32_t h) { stage_wave_draw_x(c, 8, 224, top, h); }
static void stage_wave(uint16_t c, int32_t top, int32_t h)
{
    stage_wave_snap();
    stage_wave_draw(c, top, h);
}

/* the notes the selected track played (live_last: its last voicing, held or let go), the chord they make, in front of
 * what is on the canvas (cv_over): the root 30 px, its quality 15 px, then the notes 11 px, on one baseline (38).
 * Held: the chord in text, the notes in the track's colour; let go: both faded into the panel. 0 none, 1 let go, 2 held */
static int stage_notes(int32_t x1)
{
    uint32_t bits[4], i, n = 0, pcs = 0, root = 0, bass, held = 0, k = song.sel % NTRK;
    uint8_t note[8];
    char b[48];
    const char *q;
    uint16_t c, cn;
    int32_t x = 10;
    fm1_irq_off();                                      /* one coherent ISR snapshot, nothing drawn meanwhile */
    for (i = 0; i < 4u; i++) {
        bits[i] = live_last[k][i];
        held |= live_held[k][i];
    }
    fm1_irq_on();
    for (i = 0; i < 128u; i++)
        if ((bits[i >> 5] >> (i & 31u)) & 1u) {
            pcs |= 1u << (i % 12u);
            if (n < sizeof note) note[n] = (uint8_t)i;
            n++;
        }
    if (!n) return 0;
    c = held ? T_TEXT : ux_mix(T_PANEL, T_THEME, 75);
    cn = held ? T_THEME : ux_mix(T_PANEL, T_THEME, 55);
    bass = note[0] % 12u;
    q = micro_active(TSEL) ? 0 : chord_of(pcs, bass, &root);
    if (q) {
        x = cv_text(x, 9, &AF_L, N_NOTE[root], c);
        x = cv_text(x + 1, 23, &AF_M, q, c);
        if (root != bass) {
            x = cv_text(x + 3, 9, &AF_L, "/", c);
            x = cv_text(x, 9, &AF_L, N_NOTE[bass], c);
        }
        x += 10;
    }
    notes_fit(b, note, n, &AF_S, x1 - x);
    cv_text(x, 27, &AF_S, b, cn);
    return (int)(1u + (held != 0u));
}
/* HOME's notes alone (tests/ui_test.c): 1 = there were notes */
static int graph_notes(void) { return stage_notes(220) != 0; }

/* a DRUM track: its eight lanes in a row at the panel's foot, a lane's name lit while it hits (drum_flash: the ISR's) */
static void stage_hits(void)
{
    uint32_t l;
    cv_over = 0;
    for (l = 0; l < NLANE; l++) {
        int32_t x = 7 + (int32_t)l * 28;
        int lit = stage.hit_t[l] != 0u;
        if (lit) {
            cv_rrect(x, 38, 26, 16, 4, T_THEME, T_PANEL);
        } else {                                        /* (an unlit lane: outlined in line) */
            cv_rrect(x, 38, 26, 16, 4, T_LINE, T_PANEL);
            cv_rrect(x + 1, 39, 24, 14, 3, T_SURF, T_LINE);
        }
        cv_text_c(x + 13, 40, &AF_X, drum_lane_abbr(TSEL, l), lit ? T_INK : T_DIM, lit ? T_THEME : T_SURF);
    }
}

/* While the knobs are out, the panel shows what the one turning shapes behind the notes: a filter's response (cutoff,
 * resonance; the Prophet's envelope amount too), an envelope (its attack .. release); 0 = nothing to show */
enum { SP_NONE, SP_FILTER, SP_ENV };
static uint32_t stage_picture(int32_t *v)
{
    const page_t *sp = stage_page();
    uint32_t hot = ui.hot_col & 3u, id = sp->id[hot], k;
    if (!stage_knobs_out())
        return SP_NONE;
    if (sp->scope == SC_P5) {
        const uint8_t *r = p5_patch_of(TSEL)->raw;
        if (id == P5_CUTOFF || id == P5_RESONANCE || id == P5_ENV_FILTER) {
            v[0] = r[P5_CUTOFF];
            v[1] = r[P5_RESONANCE];
            return SP_FILTER;
        }
        if (id == P5_RELEASE_AMP) {
            v[0] = r[P5_ATTACK_AMP]; v[1] = r[P5_DECAY_AMP]; v[2] = r[P5_SUSTAIN_AMP]; v[3] = r[P5_RELEASE_AMP];
            return SP_ENV;
        }
        return SP_NONE;
    }
    if (sp->scope != SC_ENGINE)
        return SP_NONE;
    if (id >= P_ATK && id <= P_REL) {
        for (k = 0; k < 4u; k++)
            v[k] = TSEL->p[P_ATK + k];
        return SP_ENV;
    }
    {   /* an engine's filter: its F_CUTOFF knob and the RES beside it */
        int32_t cut = -1, res = 0;
        for (k = 0; k < 4u; k++) {
            const param_desc_t *d = track_desc(TSEL, sp->id[k]);
            if (d->fmt == F_CUTOFF) cut = (int32_t)k;
            else if (str_eq(d->label, "RES")) res = TSEL->p[sp->id[k]];
        }
        if (cut < 0 || (hot != (uint32_t)cut && !str_eq(track_desc(TSEL, id)->label, "RES")))
            return SP_NONE;
        v[0] = TSEL->p[sp->id[cut]];
        v[1] = res;
        return SP_FILTER;
    }
}
/* a low-pass response: flat, the resonance's peak at the cutoff (a dotted line), then the slope; cut, res 0..127 */
static void stage_filter(int32_t cut, int32_t res, uint16_t c)
{
    int32_t x, fc = 10 + clamp(cut, 0, 127) * 200 / 127, peak = clamp(res, 0, 127) * 14 / 127;
    int32_t top = 3, bot = ST_PANEL_H - 4, y0 = 22 * 16, py = 0;
    for (x = top; x < bot; x += 5)
        cv_rect(fc, x, 1, 2, ux_mix(T_PANEL, c, 60));
    for (x = 0; x < 220; x++) {
        int32_t d = 8 + x - fc, y = y0 - peak * 16 * 64 / (64 + d * d) + (d > 0 ? d * 10 : 0);
        y = clamp(y, top * 16, bot * 16);
        cv_vspan_aa(8 + x, x ? py : y, y, c);
        py = y;
    }
}

static void stage_panel_hits(void)                      /* every frame: the DRUM lanes hit since the last one */
{
    uint8_t f[NTRK];
    uint32_t k, sel = song.sel % NTRK, l;
    fm1_irq_off();
    for (k = 0; k < NTRK; k++) {
        f[k] = drum_flash[k];
        drum_flash[k] = 0;
    }
    fm1_irq_on();
    if (stage.hit_trk != sel + 1u) {
        stage.hit_trk = (uint8_t)(sel + 1u);
        memset(stage.hit_t, 0, sizeof stage.hit_t);
    }
    for (l = 0; l < NLANE; l++)
        stage.hit_t[l] = (f[sel] >> l) & 1u ? 10u : stage.hit_t[l] ? stage.hit_t[l] - 1u : 0u;
}
static void stage_panel(void)
{
    uint32_t sel = song.sel % NTRK, drum = (uint32_t)drum_track(TSEL), pic;
    int32_t pv[4];
    int held = 0;
    cv_begin(240, ST_PANEL_H, T_BG);
    cv_rrect(4, 0, 232, ST_PANEL_H, 6, T_PANEL, T_BG);
    cv_bg = T_PANEL;
    if (!drum) {
        uint32_t i;
        fm1_irq_off();
        for (i = 0; i < 4u; i++) held |= live_held[sel][i] != 0u;
        fm1_irq_on();
    }
    pic = drum ? SP_NONE : stage_picture(pv);
    /* behind: the waveform (dimmed while notes are held, or over a drum's lanes), or what the knob turning shapes */
    if (pic == SP_FILTER)
        stage_filter(pv[0], pv[1], ux_mix(T_PANEL, T_THEME, 55));
    else if (pic == SP_ENV)
        graph_adsr_v(pv[0], pv[1], pv[2], pv[3], 4, ST_PANEL_H - 8, ux_mix(T_PANEL, T_THEME, 55));
    else
        stage_wave(held || drum ? ux_mix(T_PANEL, T_THEME, 40) : T_THEME, drum ? 2 : 4, drum ? 34 : ST_PANEL_H - 8);
    cv_over = 1;                                        /* in front: blended over it */
    if (drum) {
        stage_hits();
    } else {
        stage_notes(pic ? 176 : 228);
        if (pic) {                                      /* the value turning, at the top right */
            int16_t *vp;
            const char *unit;
            char val[12];
            const page_t *sp = stage_page();
            const param_desc_t *d = page_desc(sp, ui.hot_col & 3u, &vp);
            if (d && vp) {
                stage_value(sp, d, *vp, val, &unit);
                str_cpy(val + str_len(val), unit, 4);
                cv_text_r(228, 4, &AF_M, val, T_THEME, T_PANEL);
            }
        }
    }
    cv_over = 0;
    cv_blit(0, ST_PANEL_Y);
}

/* ------------------------------------------------------ the columns --- */
/* track k's sound as a list entry (browse.c cur_entry, which reads the selected track) */
static void stage_entry(uint32_t k, uint32_t *src, uint32_t *kk)
{
    uint8_t s = song.sel;
    song.sel = (uint8_t)k;
    cur_entry(src, kk);
    song.sel = s;
}
/* a name in two lines of w px: the first ends at the last space, '.' or '-' that fits (one long word: cut), the rest below */
static void stage_two_lines(const char *s, char *a, char *b, int32_t w)
{
    uint32_t i, cut = 0;
    for (i = 0; s[i] && i < 15u; i++)                   /* (cut: the first line's length; a space goes) */
        if (s[i] == ' ' || s[i] == '.' || s[i] == '-') {
            uint32_t n = i + (s[i] != ' ');
            str_cpy(a, s, n + 1u);
            if (n && text_w(&AF_S, a) <= w) cut = n;
        }
    if (!cut || text_w(&AF_S, s) <= w) {
        str_cpy(a, s, 16);
        b[0] = 0;
        return;
    }
    str_cpy(a, s, cut + 1u);
    str_cpy(b, s + cut + (s[cut] == ' '), 16);
}
/* column k's 4 x 4 steps (the bar playing): hits in the track's colour, the playhead larger in text, the rest in line;
 * a canvas of 50 x ST_GRID_H at the column's (2, ST_GRID_Y), clear of the level's ring under it */
static void stage_grid_at(const track_t *t, uint32_t page, uint32_t ph, uint16_t tc, uint16_t bg, int32_t ox)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN];
    for (i = 0; i < 16u; i++) {
        uint32_t si = page * 16u + i;
        const step_t *st = &seq_steps(t)[si % NSTEP];
        int32_t x = ox + 8 + (int32_t)(i % 4u) * 113 / 10, y = 6 + (int32_t)(i / 4u) * 95 / 10;
        if (si >= len)
            break;
        cv_circle(x, y, si == ph ? 6 : 5, si == ph ? T_TEXT : step_on(st) ? tc : st->time == ST_TIE ? ux_mix(bg, tc, 40) : T_LINE, bg);
    }
}
static void __attribute__((unused)) stage_col(uint32_t k)   /* (the four tracks: not drawn in ATMOS, kept) */
{
    const track_t *t = &trk[k];
    uint32_t sel = k == song.sel, mute = t->p[P_MUTE] != 0u, arm = (song.rec >> k) & 1u, len = (uint32_t)t->p[P_SLEN];
    uint32_t page = song.playing && t->seq_idx < len ? t->seq_idx / 16u : 0u, ph = song.playing ? t->seq_idx : 0xFFFFu;
    uint32_t next = t->pattern_next < NPAT ? t->pattern_next + 1u : 0u, lvl = (uint32_t)t->p[P_LEVEL] & 127u, sig, src, kk;
    int pending = sel && browse_pending();
    uint16_t tc = mute ? T_DIM : T_TRK(k), bg = sel ? T_LIFT : T_PANEL;
    int32_t x = CARD_X(k);
    char nm[16], l1[16], l2[16], chip[4] = {'P', (char)('1' + t->pattern % NPAT), 0, 0};
    const char *en;
    if (pending) {                                      /* browsing: the sound the list shows, not yet loaded */
        char tag[6];
        src = browse_shown(&kk);
        entry_label(src, kk, tag, nm);
        if (!nm[0]) str_cpy(nm, tag, sizeof nm);
    } else {
        trk_short_name(k, nm);
        if (t->eng_req == ENGI_PROPHET)
            p5_short_name(t, nm, sizeof nm);
        stage_entry(k, &src, &kk);
    }
    en = ENGINES[eng_idx(pending ? src_engine(src, kk) : t->eng_req)]->name;
    sig = str_hash(str_hash(1u + sel * 2u + mute * 4u + arm * 8u + (uint32_t)pending * 16u + stage.out * 32u, nm), en) +
          next * 131u + lvl * 40503u + t->pattern * 7919u + len * 13u + ux.gen * 977u + ux.pal * 31u + entry_cat(src, kk) * 61u;
    if (!ui.force && sig == stage.col[k]) {
        uint32_t g = steps_hash(t) + page * 613u + ph * 104729u;
        if (g != stage.grid[k]) {                       /* the steps or the playhead: the grid only */
            stage.grid[k] = g;
            cv_begin(50, ST_GRID_H, bg);
            stage_grid_at(t, page, ph, tc, bg, 0);
            cv_blit((uint32_t)(x + 2), (uint32_t)(ST_COL_Y + ST_GRID_Y));
        }
        return;
    }
    stage.col[k] = sig;
    stage.grid[k] = steps_hash(t) + page * 613u + ph * 104729u;
    cv_begin(CARD_W, ST_COL_H, T_BG);
    if (sel) {                                          /* the selected track: lifted, outlined in its colour */
        cv_rrect(0, 0, CARD_W, ST_COL_H, 6, T_TRK(k), T_BG);
        cv_rrect(1, 1, CARD_W - 2, ST_COL_H - 2, 5, bg, T_TRK(k));
    } else {
        cv_rrect(0, 0, CARD_W, ST_COL_H, 6, bg, T_BG);
    }
    if (!stage.out) {                                   /* (under the knobs, while they are out: not drawn) */
        cv_rect(3, 0, CARD_W - 6, 3, tc);               /* the bar */
        {   /* the role: the sound's category, in NEW SONG's words (DRUMS, CHORDS); none, OTHER: the track */
            uint32_t cat = entry_cat(src, kk) % CAT_N;
            char tn[3] = {'T', (char)('1' + k), 0};
            cv_text_on(6, 6, &AF_X, cat == CAT_DRUM ? "DRUMS" : cat == CAT_KEYS ? "CHORDS" : cat && cat != CAT_OTHER ? CAT_NAME[cat] : tn, tc, bg);
        }
        if (arm)                                        /* armed: a REC dot */
            cv_circle(47, 10, 6, T_REC, bg);
        stage_two_lines(nm, l1, l2, CARD_W - 10);
        cv_free_text(6, 20, &AF_S, l1, pending ? T_ACCENT : mute ? T_DIM : T_TEXT, bg, CARD_W - 8);
        if (l2[0])
            cv_free_text(6, 34, &AF_S, l2, pending ? T_ACCENT : mute ? T_DIM : T_TEXT, bg, CARD_W - 8);
    }
    cv_text_fit(6, 49, &AF_X, en, T_MID, bg, CARD_W - 8);
    cv_rrect(6, 64, 22, 14, 4, tc, bg);                 /* the pattern playing */
    cv_text_c(17, 65, &AF_X, chip, T_INK, tc);
    if (next) {                                         /* the next one, waiting for the bar: dashed */
        char n2[3] = {'P', (char)('0' + next), 0};
        cv_dashed(31, 64, 20, 14, 3, tc);
        cv_text_c(41, 65, &AF_X, n2, tc, bg);
    }
    {   /* the steps, as stage_grid draws them on their own canvas (2 px in, from row ST_GRID_Y) */
        int32_t ox = 2;
        cv_oy = ST_GRID_Y;
        stage_grid_at(t, page, ph, tc, bg, ox);
        cv_oy = 0;
    }
    {   /* the level: a ring (KNOB_RING, 3 px, from 7:30), its dB inside */
        char v[8];
        int32_t a1 = lvl ? -KA_END + (int32_t)lvl * 2 * KA_END / 127 : -KA_END - 1;
        knob_arc(27 - KNOB_RING_R, 136 - KNOB_RING_R, KNOB_RING_R, KNOB_RING_COV, KNOB_RING_ANG, -KA_END, a1, T_LINE, tc, bg);
        if (lvl) {
            int32_t d = LEVEL_DB_X10[lvl];
            fmt_int(v, (d + (d < 0 ? -5 : 5)) / 10);
        } else {
            str_cpy(v, "OFF", sizeof v);
        }
        cv_text_c(27, 131, &AF_X, v, T_TEXT, bg);
    }
    if (stage.out)                                      /* the knobs over the tops: the rows under them only */
        cv_blit_from((uint32_t)x, ST_COL_Y, CARD_H);
    else
        cv_blit((uint32_t)x, ST_COL_Y);
}

/* the knobs: out while one turns (and ST_KNOB_MS after), dropping in over ST_DROP frames (each its cards' bottom rows,
 * more each frame: no column under them redraws); gone at once, the columns whole again */
static void stage_knobs(void)
{
    int out = stage_knobs_out();
    if (out && !stage.out) {
        stage.out = 1;
        stage.drop = ST_DROP;
    } else if (!out && stage.out) {
        stage.out = 0;
        memset(stage.col, 0, sizeof stage.col);         /* (the columns' tops back) */
        memset(ui.col, 0, sizeof ui.col);
        return;
    }
    if (!stage.out)
        return;
    if (stage.drop) {                                   /* each card whole (its value snaps, no roll), its bottom rows */
        uint32_t c;
        for (c = 0; c < 4u; c++) {
            ui.col[c][0] = 0;
            ui.roll[c].sig = (uint8_t)~ui.roll[c].sig;
        }
        stage_drop_rows = (uint8_t)((ST_DROP + 1u - stage.drop) * CARD_H / ST_DROP);
        stage_columns();
        stage_drop_rows = 0;
        stage.drop--;
        return;
    }
    stage_columns();
}

/* the BG around the columns and the panel (a full redraw: the canvases cover the rest) */
static void stage_frame(void)
{
    uint32_t k;
    for (k = 0; k <= NTRK; k++) {                       /* left of each column, right of the last */
        uint32_t x0 = k ? (uint32_t)(CARD_X(k - 1u) + CARD_W) : 0u, x1 = k < NTRK ? (uint32_t)CARD_X(k) : 240u;
        lcd_fill(x0, ST_COL_Y, x1 - x0, ST_COL_H, T_BG);
    }
    lcd_fill(0, ST_COL_Y + ST_COL_H, 240, ST_PANEL_Y - ST_COL_Y - ST_COL_H, T_BG);
    lcd_fill(0, ST_PANEL_Y + ST_PANEL_H, 240, 240u - ST_PANEL_Y - ST_PANEL_H, T_BG);
}

/* ------------------------------------------------ ATMOS: the sounds --- */
/* ATMOS has no sequencer tracks: Stage shows the sound alone (SINGLE: bank, name large, category) or the two layers
 * side by side (DUAL: A, B; SPLIT: the low zone, which plays layer B, then the high one, layer A). One card of
 * 232 x ST_COL_H at (4, ST_COL_Y), drawn in two bands (a canvas holds 240 x 124 at most) by moving cv_oy. While the
 * knobs are out they cover its top CARD_H rows (the bank and role stay under them, the names show). */
static uint32_t atmos_bank_of(uint32_t e);               /* ui_input.c */
static const char *atmos_bank_word(uint32_t b);
#define ST_SND_W 232
#define ST_SND_BAND 76                                  /* rows a band holds: 2 x 76 = ST_COL_H */
typedef struct {
    char nm[16];
    const char *en;
    uint32_t bank, cat, num, total;                     /* num of total: the sound's place in its engine's list */
    int pend, fav, user;
} stage_snd_t;

static void stage_layer(uint32_t k, stage_snd_t *s)
{
    const track_t *t = &trk[k];
    uint32_t src, kk, eng;
    s->pend = k == song.sel % NTRK && browse_pending();
    if (s->pend) {                                      /* browsing: the sound the list shows, not yet loaded */
        char tag[6];
        src = browse_shown(&kk);
        entry_label(src, kk, tag, s->nm);
        if (!s->nm[0])
            str_cpy(s->nm, tag, sizeof s->nm);
        eng = src_engine(src, kk);
    } else {
        trk_short_name(k, s->nm);
        if (t->eng_req == ENGI_PROPHET)
            p5_short_name(t, s->nm, sizeof s->nm);
        stage_entry(k, &src, &kk);
        eng = t->eng_req;
    }
    s->en = ENGINES[eng_idx(eng)]->name;
    s->bank = atmos_bank_of(eng);
    s->cat = entry_cat(src, kk);
    {   uint32_t ku = user_of(t);
        s->user = !s->pend && ku < USER_NONE;
        s->num = s->user ? ku + 1u : (uint32_t)t->preset + 1u;
        s->total = ENGINES[eng_idx(eng)]->npresets;
        s->fav = !s->pend && preset_favorite_of(t);
    }
}
/* a name over up to two lines of w px in font f: the first ends at the last space, '.' or '-' that fits */
static void stage_name_lines(const char *s, const aafont_t *f, int32_t w, char *a, char *b)
{
    uint32_t i, cut = 0;
    b[0] = 0;
    if (text_w(f, s) <= w) {
        str_cpy(a, s, 16);
        return;
    }
    for (i = 0; s[i] && i < 15u; i++)
        if (s[i] == ' ' || s[i] == '.' || s[i] == '-') {
            uint32_t n = i + (s[i] != ' ');
            str_cpy(a, s, n + 1u);
            if (n && text_w(f, a) <= w)
                cut = n;
        }
    if (!cut) {                                         /* one long word: its end is cut by the free text */
        str_cpy(a, s, 16);
        return;
    }
    str_cpy(a, s, cut + 1u);
    str_cpy(b, s + cut + (s[cut] == ' '), 16);
}
/* a line centred on cx when it fits in w px, else from cx - w / 2 with its end cut */
static void stage_line(int32_t cx, int32_t y, const aafont_t *f, const char *s, uint16_t c, uint16_t bg, int32_t w)
{
    if (text_w(f, s) <= w)
        cv_text_c(cx, y, f, s, c, bg);
    else
        cv_free_text(cx - w / 2, y, f, s, c, bg, w);
}
static void stage_bank_chip(int32_t x, int32_t y, int32_t sz, const aafont_t *f, uint32_t bank, uint16_t tc, uint16_t under)
{
    char l[2] = {bank < ATMOS_NBANK ? (char)('A' + bank) : '-', 0};
    cv_rrect(x, y, sz, sz, 6, tc, under);
    cv_text_c(x + sz / 2, y + (sz - (int32_t)f->h) / 2, f, l, T_INK, tc);
}
static void stage_card_bg(int32_t x, int32_t w, uint16_t tc)
{
    cv_rrect(x, 0, w, ST_COL_H, 6, tc, T_BG);           /* outlined in the layer's colour, lifted */
    cv_rrect(x + 1, 1, w - 2, ST_COL_H - 2, 5, T_LIFT, tc);
    cv_rect(x + 4, 0, w - 8, 3, tc);
}
/* SINGLE: the bank (a lettered chip and its name, the category), the sound's name large, the mode at the foot */
static void stage_card_single(const stage_snd_t *s, uint16_t tc)
{
    char a[16], b[16];
    uint16_t nc = s->pend ? T_ACCENT : T_TEXT;
    stage_card_bg(0, ST_SND_W, tc);
    stage_bank_chip(10, 9, 32, &AF_M, s->bank, tc, T_LIFT);
    cv_text_fit(50, 8, &AF_M, s->bank < ATMOS_NBANK ? atmos_bank_word(s->bank) : s->en, T_TEXT, T_LIFT, 170);
    if (s->cat && s->cat != CAT_OTHER && s->cat < CAT_N)
        cv_text_on(50, 29, &AF_X, CAT_NAME[s->cat], T_MID, T_LIFT);
    stage_name_lines(s->nm, &AF_L, 212, a, b);
    if (b[0]) {
        stage_line(116, 52, &AF_L, a, nc, T_LIFT, 212);
        stage_line(116, 90, &AF_L, b, nc, T_LIFT, 212);
    } else {
        stage_line(116, 71, &AF_L, a, nc, T_LIFT, 212);
    }
    cv_text_c(116, 134, &AF_X, "SINGLE", T_DIM, T_LIFT);
    if (s->fav)                                         /* a star when the sound is a favourite */
        cv_icon_on(212, 12, 12, ICON_X_STAR, T_THEME, T_LIFT);
}
/* DUAL / SPLIT: half a card: its role, the bank, the name in two lines */
static void stage_card_half(int32_t x, const stage_snd_t *s, uint16_t tc, const char *role)
{
    const int32_t w = 114;
    char a[16], b[16];
    uint16_t nc = s->pend ? T_ACCENT : T_TEXT;
    stage_card_bg(x, w, tc);
    cv_text_on(x + 7, 8, &AF_X, role, tc, T_LIFT);
    stage_bank_chip(x + 7, 24, 22, &AF_S, s->bank, tc, T_LIFT);
    cv_text_fit(x + 34, 27, &AF_S, s->bank < ATMOS_NBANK ? atmos_bank_word(s->bank) : s->en, T_TEXT, T_LIFT, w - 40);
    stage_name_lines(s->nm, &AF_M, w - 14, a, b);
    cv_free_text(x + 7, 62, &AF_M, a, nc, T_LIFT, w - 12);
    if (b[0])
        cv_free_text(x + 7, 84, &AF_M, b, nc, T_LIFT, w - 12);
    if (s->cat && s->cat != CAT_OTHER && s->cat < CAT_N)
        cv_text_on(x + 7, 132, &AF_X, CAT_NAME[s->cat], T_MID, T_LIFT);
}
/* a note as "C3" (the split point's) */
static void stage_note_name(char *d, uint32_t n)
{
    str_cpy(d, N_NOTE[n % 12u], 4);
    fmt_int(d + str_len(d), (int32_t)(n / 12u) - 1);
}
/* "12 / 128" (the sound's place in its engine's list) or "USER 5" */
static void stage_num_text(const stage_snd_t *s, char *d)
{
    d[0] = 0;
    if (s->pend || !s->num)
        return;
    if (s->user) {
        str_cpy(d, "USER ", 8);
        fmt_int(d + 5, (int32_t)s->num);
        return;
    }
    fmt_int(d, (int32_t)s->num);
    str_cpy(d + str_len(d), " / ", 4);
    fmt_int(d + str_len(d), (int32_t)s->total);
}
static uint32_t atmos_voices(uint32_t k)                /* the voices of a track that sound now (held or releasing) */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[k].v[i].active != 0;
    return n;
}
/* PLAY's monitor (ATMOS): the output's wave large with the level on the far right (VU, 20 segments of 3 dB, a peak segment
 * that holds), under them the sound or the two sounds (bank, name, number, a star when it is a favourite), the mode and the
 * voices sounding. Drawn in two bands of 108 rows over the whole Stage (the sounds card and the panel). */
#define ST_MON_BAND 108
static void stage_monitor_row(int32_t y, const stage_snd_t *s, uint16_t tc, const char *role)
{
    char n[20], l[40];
    stage_bank_chip(0, y, 22, &AF_S, s->bank, tc, T_BG);
    cv_text_fit(28, y, &AF_M, s->nm, s->pend ? T_ACCENT : T_TEXT, T_BG, 160);
    cv_icon_on(196, y + 3, 12, s->fav ? ICON_X_STAR : ICON_X_STAR_O, s->fav ? T_THEME : T_DIM, T_BG);
    stage_num_text(s, n);
    str_cpy(l, role, 20);
    if (n[0]) {
        str_cpy(l + str_len(l), "  ", 3);
        str_cpy(l + str_len(l), n, 16);
    }
    cv_text_on(28, y + 20, &AF_X, l, T_MID, T_BG);
}
static void stage_monitor_card(const stage_snd_t *sa, const stage_snd_t *sb, uint32_t mode, uint32_t ka, uint32_t kb,
                               uint32_t lvl, uint32_t pk, uint32_t voices)
{
    char l[40], n[20];
    uint16_t ca = T_TRK(ka), cb = T_TRK(kb);
    uint32_t i;
    cv_rrect(0, 0, 214, 134, 6, T_LINE, T_BG);          /* the scope, large */
    cv_rrect(1, 1, 212, 132, 5, T_LIFT, T_LINE);
    stage_wave_draw_x(ca, 6, 202, 3, 128);
    for (i = 0; i < ATMOS_VU_N; i++) {                  /* the VU, from the bottom: green, then yellow from -15 dB, red from -6 dB */
        uint16_t c = i >= 18u ? T_REC : i >= 14u ? T_TRK(1) : T_THEME;
        cv_rect(221, 206 - (int32_t)i * 10, 10, 8, i < lvl || i + 1u == pk ? c : T_LIFT);
    }
    if (!mode) {
        stage_bank_chip(0, 140, 24, &AF_M, sa->bank, ca, T_BG);
        cv_text_fit(32, 142, &AF_M, sa->bank < ATMOS_NBANK ? atmos_bank_word(sa->bank) : sa->en, T_TEXT, T_BG, 150);
        cv_icon_on(196, 146, 12, sa->fav ? ICON_X_STAR : ICON_X_STAR_O, sa->fav ? T_THEME : T_DIM, T_BG);
        stage_line(106, 164, &AF_L, sa->nm, sa->pend ? T_ACCENT : T_TEXT, T_BG, 212);
        stage_num_text(sa, n);
        cv_text_on(0, 205, &AF_X, n, T_MID, T_BG);
        cv_text_c(106, 205, &AF_X, "SINGLE", T_TEXT, T_BG);
    } else {
        char low[16], high[16], nn[6];
        stage_note_name(nn, atmos.split_point);
        str_cpy(low, "LOW  < ", sizeof low);
        str_cpy(low + str_len(low), nn, 6);
        str_cpy(high, "HIGH  ", sizeof high);
        str_cpy(high + str_len(high), nn, 6);
        str_cpy(high + str_len(high), "+", 2);
        if (mode == ATMOS_DUAL) {
            stage_monitor_row(140, sa, ca, "LAYER A");
            stage_monitor_row(170, sb, cb, "LAYER B");
        } else {
            stage_monitor_row(140, sb, cb, low);        /* below the split point: layer B */
            stage_monitor_row(170, sa, ca, high);
        }
        cv_text_on(0, 205, &AF_X, mode == ATMOS_DUAL ? "DUAL" : "SPLIT", T_TEXT, T_BG);
    }
    str_cpy(l, "VOICES ", 8);
    fmt_int(l + 7, (int32_t)voices);
    cv_text_on(212 - text_w(&AF_X, l), 205, &AF_X, l, T_TEXT, T_BG);
}
static void stage_monitor(void)
{
    static uint32_t vu_lvl, vu_pk, vu_ms;
    uint32_t mode = atmos.mode % ATMOS_NMODE, ka = song.sel % NTRK, kb = atmos_b_track(ka, NTRK), band, pk, lvl, voices;
    stage_snd_t sa, sb;
    pk = atmos_vu_peak;                                 /* the output's peak since the last frame */
    atmos_vu_peak = 0;
    lvl = atmos_vu_level(pk);
    if (lvl >= vu_lvl)
        vu_lvl = lvl;
    else if (vu_lvl)
        vu_lvl--;                                       /* it falls a segment a frame */
    if (lvl >= vu_pk) {
        vu_pk = lvl;
        vu_ms = fm1_ms;
    } else if (vu_pk && fm1_ms - vu_ms > 700u) {        /* the peak holds 0.7 s, then falls */
        vu_pk--;
        vu_ms = fm1_ms - 600u;
    }
    stage_layer(ka, &sa);
    if (mode)
        stage_layer(kb, &sb);
    else
        sb = sa;
    voices = atmos_voices(ka) + (mode ? atmos_voices(kb) : 0u);
    stage_wave_snap();                                  /* one snapshot for both bands */
    stage.col[0] = 0;                                   /* it moves every frame: the sounds card redraws when it is back */
    for (band = 0; band < 2u; band++) {
        cv_begin(ST_SND_W, ST_MON_BAND, T_BG);
        cv_oy = -(int32_t)(band * ST_MON_BAND);
        stage_monitor_card(&sa, &sb, mode, ka, kb, vu_lvl, vu_pk, voices);
        cv_oy = 0;
        if (stage.out && band == 0u)                    /* the knobs cover the top rows: below them only */
            cv_blit_from(4, ST_COL_Y, CARD_H);
        else
            cv_blit(4, (uint32_t)(ST_COL_Y + band * ST_MON_BAND));
    }
}
static void stage_sounds(void)
{
    uint32_t mode = atmos.mode % ATMOS_NMODE, ka = song.sel % NTRK, kb = atmos_b_track(ka, NTRK), band, sig;
    stage_snd_t sa, sb;
    stage_layer(ka, &sa);
    if (mode)
        stage_layer(kb, &sb);
    else
        sb = sa;
    sig = str_hash(str_hash(str_hash(str_hash(1u + mode * 3u + atmos.split_point * 7u + (uint32_t)stage.out * 31u +
                                              (uint32_t)sa.pend * 5u + (uint32_t)sb.pend * 53u, sa.nm), sa.en), sb.nm), sb.en) +
          sa.bank * 11u + sb.bank * 101u + sa.cat * 61u + sb.cat * 6151u + (uint32_t)sa.fav * 12289u + (uint32_t)sb.fav * 24593u +
          ka * 1009u + ux.gen * 977u + ux.pal * 31u;
    sig |= 1u;
    if (!ui.force && sig == stage.col[0])
        return;
    stage.col[0] = sig;
    for (band = 0; band < 2u; band++) {
        int32_t r0 = (int32_t)band * ST_SND_BAND;
        cv_begin(ST_SND_W, ST_SND_BAND, T_BG);
        cv_oy = -r0;                                    /* the band shows card rows r0 .. r0 + ST_SND_BAND */
        if (!mode) {
            stage_card_single(&sa, T_TRK(ka));
        } else if (mode == ATMOS_DUAL) {
            stage_card_half(0, &sa, T_TRK(ka), "LAYER A");
            stage_card_half(118, &sb, T_TRK(kb), "LAYER B");
        } else {
            char low[16], high[16], n[6];
            stage_note_name(n, atmos.split_point);
            str_cpy(low, "LOW  < ", sizeof low);
            str_cpy(low + str_len(low), n, 6);
            str_cpy(high, "HIGH  ", sizeof high);
            str_cpy(high + str_len(high), n, 6);
            str_cpy(high + str_len(high), "+", 2);
            stage_card_half(0, &sb, T_TRK(kb), low);    /* below the split point: layer B */
            stage_card_half(118, &sa, T_TRK(ka), high);
        }
        cv_oy = 0;
        if (stage.out && band == 0u)                    /* the knobs cover the top rows: below them only */
            cv_blit_from(4, ST_COL_Y, CARD_H);
        else
            cv_blit(4, (uint32_t)(ST_COL_Y + r0));
    }
}

/* every frame the knobs; every other frame (the scope's) the sounds, then the panel last */
static void stage_draw(void)
{
    stage_panel_hits();
    stage_knobs();
    if (!ui.force && (ui.frame & 1u))
        return;
    if (atmos_met.view == 1u) {                         /* PLAY: the monitor takes the whole Stage */
        stage_monitor();
        return;
    }
    stage_sounds();
    stage_panel();
}
