#include "ui_test_lib.c"
static void snap(const char *name)
{
    char path[96]; uint32_t i; FILE *f;
    snprintf(path, sizeof path, "/tmp/hdr_%s.ppm", name);
    f = fopen(path, "wb"); fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = (uint16_t)((host_screen[i] >> 8) | (host_screen[i] << 8));
        uint8_t b[3] = {(uint8_t)((c >> 11) * 255u / 31u), (uint8_t)(((c >> 5) & 63u) * 255u / 63u), (uint8_t)((c & 31u) * 255u / 31u)};
        fwrite(b, 1, 3, f);
    }
    fclose(f);
}
int main(void)
{
    ui_power_on(); frames(300);
    song.batt_raw = 570; usb.config = 0; frames(200); snap("bat57");
    song.batt_raw = 520; frames(200); snap("bat16");
    song.batt_raw = 512; frames(200); snap("bat7low");
    song.batt_raw = 590; usb.config = 1; usb.suspended = 0; frames(200); snap("charging");
    midi_rx_usb += 1; midi_rx_trs += 1; frames(32); snap("midi");
    return 0;
}
