/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Melodee menu (HOME held): COLOR, HOLD (the layer threshold), LIGHTS (the keys' and buttons'
 * glow: ui_input.c ui_leds), KNOB ACCEL (ui_input.c accel_by), USB AUDIO (IN+OUT, IN, OUT or OFF; usb.c ua_off_set),
 * USB INPUT (stereo master or four track stems; usb.c ua_tracks_set),
 * AUTOSAVE (automatic project saves after five seconds stopped and untouched),
 * CALIBRATION (the setup screen: HARDWARE CALIBRATION). The ABOUT page (QR code, credits) was removed. ui.menu: 1 list. */
/* ------------------------------------------------------------ menu --- */
#define MENU_HEAD 24                     /* the menu's own title strip (Stage and the pages: H_HEAD) */
enum { MI_COLOR, MI_HOLD, MI_LIGHTS,
#if MELODEE_USB_AUDIO
       MI_USB, MI_USB_INPUT,
#endif
       MI_ACCEL, MI_SCREEN, MI_PANEL, MI_DESIGNER, MI_BACK,
       MI_COUNT };
static const char *const MI_NAME[MI_COUNT] = {"COLOR", "HOLD", "LIGHTS",
#if MELODEE_USB_AUDIO
                                              "USB AUDIO", "USB INPUT",
#endif
                                              "KNOB ACCEL", "SCREEN OFF", "CALIBRATION", "SOUND DESIGNER", "BACK"};
#if MELODEE_USB_AUDIO
/* USB AUDIO's four settings, as ua_off_want (UA_OFF_OUT | UA_OFF_IN) */
static const char *const MI_USB_NAME[4] = {"IN+OUT", "IN", "OUT", "OFF"};
#endif

/* the menu's header (0..24): its title, the REC mark, the way back */
static void menu_head(void)
{
    cv_begin(240, MENU_HEAD, T_BG);
    cv_text(8, 2, &AF_M, "System", T_TEXT);
    draw_rec_mark(178, T_BG);
    cv_blit(0, Y_HEAD);
}

/* the list (docs/design: mock/r5_pages, ref/settings): nine rows, 20 px, 22 apart from y 28: the name (S, secondary)
 * and its value (S, mid) at the right in words ("Night", "0.4 s"); the selected row lifted across the screen with a
 * bar in the track's colour, its name in text and its value in the track's colour; COLOR: the palettes as swatches,
 * the one chosen outlined. Under the rows where they are in the list. Drawn in two bands (the canvas holds 124
 * rows), split between two rows */
#define MENU_VISIBLE 9u
#define MENU_Y0 28
#define MENU_ROW 22
#define MENU_RH 20
#define MENU_SPLIT 137
/* "AUDIO CLICK" -> "Audio click", "P5 OSC A" -> "Osc A" (acronyms and a letter alone stay capitals) */
static void menu_words(char *d, const char *s, uint32_t n)
{
    static const char *const AC[] = {"USB", "FX", "MIDI", "LFO", "EG", "FM", "OP", "PW", "DCW", "DCA", "DCO", "MPC"};
    uint32_t i, w = 0, a;
    str_cpy(d, s, n);
    for (i = 0; d[i]; i++) {
        int first = !i, ac = 0;
        if (d[i] == ' ' || d[i] == '+' || d[i] == '-' || d[i] == '/') { w = i + 1u; continue; }
        if (i == w) {                                   /* a word: an acronym, a letter alone stay */
            for (a = 0; a < NELEM(AC) && !ac; a++) {
                uint32_t l = str_len(AC[a]);
                ac = !memcmp(d + w, AC[a], l) && (!d[w + l] || d[w + l] == ' ' || d[w + l] == '/' || (d[w + l] >= '0' &&
                     d[w + l] <= '9'));
            }
            ac |= !d[i + 1] || d[i + 1] == ' ';
        }
        if (ac) { while (d[i + 1] && d[i + 1] != ' ' && d[i + 1] != '/') i++; continue; }
        if (!first && d[i] >= 'A' && d[i] <= 'Z') d[i] = (char)(d[i] + 32);
    }
}
static void draw_menu(void)
{
    uint32_t i, pass, sig = ui.menu * 7u + ui.menu_sel * 131u + settings.palette * 1009u + settings.lowcut * 7919u +
                            settings_hold * 3511u + settings_lights * 6151u + settings_autosave * 15485863u +
#if MELODEE_USB_AUDIO
                            ua_off_want * 92821u + ua_tracks_want * 324161u +
#endif
                            song.rec * 65537u + song.sel * 13u;
    if (!ui.force && sig == ui.menu_sig)
        return;
    ui.menu_sig = sig;
    menu_head();
    for (pass = 0; pass < 2u; pass++) {
        int32_t top = pass ? MENU_SPLIT : MENU_HEAD;
        cv_begin(240, (uint32_t)(pass ? 240 - MENU_SPLIT : MENU_SPLIT - MENU_HEAD), T_BG);
        cv_oy = -top;                                 /* drawn in screen rows */
        uint32_t first=ui.menu_sel>=MENU_VISIBLE?ui.menu_sel-MENU_VISIBLE+1u:0u;
        for (i = first; i < first+MENU_VISIBLE && i<MI_COUNT; i++) {
            int32_t y = MENU_Y0 + (int32_t)(i-first) * MENU_ROW, yt = y + 3;
            int sel = i == ui.menu_sel;
            uint16_t bg = sel ? T_LIFT : T_BG, val = sel ? T_THEME : T_MID;
            const char *value = 0;
            char b[24], w[24];
            if (y + MENU_RH <= top || y >= top + (int32_t)cv_h)
                continue;
            if (sel) {
                cv_rect(0, y, 240, MENU_RH, T_LIFT);
                cv_rect(0, y, 3, MENU_RH, T_THEME);
            }
            menu_words(w, MI_NAME[i], sizeof w);
            cv_text_on(12, yt, &AF_S, w, sel ? T_TEXT : T_SEC, bg);
            if (i == MI_ACCEL) value = PREF_BITS & PREF_ACCEL_OFF ? "OFF" : "ON";
            if (i == MI_SCREEN) value = (const char *const[]){"NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"}[scr_get()];
            if (i == MI_LIGHTS) value = LIGHTS_NAME[settings_lights % LIGHTS_N];
#if MELODEE_USB_AUDIO
            if (i == MI_USB_INPUT) value = ua_tracks_want ? "4 TRACKS" : "STEREO";
            if (i == MI_USB) value = (const char *const[]){"In + Out", "In", "Out", "Off"}[ua_off_want & 3u];
#endif
            if (i == MI_HOLD) {                         /* "0.4 s" */
                str_cpy(b, "0.4 s", sizeof b);
                b[2] = (char)('0' + HOLD_MS[settings_hold % 4u] / 100u);
                value = b;
            }
            if (i == MI_COLOR) {                        /* the palettes as swatches, the one chosen outlined */
                uint32_t k;
                int32_t x;
                menu_words(w, UI_PALETTES[settings.palette % NPALETTES].name, sizeof w);
                x = cv_text_r(228, yt, &AF_S, w, val, bg) - 10 - 12 * (int32_t)NPALETTES;   /* (cv_text_r: its left) */
                for (k = 0; k < NPALETTES; k++) {
                    int on = k == settings.palette % NPALETTES;
                    cv_rrect(x + 12 * (int32_t)k, y + 5, 10, 10, 2, on ? T_THEME : T_MID, bg);
                    cv_rrect(x + 12 * (int32_t)k + 1, y + 6, 8, 8, 2,   /* (GRAY, the tests': greys) */
                             ux.mono ? ux_mix(T_BG, T_TEXT, 40 * k) : UI_PALETTES[k].bg, on ? T_THEME : T_MID);
                }
            } else if (value) {
                menu_words(w, value, sizeof w);
                cv_text_r(228, yt, &AF_S, w, val, bg);
            }
        }
        {   /* where the rows are in the list */
            int32_t tw = 232 * (int32_t)MENU_VISIBLE / (int32_t)MI_COUNT;
            int32_t tx = MI_COUNT > MENU_VISIBLE ? (232 - tw) * (int32_t)first / (int32_t)(MI_COUNT - MENU_VISIBLE) : 0;
            cv_rrect(4, 228, 232, 2, 1, T_LINE, T_BG);
            cv_rrect(4 + tx, 228, tw, 2, 1, T_THEME, T_LINE);
        }
        cv_oy = 0;
        cv_blit(0, (uint32_t)top);
    }
}
static void enc_drop(void)                             /* knob turns nobody takes */
{
    uint32_t k;
    for (k = 0; k < NE; k++)
        panel_enc(k);
}

static void menu_close(void)
{
    settings_save();                                   /* palette / panel table, if changed */
    ui.menu = 0;
    ui.force = 1;
    go_home();
}

/* menu: PRESETS moves, OCT+ confirms, OCT- cancels (ABOUT -> list -> close) */
static void menu_input(uint32_t oct)                  /* oct: ui_input.c oct_taps */
{
    int32_t s;
    uint32_t ok = (oct >> 1) & 1u, back = oct & 1u;
    if (back) {
        menu_close();
        return;
    }
    if ((s = panel_enc(EN_K2)) != 0 && ui.menu == 1)  /* KNOB 2 the row (the standard; PRESETS too) */
        ui.menu_sel = (uint8_t)clamp((int32_t)ui.menu_sel + s, 0, (int32_t)MI_COUNT - 1);
    if ((s = panel_enc(EN_PRESET)) != 0 && ui.menu == 1)
        ui.menu_sel = (uint8_t)((ui.menu_sel + (s > 0 ? 1u : MI_COUNT - 1u)) % MI_COUNT);
    s = panel_enc(EN_K1);
    if (s != 0 && ui.menu == 1 && ui.menu_sel == MI_COLOR) {
        settings.palette = (settings.palette + (s > 0 ? 1u : NPALETTES - 1u)) % NPALETTES;   /* (wraps) */
        palette_set(settings.palette);              /* (the menu signature redraws) */
        ui.force = 1;
    }

#if MELODEE_USB_AUDIO
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_USB_INPUT) {
        ua_tracks_set(s > 0 ? 1 : s < 0 ? 0 : !ua_tracks_want, fm1_ms);
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_USB) {
        /* KNOB 1 steps IN+OUT IN OUT OFF, OCT+ steps and wraps; the host gets it once it rests (main.c) */
        uint32_t v = ua_off_want & 3u;
        v = s > 0 ? (v < 3u ? v + 1u : 3u) : s < 0 ? (v ? v - 1u : 0u) : (v + 1u) % 4u;
        ua_off_set(UA_OFF_OUT, !(v & UA_OFF_OUT), fm1_ms);
        ua_off_set(UA_OFF_IN, !(v & UA_OFF_IN), fm1_ms);
        ok = 0;
    }
#endif
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_LIGHTS) {
        /* KNOB 1 steps OFF .. FULL, OCT+ steps and wraps (saved when the menu closes) */
        uint32_t v = settings_lights % LIGHTS_N;
        settings_lights = (uint8_t)(s > 0 ? (v + 1u < LIGHTS_N ? v + 1u : v) : s < 0 ? (v ? v - 1u : 0u) : (v + 1u) % LIGHTS_N);
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_HOLD) {
        /* KNOB 1 steps 0.3 .. 0.6 s, OCT+ steps and wraps */
        uint32_t v = settings_hold % 4u;
        settings_hold = (uint8_t)(s > 0 ? (v < 3u ? v + 1u : 3u) : s < 0 ? (v ? v - 1u : 0u) : (v + 1u) % 4u);
        ok = 0;
    }
    if ((s || ok) && ui.menu==1 && ui.menu_sel==MI_ACCEL) {   /* KNOB 1 right ON, left OFF; OCT+ toggles */
        uint32_t on = s > 0 ? 1u : s < 0 ? 0u : !!(PREF_BITS & PREF_ACCEL_OFF);
        PREF_BITS = (uint8_t)(on ? PREF_BITS & ~PREF_ACCEL_OFF : PREF_BITS | PREF_ACCEL_OFF);
        ui.force=1; ok=0;
    }
    if ((s || ok) && ui.menu==1 && ui.menu_sel==MI_SCREEN) {
        uint32_t value=scr_get(), top=4u;
        value=s>0?(value<top?value+1u:value):s<0?(value?value-1u:0u):(value+1u)%(top+1u);
        scr_put(value);
        ui.force=1; ok=0;
    }
    if (ok && ui.menu == 1) {
        switch (ui.menu_sel) {
        case MI_COLOR:                                 /* OCT+ steps through the palettes too */
            settings.palette = (settings.palette + 1u) % NPALETTES;
            palette_set(settings.palette);
            ui.force = 1;
            break;
        case MI_PANEL:
            panel_setup();
            ui.force = 1;
            break;
        case MI_DESIGNER:
            menu_close();designer_open();
            break;
        default:
            menu_close();
            break;
        }
    }
    enc_drop();                                        /* swallow the rest while the menu is up */
}
