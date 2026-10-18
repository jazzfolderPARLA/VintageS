/* ATMOS: renders one JUNO-106 patch through the real engine path (host_preset -> input_on -> mix_block) and writes the
 * left channel as raw int32. usage: juno_test patch note hold_frames total_frames out.raw */
#define main hostsim_main
#include "hostsim.c"
#undef main
int main(int argc, char **argv)
{
    uint32_t k = argc > 1 ? (uint32_t)atoi(argv[1]) : 0u, note = 60u, notes[8], nn = 0;
    uint32_t hold = argc > 3 ? (uint32_t)atoi(argv[3]) : FS, total = argc > 4 ? (uint32_t)atoi(argv[4]) : 2u * FS, f, i;
    const char *path = argc > 5 ? argv[5] : "/tmp/juno.raw";
    FILE *w = fopen(path, "wb");
    track_t *t = &trk[0];
    uint32_t active_after = 0;
    if (!w)
        return 1;
    {   const char *q = argc > 2 ? argv[2] : "60";   /* notes: 60 or 48,55,60,64 */
        while (*q && nn < 8u) { notes[nn++] = (uint32_t)atoi(q); while (*q && *q != ',') q++; if (*q == ',') q++; }
        note = notes[0]; }
    host_tracks_init();
    for (i = 1; i < NPART; i++)
        trk[i].p[P_LEVEL] = 0;
    uint32_t eng_id = argc > 6 ? (uint32_t)atoi(argv[6]) : ENGI_JUNO; host_preset(t, eng_id, k);
    fprintf(stderr, "FS=%u CTL=%u engine=%s patch=%s\n", (unsigned)FS, (unsigned)CTL, ENGINES[t->engine]->name,
            eng_preset(ENGINES[eng_id], k)->name);
    for (i = 0; i < nn; i++)
        input_on(t, notes[i], 100);
    for (f = 0; f < total; f += CTL) {
        int32_t o[2 * CTL];
        if (f == hold)
            for (i = 0; i < nn; i++)
                input_off(t, notes[i]);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            fwrite(&o[2 * i], 4, 1, w);
    }
    for (i = 0; i < NVOICE; i++)
        active_after += t->v[i].active;
    fprintf(stderr, "voices still active at the end: %u\n", (unsigned)active_after);
    fclose(w);
    return 0;
}
