#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int test_bank(void)
{
    int bad = 0, i;
    ui_power_on();
    host_preset(&trk[0], ENGI_FM6, 0);
    atmos.bank = 0;
    for (i = 0; i < 4; i++) press(B_SCL);
    bad += check("BANK E: four SCL taps from A reach OBERVAVE", trk[song.sel].eng_req == ENGI_OBV && atmos.bank == 4);
    bad += check("BANK E: it starts on its first factory sound", trk[song.sel].preset == 1);
    bad += check("BANK E: the bank name is shown", !memcmp(ui.msg, "BANK E  OBERVAVE", 16));
    press(B_SCL);
    bad += check("BANK E: one more tap wraps to A (DX7)", trk[song.sel].eng_req == ENGI_FM6 && atmos.bank == 0);
    /* DUAL then bank E: SINGLE */
    atmos.mode = ATMOS_DUAL;
    for (i = 0; i < 4; i++) press(B_SCL);
    bad += check("BANK E: entering it from DUAL puts the mode back to SINGLE", trk[song.sel].eng_req == ENGI_OBV && atmos.mode == ATMOS_SINGLE && !memcmp(ui.msg2, "SINGLE ONLY", 11));
    /* SEQ tap: no DUAL */
    press(B_SEQ);
    bad += check("BANK E: SEQ stays SINGLE with OBERVAVE", atmos.mode == ATMOS_SINGLE && msg_is("OBERVAVE: SINGLE ONLY"));
    /* SAVE */
    press(B_SAVE); press(B_SAVE);
    bad += check("BANK E: SAVE says it cannot save yet and writes nothing", msg_is("OBERVAVE: NO SAVE YET") && !up_used(0));
    /* layer B never takes E */
    {
        uint32_t kb = atmos_b_track(song.sel % NTRK, NTRK), n, hit = 0;
        for (n = 0; n < 12; n++) { atmos_b_bank_next(); if (trk[kb].eng_req == ENGI_OBV) hit = 1; }
        bad += check("BANK E: layer B's banks skip OBERVAVE", !hit);
    }
    /* loading it through the list on a DUAL session */
    atmos.mode = ATMOS_DUAL;
    apply_preset_to(&trk[song.sel], 3);
    bad += check("BANK E: any load of OBERVAVE (A) returns to SINGLE", atmos.mode == ATMOS_SINGLE || trk[song.sel].eng_req != ENGI_OBV);
    return bad;
}
int main(void) { int bad = test_bank(); printf("obv-bank: %d FAILED\n", bad); return bad != 0; }
