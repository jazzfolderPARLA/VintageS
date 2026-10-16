#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Convierte los 4 cartuchos DX7 (.syx de 32 voces) en src/dx7_rom.h.

Uso:  python3 tools/gen_dx7_rom.py rom1a.syx rom1b.syx rom2a.syx rom2b.syx src/dx7_rom.h

Cada .syx debe medir 4104 bytes: F0 43 0n 09 20 00 + 32 x 128 bytes + checksum + F7.
El script rechaza cualquier archivo que no cumpla (tamano, cabecera, checksum, bytes de 7 bits).
Salida: DX7_ROM[128][128], los registros empaquetados tal cual (el mismo formato que
FM6_PACKED en eng_fm6.c, asi que se cargan con fm6_unpack() sin conversion), mas
DX7_ROM_LIST(X) con el nombre y el flag mono de cada voz (la tabla de presets de FM6) y
DX7_ROM_CATS, una letra de categoria por voz (el navegador de presets de browse.c)."""
import re
import sys

# letra de categoria de browse.c (CAT_LETTER "?BLPUKDFOGSRWE"), por palabras del nombre; el orden importa
RULES = [
    ('B', ('BASS',)),
    ('E', ('BELL', 'MARIMBA', 'XYLO', 'VIBE', 'CELESTA', 'GLOCK', 'CHIME', 'KALIMBA', 'STEEL', 'GONG', 'MALLET', 'TUB')),
    ('D', ('DRUM', 'TOM', 'SNARE', 'KICK', 'PERC', 'TIMP', 'CONGA', 'CLAP')),
    ('G', ('ORGAN', 'ORG ', 'ACCORD')),
    ('K', ('PIANO', 'E.P', 'EP ', 'CLAV', 'HARPSI', 'RHODES', 'TINE', 'KEYS', 'PNO')),
    ('R', ('BRASS', 'TRUMPET', 'TRPT', 'TROMBONE', 'HORN', 'TUBA')),
    ('S', ('STRING', 'VIOLIN', 'CELLO', 'ORCH', 'STR ')),
    ('W', ('FLUTE', 'CLARINET', 'SAX', 'OBOE', 'PICCOLO', 'BASSOON', 'HARMONICA', 'WHISTLE', 'PAN ', 'BREATH')),
    ('U', ('HARP', 'GUITAR', 'GTR', 'KOTO', 'BANJO', 'PLUCK', 'SITAR', 'LUTE', 'MANDOLIN')),
    ('F', ('EFFECT', 'SFX', 'NOISE', 'SPACE', 'EXPLO', 'TRAIN', 'HELI', 'LASER', 'FX')),
    ('L', ('LEAD', 'SOLO', 'SYN')),
    ('P', ('PAD', 'CHOIR', 'VOX', 'VOICE')),
]

def clean_name(raw):
    """A voice's 10 name bytes as a C string: printable ASCII, no quotes or backslashes, spaces collapsed."""
    t = ''.join(chr(c) if 32 <= c < 127 else ' ' for c in raw)
    t = t.replace('"', "'").replace('\\', '/').replace('?', ' ')
    return re.sub(' +', ' ', t).strip() or 'VOICE'

def category(name):
    n = name.upper() + ' '
    for letter, words in RULES:
        if any(w in n for w in words):
            return letter
    return 'O'

def load(path):
    d = open(path, 'rb').read()
    if len(d) != 4104:
        sys.exit(f"{path}: mide {len(d)} bytes, deberian ser 4104")
    if d[0] != 0xF0 or d[1] != 0x43 or (d[2] & 0xF0) != 0x00 or d[3] != 0x09 \
       or d[4] != 0x20 or d[5] != 0x00 or d[-1] != 0xF7:
        sys.exit(f"{path}: cabecera SysEx de 32 voces DX7 no valida")
    body = d[6:6 + 4096]
    if (-sum(body)) & 0x7F != d[6 + 4096]:
        sys.exit(f"{path}: checksum incorrecto (archivo danado)")
    if max(body) > 0x7F:
        sys.exit(f"{path}: hay bytes de mas de 7 bits")
    return body

def main():
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    *srcs, out = sys.argv[1:]
    names = ["ROM1A", "ROM1B", "ROM2A", "ROM2B"]
    rows = []
    for s in srcs:
        b = load(s)
        rows += [b[i * 128:(i + 1) * 128] for i in range(32)]
    with open(out, 'w', encoding='utf-8') as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n")
        f.write("/* Generado por tools/gen_dx7_rom.py. NO editar a mano.\n")
        f.write(" * Banco DX7: 128 voces = ROM1A, ROM1B, ROM2A, ROM2B (32 cada una), en ese orden.\n")
        f.write(" * Cada registro son los 128 bytes empaquetados del bulk dump (sin cabecera ni checksum):\n")
        f.write(" * el mismo formato que FM6_PACKED, listo para fm6_unpack(). */\n")
        f.write("#pragma once\n#include <stdint.h>\n")
        f.write("#define DX7_ROM_N 128u\n")
        f.write("static const uint8_t DX7_ROM[DX7_ROM_N][128] = {\n")
        for i, r in enumerate(rows):
            nm = ''.join(chr(c) if 32 <= c < 127 else '?' for c in r[118:128]).rstrip()
            f.write(f"    /* {i + 1:3d} {names[i // 32]}-{i % 32 + 1:02d} {nm} */ {{\n")
            for k in range(0, 128, 16):
                f.write("        " + ", ".join(f"0x{c:02X}" for c in r[k:k + 16]) + ",\n")
            f.write("    },\n")
        f.write("};\n")
        nm = [clean_name(r[118:128]) for r in rows]
        cats = ''.join(category(x) for x in nm)
        f.write("\n/* FM6's preset table (eng_fm6.c): X(name, mono). BASS voices play MONO. */\n")
        f.write("#define DX7_ROM_LIST(X) \\\n")
        f.write(" \\\n".join(f'    X("{x}", {1 if category(x) == "B" else 0})' for x in nm) + "\n")
        f.write("\n/* one category letter per voice, in browse.c's CAT_LETTER (B bass, K keys, ...) */\n")
        f.write(f'#define DX7_ROM_CATS "{cats}"\n')
    print(f"OK: {len(rows)} voces escritas en {out}")

main()
