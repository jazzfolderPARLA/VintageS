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
    press(B_ARP);  bad += check("ARP tap: A (FM6) -> B (PROPHET)", TSEL->eng_req == ENGI_PROPHET);
    press(B_ARP);  bad += check("ARP tap: B -> C (JUNO)", TSEL->eng_req == ENGI_JUNO);
    press(B_ARP);  bad += check("ARP tap: C -> D (CZ)", TSEL->eng_req == ENGI_CZ);
    press(B_ARP);  bad += check("ARP tap: D -> A, on the sound it left (6th)", TSEL->eng_req == ENGI_FM6 && TSEL->preset == 5);
    hold(B_ARP);   bad += check("ARP held: the PRESETS list", cur_page()->graph == GR_BROWSE && !ui.home);
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
    press(B_ARP);
    bad += check("SEQ + ARP: layer B's bank (A -> B, PROPHET)", trk[kb].eng_req == ENGI_PROPHET);
    btn_up(B_SEQ); frames(100);
    bad += check("no SEQ tap on release (the mode stays SINGLE)", atmos.mode == ATMOS_SINGLE);

    /* the split point from a key */
    btn_down(B_SEQ); frame();
    key_down(5); frame(); key_up(5); frame();
    k = kb_map(TSEL, 5);
    btn_up(B_SEQ); frames(100);
    bad += check("SEQ + a key: SPLIT at that note", atmos.mode == ATMOS_SPLIT && atmos.split_point == k);

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

    /* PLAY: the view (tap), the metronome (held), its tempo on ALGORITHM, SELECT untouched */
    go_home();
    atmos_met.view = 0;
    press(B_PLAY);  bad += check("PLAY tap: the monitor", atmos_met.view == 1 && ui.home);
    press(B_PLAY);  bad += check("PLAY tap: back to the sounds", atmos_met.view == 0);
    press(B_PLAY);  press(B_PLAY);
    bad += check("PLAY taps: two views only (monitor, sounds)", atmos_met.view == 0);
    bad += check("VU: silence is level 0, full scale 20, -6 dB 18, -30 dB 10",
                 atmos_vu_level(0) == 0 && atmos_vu_level(32767) == 20 && atmos_vu_level(16422) == 18 && atmos_vu_level(1036) == 10);
    bad += check("the metronome starts off, at 120", !atmos_met.on && atmos_met.bpm == 120);
    hold(B_PLAY);   bad += check("PLAY held: the metronome on", atmos_met.on == 1);
    turn(EN_ALGO, 1);  turn(EN_ALGO, 1);
    bad += check("ALGORITHM: the tempo (120 -> 122)", atmos_met.bpm == 122);
    turn(EN_ALGO, -1);
    bad += check("ALGORITHM the other way (121)", atmos_met.bpm == 121);
    {   uint16_t b0 = atmos_met.bpm;
        turn(EN_SELECT, 1);
        bad += check("SELECT does not touch the tempo", atmos_met.bpm == b0);
    }
    for (k = 0; k < 400; k++) turn(EN_ALGO, 1);
    bad += check("the tempo stops at 250", atmos_met.bpm == 250);
    for (k = 0; k < 400; k++) turn(EN_ALGO, -1);
    bad += check("... and at 30", atmos_met.bpm == 30);
    atmos_met.bpm = 120;
    press(B_PLAY);  bad += check("PLAY tap with the metronome on: it stops (no view change)", !atmos_met.on && atmos_met.view == 0);
    turn(EN_ALGO, 1);
    bad += check("ALGORITHM with the metronome off: nothing", atmos_met.bpm == 120);

    /* the click, in the audio: one every 22050 samples at 120 bpm, the first one stronger, nothing when off */
    {   uint32_t f, i, n = 0, first[8], quiet = 2000;
        int32_t peak_bar = 0, peak_beat = 0, silent_peak = 0;
        atmos_met.on = 0;
        for (f = 0; f < 4096; f += CTL) {
            int32_t o[2 * CTL];
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++) { int32_t a = o[2 * i] < 0 ? -o[2 * i] : o[2 * i]; if (a > silent_peak) silent_peak = a; }
        }
        bad += check("metronome off: no click in the audio", silent_peak == 0);
        atmos_met.pos = 0; atmos_met.beat = 0; atmos_met.env = 0; atmos_met.bpm = 120; atmos_met.on = 1;
        for (f = 0; f < 5u * 22050u && n < 8u; f += CTL) {
            int32_t o[2 * CTL];
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++) {
                int32_t a = o[2 * i] < 0 ? -o[2 * i] : o[2 * i];
                if (a > 1500 && quiet > 1000u) {
                    first[n++] = f + i;
                    if (n == 1u) peak_bar = a;
                    if (n == 2u) peak_beat = a;
                }
                quiet = a > 1500 ? 0u : quiet + 1u;
                if (n == 1u && a > peak_bar) peak_bar = a;
                if (n == 2u && a > peak_beat) peak_beat = a;
            }
        }
        bad += check("metronome on at 120: the clicks come 22050 samples apart", n >= 4u && first[1] - first[0] >= 22040u && first[1] - first[0] <= 22060u &&
                     first[2] - first[1] >= 22040u && first[2] - first[1] <= 22060u);
        bad += check("the bar's first click is stronger", peak_bar > peak_beat);
        atmos_met.on = 0;
    }
    printf("atmos_test: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
