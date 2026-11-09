#include "ui_test_lib.c"
static double thd(const int32_t *y, uint32_t N, uint32_t k0)
{
    double f1 = 0, h = 0; uint32_t k, n;
    for (k = 1; k <= 8; k++) {
        double re = 0, im = 0;
        for (n = 0; n < N; n++) { double a = 2 * 3.14159265358979 * (double)(k0 * k) * n / N; re += y[n] * cos(a); im += y[n] * sin(a); }
        double m = sqrt(re * re + im * im);
        if (k == 1) f1 = m; else h += m * m;
    }
    return f1 > 0 ? 100.0 * sqrt(h) / f1 : 0;
}
int main(void)
{
    static const char *NM[] = {"OFF", "PHASER", "WIDTH", "FORMANT", "TAPE", "SPIT", "ECHO", "ROOM", "REVERB", "CHORUS"};
    static const int32_t AMP[] = {12000, 30000, 45000, 60000};
    uint32_t ty, ai;
    ui_power_on(); frames(100);
    printf("THD %% of a 1 kHz sine through each track effect (mix 64), by input peak:\n%-9s", "effect");
    for (ai = 0; ai < 4; ai++) printf(" %7d", (int)AMP[ai]);
    printf("\n");
    for (ty = 1; ty <= 9; ty++) {
        printf("%-9s", NM[ty]);
        for (ai = 0; ai < 4; ai++) {
            static int32_t y[2048]; int32_t b[CTL]; uint32_t blk, i, n = 0; track_t *t = TSEL;
            const uint32_t N = 2048, K0 = 47;                       /* 47 cycles in 2048 samples = 1012 Hz at 44.1 kHz */
            t->p[P_FXTYPE] = (int16_t)ty; t->p[P_FXMIX] = 64; t->p[P_FXRATE] = 64; t->p[P_FXCHAR] = 64;
            t->p[P_FXTYPE + FX_PARAMS] = 0; t->p[P_FXTYPE + 2 * FX_PARAMS] = 0;
            for (blk = 0; blk < 600; blk++) {                       /* settle, then record the last N samples */
                for (i = 0; i < CTL; i++) b[i] = (int32_t)(AMP[ai] * sin(2 * 3.14159265358979 * K0 * (blk * CTL + i) / (double)N));
                fx_process(t, b, CTL);
                if (blk >= 600 - N / CTL) for (i = 0; i < CTL; i++) y[n++] = fx_left[i];
            }
            printf(" %6.2f%%", thd(y, N, K0));
        }
        printf("\n");
    }
    return 0;
}
