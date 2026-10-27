/* ATMOS: the buttons and the layer / favourites logic, run on the real UI code (ui_test.c's helpers). */
#include "ui_test_lib.c"      /* ui_test.c with its main renamed (sed, see tests/atmos_test.sh) */
int main(void)
{
    int bad = 0;
    uint32_t kb, k;
    ui_power_on();
    atmos.mode = ATMOS_SINGLE;
    press(B_SEQ);  bad += check("SEQ tap: SINGLE -> DUAL", atmos.mode == ATMOS_DUAL);
    press(B_SEQ);  bad += check("SEQ tap: DUAL -> SPLIT", atmos.mode == ATMOS_SPLIT);
    press(B_SEQ);  bad += check("SEQ tap: SPLIT -> SINGLE", atmos.mode == ATMOS_SINGLE);

    /* ARP: the banks A B C D, each coming back to the sound it had */
    engine_load_of(TSEL, ENGI_FM6, 5);
    press(B_SCL);  bad += check("SCL tap: A (FM6) -> B (PROPHET)", TSEL->eng_req == ENGI_PROPHET);
    press(B_SCL);  bad += check("SCL tap: B -> C (JUNO)", TSEL->eng_req == ENGI_JUNO);
    press(B_SCL);  bad += check("SCL tap: C -> D (CZ)", TSEL->eng_req == ENGI_CZ);
    press(B_SCL);  bad += check("SCL tap: D -> A, on the sound it left (6th)", TSEL->eng_req == ENGI_FM6 && TSEL->preset == 5);
    hold(B_SCL);   bad += check("SCL held: the PRESETS list", cur_page()->graph == GR_BROWSE && !ui.home);
    go_home();

    /* REC: the favourites */
    engine_load_of(TSEL, ENGI_FM6, 7);
    bad += check("starts as no favourite", !preset_favorite());
    hold(B_REC);   bad += check("REC held: the sound is starred", preset_favorite());
    hold(B_REC);   bad += check("REC held again: unstarred", !preset_favorite());
    hold(B_REC);
    favorite_set(ENGI_FM6, 20, 1);
    favorite_set(ENGI_FM6, 33, 1);
    press(B_REC);  bad += check("REC tap: the favourites view (list FAV, the PRESETS page)", list_mode() == LM_FAV && cur_page()->graph == GR_BROWSE && !ui.home);
    turn(EN_PRESET, 1);
    frames(1200);
    bad += check("PRESETS in the view: the next favourite (7 -> 20)", TSEL->eng_req == ENGI_FM6 && TSEL->preset == 20);
    turn(EN_PRESET, 1);
    frames(1200);
    bad += check("PRESETS again: 20 -> 33", TSEL->preset == 33);
    turn(EN_PRESET, 1);
    frames(1200);
    bad += check("PRESETS again: wraps to the first favourite (a DX7 one: 7)", TSEL->preset == 7);
    press(B_REC);  bad += check("REC tap: out of the view, HOME and the whole list", list_mode() == LM_ALL && ui.home);

    /* layer B from the keys */
    kb = atmos_b_track(song.sel % NTRK, NTRK);
    engine_load_of(&trk[kb], ENGI_FM6, 0);
    atmos.mode = ATMOS_SINGLE;
    btn_down(B_SEQ); frame();
    turn(EN_PRESET, 1); frame();
    bad += check("SEQ + PRESETS: layer B's sound steps (0 -> 1), A untouched", trk[kb].eng_req == ENGI_FM6 && trk[kb].preset == 1 && TSEL->preset == 7);
    turn(EN_PRESET, -1); turn(EN_PRESET, -1); frame();
    bad += check("SEQ + PRESETS the other way: 0, then the INIT VOICE (the list wraps)", trk[kb].preset == 128);
    press(B_SCL);
    bad += check("SEQ + SCL: layer B's bank (A -> B, PROPHET)", trk[kb].eng_req == ENGI_PROPHET);
    btn_up(B_SEQ); frames(100);
    bad += check("no SEQ tap on release (the mode stays SINGLE)", atmos.mode == ATMOS_SINGLE);

    /* the split point from a key */
    btn_down(B_SEQ); frame();
    key_down(5); frame(); key_up(5); frame();
    k = kb_map(TSEL, 5);
    btn_up(B_SEQ); frames(100);
    bad += check("SEQ + a key: SPLIT at that note", atmos.mode == ATMOS_SPLIT && atmos.split_point == k);

    /* SCL has no page or layer any more; ARP is the arpeggiator */
    go_home();
    press(B_SCL); go_home();
    bad += check("SCL opens no scale page (it changes the bank)", ui.home);
    btn_down(B_SCL); frame(); key_down(3); frame(); key_up(3); frame(); btn_up(B_SCL); frames(200);
    bad += check("SCL held with a key: no scale layer (nothing about the root or the scale changes)", TSEL->p[P_SCALE] == 0 && TSEL->p[P_ROOT] == 0);
    engine_load_of(TSEL, ENGI_FM6, 2);
    bad += check("the arpeggiator starts off", TSEL->p[P_AMODE] == 0);
    press(B_ARP);   bad += check("ARP tap: the arpeggiator on (UP)", TSEL->p[P_AMODE] == 1);
    press(B_ARP);   bad += check("ARP tap: off", TSEL->p[P_AMODE] == 0);
    TSEL->p[P_AMODE] = 6;
    press(B_ARP);   press(B_ARP);
    bad += check("ARP comes back on the mode it had (REPEAT)", TSEL->p[P_AMODE] == 6);
    TSEL->p[P_AMODE] = 0;
    hold(B_ARP);    bad += check("ARP held: its page (mode, rate, octaves, gate)", cur_page()->graph == GR_ARP && !ui.home);
    go_home();
    atmos.mode = ATMOS_DUAL; TSEL->p[P_AMODE] = 0; trk[1].p[P_AMODE] = 0;
    press(B_ARP);   bad += check("ARP in DUAL: both layers get the same mode", TSEL->p[P_AMODE] != 0 && trk[1].p[P_AMODE] == TSEL->p[P_AMODE]);
    press(B_ARP);   atmos.mode = ATMOS_SINGLE;

    /* the tracks: layer A is track 1, B is track 2, nothing else can be selected */
    track_select(2);
    bad += check("track select: only track 1 can be selected", song.sel == 0);
    song.sel = 2; song.g[G_ROUTE] = 0;
    frames(100);
    bad += check("a track 3 selection / ROUT = per-channel are put back (track 1, ROUT = SEL)", song.sel == 0 && song.g[G_ROUTE] == 1);

    /* MIDI in, DUAL / SPLIT: channel ch is layer A, ch + 8 layer B */
    kb = atmos_b_track(song.sel % NTRK, NTRK);
    atmos.mode = ATMOS_SINGLE;
    midi_event(0x90, 0, 60, 100);
    bad += check("MIDI SINGLE: the note plays layer A only", (mn_get(0, 60) & 0x7F) == 1 && !mn_get(8, 60));
    midi_event(0x80, 0, 60, 0);
    bad += check("MIDI SINGLE: its release", !mn_get(0, 60));
    atmos.mode = ATMOS_DUAL;
    midi_event(0x90, 2, 64, 100);
    bad += check("MIDI DUAL: the note plays A and B", (mn_get(2, 64) & 0x7F) == 1 && (mn_get(10, 64) & 0x7F) == kb + 1);
    midi_event(0x80, 2, 64, 0);
    bad += check("MIDI DUAL: the release ends both", !mn_get(2, 64) && !mn_get(10, 64));
    atmos.mode = ATMOS_SPLIT; atmos.split_point = 60;
    midi_event(0x90, 0, 50, 100);
    midi_event(0x90, 0, 72, 100);
    bad += check("MIDI SPLIT: below the split point B only, above it A only",
                 !mn_get(0, 50) && (mn_get(8, 50) & 0x7F) == kb + 1 && (mn_get(0, 72) & 0x7F) == 1 && !mn_get(8, 72));
    midi_event(0x80, 0, 50, 0);
    midi_event(0x80, 0, 72, 0);
    bad += check("MIDI SPLIT: both released", !mn_get(0, 50) && !mn_get(8, 50) && !mn_get(0, 72) && !mn_get(8, 72));
    midi_event(0x90, 9, 40, 100);
    bad += check("MIDI SPLIT: channel 10 counts as channel 2 (note 40, below the point: its shadow, channel 10)", (mn_get(9, 40) & 0x7F) == kb + 1 && !mn_get(1, 40));
    midi_event(0x80, 9, 40, 0);
    bad += check("... and its release", !mn_get(9, 40));
    atmos.mode = ATMOS_SINGLE;
    midi_event(0xB0, 0, 64, 127); midi_event(0xB0, 0, 64, 0);

    /* PLAY and the real-time bytes do nothing */
    press(B_PLAY);
    bad += check("PLAY does nothing: no transport", !song.playing);
    {   uint32_t before = midi_rx_usb;
        midi_in_event(0xFu | 0xFAu << 8);                /* USB-MIDI: Start, Clock, Active Sensing */
        midi_in_event(0xFu | 0xF8u << 8);
        midi_in_event(0xFu | 0xFEu << 8);
        bad += check("MIDI Start / Clock / Active Sensing over USB: dropped (not counted, not queued)", midi_rx_usb == before);
        midi_in_event(0x9u | 0x90u << 8 | 60u << 16 | 100u << 24);
        bad += check("a USB note-on is counted (the header's MIDI dot)", midi_rx_usb == before + 1u);
    }

    /* PLAY: the view (tap) and PANIC (held); ALGORITHM moves the split point (SPLIT only); SELECT untouched */
    go_home();
    atmos_view = 0;
    press(B_PLAY);  bad += check("PLAY tap: the monitor", atmos_view == 1 && ui.home);
    press(B_PLAY);  bad += check("PLAY tap: back to the sounds", atmos_view == 0);
    press(B_PLAY);  press(B_PLAY);
    bad += check("PLAY taps: two views only (monitor, sounds)", atmos_view == 0);
    bad += check("VU: silence is level 0, full scale 20, -6 dB 18, -30 dB 10",
                 atmos_vu_level(0) == 0 && atmos_vu_level(32767) == 20 && atmos_vu_level(16422) == 18 && atmos_vu_level(1036) == 10);
    {   uint32_t f, q, n;
        atmos.mode = ATMOS_SINGLE;
        for (f = 0; f < 4000u; f += CTL) { int32_t o[2 * CTL]; mix_block(o, CTL); }
        midi_event(0x90, 0, 60, 100); midi_event(0x90, 0, 64, 100); midi_event(0xB0, 0, 64, 127);   /* two notes and the sustain pedal */
        for (f = 0; f < 4000u; f += CTL) { int32_t o[2 * CTL]; mix_block(o, CTL); }
        n = 0; for (q = 0; q < NVOICE; q++) n += trk[0].v[q].active;
        bad += check("two MIDI notes sound before PANIC", n >= 2u && mn_get(0, 60) && mn_get(0, 64));
        hold(B_PLAY);
        for (f = 0; f < 8000u; f += CTL) { int32_t o[2 * CTL]; mix_block(o, CTL); }
        bad += check("PLAY held: PANIC - no MIDI note left owned, nothing with its key down", !mn_get(0, 60) && !mn_get(0, 64) && gates() == 0);
        midi_event(0x80, 0, 60, 0); midi_event(0x80, 0, 64, 0); midi_event(0xB0, 0, 64, 0);
        n = 0; for (q = 0; q < 6000u; q += CTL) { int32_t o[2 * CTL]; mix_block(o, CTL); }
        midi_event(0x90, 0, 67, 100);
        for (f = 0; f < 4000u; f += CTL) { int32_t o[2 * CTL]; mix_block(o, CTL); }
        bad += check("after PANIC the instrument plays again", mn_get(0, 67) != 0 && gates() >= 1u);
        midi_event(0x80, 0, 67, 0);
    }
    atmos.mode = ATMOS_SINGLE; atmos.split_point = 60;
    turn(EN_ALGO, 1);
    bad += check("ALGORITHM does nothing outside SPLIT", atmos.split_point == 60);
    atmos.mode = ATMOS_SPLIT; atmos.split_point = 60;
    turn(EN_ALGO, 1); turn(EN_ALGO, 1); turn(EN_ALGO, 1);
    bad += check("SPLIT: ALGORITHM moves the split point up a semitone a click (60 -> 63)", atmos.split_point == 63);
    turn(EN_ALGO, -1);
    bad += check("... and down (62)", atmos.split_point == 62);
    for (k = 0; k < 300; k++) turn(EN_ALGO, 1);
    bad += check("the split point stops at 127", atmos.split_point == 127);
    for (k = 0; k < 300; k++) turn(EN_ALGO, -1);
    bad += check("... and at 0", atmos.split_point == 0);
    atmos.split_point = 60;
    {   uint32_t sel_before = atmos.split_point;
        turn(EN_SELECT, 1);
        bad += check("SELECT does not touch the split point", atmos.split_point == sel_before);
    }
    printf("atmos_test: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
