/* ATMOS: a scripted walk through the buttons on the real UI code; each step writes the screen as a PPM (host_screen). */
#include "ui_test_lib.c"
static FILE *capf;
static uint32_t nsnap;
static void snap(const char *caption)
{
    char path[128];
    uint32_t i;
    FILE *f;
    snprintf(path, sizeof path, "/tmp/story/%02u.ppm", (unsigned)nsnap);
    f = fopen(path, "wb");
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = (uint16_t)((host_screen[i] >> 8) | (host_screen[i] << 8));
        uint8_t b[3] = {(uint8_t)((c >> 11) * 255u / 31u), (uint8_t)(((c >> 5) & 63u) * 255u / 63u), (uint8_t)((c & 31u) * 255u / 31u)};
        fwrite(b, 1, 3, f);
    }
    fclose(f);
    fprintf(capf, "%s\n", caption);
    nsnap++;
}
static void audio(uint32_t ms)                    /* the audio blocks: the scope and the VU fill up */
{
    uint32_t f, i;
    for (f = 0; f < (uint32_t)((uint64_t)ms * FS / 1000u); f += CTL) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)                            /* what audio.c does after the mix: the scope */
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)o[2u * i];
        if ((f / CTL) % 16u == 0u) { frame(); }
        (void)i;
    }
}
int main(void)
{
    capf = fopen("/tmp/story/captions.txt", "w");
    ui_power_on();
    frames(400);                                             snap("Al encender: banco A, sonido 1 (DX7 BRASS 1). Modo SINGLE.");
    press(B_ARP); frames(500);                               snap("ARP (toque): banco B, Prophet-5.");
    press(B_ARP); frames(500);                               snap("ARP (toque): banco C, Juno-106.");
    press(B_ARP); frames(500);                               snap("ARP (toque): banco D, CZ / SID.");
    press(B_ARP); frames(500);                               snap("ARP (toque): vuelve al banco A, al sonido que dejaste.");
    turn(EN_PRESET, 1); frames(1200);                        snap("Encoder PRESETS: sonido siguiente (BRASS 2).");
    press(B_SEQ); frames(500);                               snap("SEQ (toque): modo DUAL, los dos sonidos a la vez.");
    press(B_SEQ); frames(500);                               snap("SEQ (toque): modo SPLIT, grave capa B / agudo capa A.");
    btn_down(B_SEQ); frame(); key_down(5); frame(); key_up(5); frame(); btn_up(B_SEQ); frames(300);
                                                             snap("SEQ mantenido + una tecla: ese es el punto de split.");
    btn_down(B_SEQ); frame(); turn(EN_PRESET, 1); frame(); turn(EN_PRESET, 1); frame(); btn_up(B_SEQ); frames(300);
                                                             snap("SEQ mantenido + PRESETS: cambia el sonido de la capa B.");
    press(B_SEQ); frames(500);                               snap("SEQ (toque): otra vez SINGLE.");
    hold(B_REC); frames(500);                                snap("REC (largo): el sonido pasa a favoritos (estrella).");
    press(B_REC); frames(600);                               snap("REC (toque): vista de la lista de favoritos.");
    press(B_REC); frames(600);                               snap("REC (toque): sale de los favoritos.");
    hold(B_ARP); frames(600);                                snap("ARP (largo): lista de sonidos.");
    press(B_HOME); frames(300); go_home(); frames(300);   /* back to the Stage (the list page is closed) */
    song.master_q12 = 3000;                                  /* (the test bench starts with the master down) */
    audio(300);                                              /* (the switched engines settle with the audio running) */
    key_down(7); audio(500);
    press(B_PLAY); frames(100); audio(300); frames(50);      snap("PLAY (toque): monitor con la onda, el vumetro, el banco y la estrella.");
    hold(B_PLAY); frames(300);                               snap("PLAY (largo): PANIC, todas las notas apagadas.");
    press(B_PLAY); frames(300);                              /* back to the sounds */
    press(B_SEQ); press(B_SEQ); frames(400);                 snap("SEQ dos toques: modo SPLIT (aparece el punto de split).");
    turn(EN_ALGO, 1); turn(EN_ALGO, 1); turn(EN_ALGO, 1); frames(400);
                                                             snap("Encoder ALGORITHM: mueve el punto de split de semitono en semitono.");
    press(B_SEQ); frames(300);                               snap("SEQ (toque): otra vez SINGLE.");
    key_up(7);
    fclose(capf);
    printf("storyboard: %u pantallas\n", (unsigned)nsnap);
    return 0;
}
