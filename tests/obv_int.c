#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static uint32_t obv_active(track_t *t) { uint32_t n = 0, i; for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].stage != 4; return n; }
static void level(track_t *t, uint32_t e, uint32_t k, const char *what, long *rms, int32_t *pk)
{
    int32_t out[CTL]; uint32_t b, i; double acc = 0; long cnt = 0; int32_t peak = 0;
    ui_power_on(); t = &trk[0];
    host_preset(t, e, 0); if (k) apply_preset_to(t, k);
    trk_note_on(t, 60, 100);
    for (b = 0; b < 200; b++) { track_render(t, out, CTL); for (i = 0; i < CTL; i++) { if (b > 20) { acc += (double)out[i] * out[i]; cnt++; } if (abs(out[i]) > peak) peak = abs(out[i]); } }
    *rms = (long)__builtin_sqrt(acc / (cnt ? cnt : 1)); *pk = peak;
    (void)what;
}
static int test_obv_int(void)
{
    int bad = 0; long rms; int32_t pk, out[CTL]; uint32_t b, k, i, quiet = 0, loud = 0;
    track_t *t;
    long r_obv = 0, r_juno = 0, r_p5 = 0, r_fm = 0;
    level(0, ENGI_JUNO, 0, "juno", &r_juno, &pk);
    level(0, ENGI_PROPHET, 1, "p5", &r_p5, &pk);
    level(0, ENGI_FM6, 0, "fm6", &r_fm, &pk);
    printf("level (rms, 1.0 = 65536): JUNO init %ld, PROPHET 1 %ld, FM6 init %ld\n", r_juno, r_p5, r_fm);
    for (k = 0; k <= OBV_NFACT; k++) {
        level(0, ENGI_OBV, k, "obv", &rms, &pk);
        if (k == 0 || k == 1 || k == 10 || k == 41) printf("OBV preset %2u: rms %ld peak %d\n", k, rms, pk);
        if (rms < 40) quiet++;
        if (pk > 70000) loud++;
        r_obv += rms;
    }
    printf("OBV mean rms over the 65 sounds: %ld; sounds with rms<40: %u, with peak>70000: %u\n", r_obv / 65, quiet, loud);
    bad += check("OBV: no preset above full scale", loud == 0);
    ui_power_on(); t = &trk[0];
    host_preset(t, ENGI_OBV, 0);
    bad += check("OBV: engine selected", t->eng_req == ENGI_OBV);
    trk_note_on(t, 60, 100);
    for (b = 0; b < 40; b++) track_render(t, out, CTL);
    bad += check("OBV: one note = one voice", obv_active(t) == 1);
    trk_note_off(t, 60);
    for (b = 0; b < 30000 && obv_active(t); b++) track_render(t, out, CTL);
    bad += check("OBV: the voice is freed after the release", obv_active(t) == 0);
    for (k = 0; k < 8; k++) trk_note_on(t, 48 + 4 * k, 100 - k);
    for (b = 0; b < 40; b++) track_render(t, out, CTL);
    bad += check("OBV: eight notes = eight voices within the budget", obv_active(t) == 8 && voices_busy() <= VBUDGET);
    trk_note_on(t, 90, 100);
    for (b = 0; b < 10; b++) track_render(t, out, CTL);
    bad += check("OBV: a ninth note steals, never above eight", obv_active(t) <= 8);
    panic_req |= 1u; events_block(CTL);
    return bad;
}
int main(void) { int bad = test_obv_int(); printf("obv-int: %d FAILED\n", bad); return bad != 0; }
