#include "ui_test_lib.c"
int main(void)
{
    uint32_t f, i, last[4] = {0, 0, 0, 0}, changes = 0, seen_multi = 0;
    ui_power_on(); frames(200);
    song.master_q12 = 2048;
    engine_load_of(TSEL, ENGI_FM6, 0);
    for (f = 0; f < 8820u; f += CTL) { int32_t o[2 * CTL]; mix_block(o, CTL); }
    press(B_ARP);
    printf("arp mode now: %d  rate=%d oct=%d gate=%d  bpm=%d\n", (int)TSEL->p[P_AMODE], (int)TSEL->p[P_ARATE], (int)TSEL->p[P_AOCT], (int)TSEL->p[P_AGATE], (int)song.g[G_BPM]);
    key_down(0); key_down(4); key_down(7);              /* a chord held */
    for (f = 0; f < 88200u; f += CTL) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        if (f % 441u < CTL) {                            /* every 10 ms: the notes that sound with their gate open */
            uint32_t m[4] = {0, 0, 0, 0}, diff = 0, nn = 0;
            for (i = 0; i < NVOICE; i++)
                if (TSEL->v[i].active && TSEL->v[i].gate) { m[TSEL->v[i].note >> 5] |= 1u << (TSEL->v[i].note & 31u); nn++; }
            for (i = 0; i < 4; i++) if (m[i] != last[i]) diff = 1;
            if (diff) changes++;
            if (nn > 1) seen_multi++;
            for (i = 0; i < 4; i++) last[i] = m[i];
        }
    }
    printf("2 s with a 3-key chord held and ARP on: the set of gated notes changed %u times (a stepping arpeggio: one note at a time), samples with more than one gated note: %u of 200\n", changes, seen_multi);
    return 0;
}
