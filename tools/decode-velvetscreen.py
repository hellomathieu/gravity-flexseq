#!/usr/bin/env python3
"""Decode a U8g2 bitmap font embedded as an octal-escaped C string in Gravity.ino.
Extracts every glyph into a {charcode: {w,h,xoff,yoff,advance,rows[]}} atlas.

PROVENANCE. The font data belongs to the original Sitka firmware
(https://git.sitkainstruments.com/Oleksiy/GravityFW), which is GPLv3. This script
only decodes it; the generated atlas carries that provenance.

SOURCE. The reference is the GravityFW clone sitting NEXT TO this repository, as
CLAUDE.md's source hierarchy expects. It used to be a hardcoded absolute path
into ~/Downloads -- a stray copy (byte-identical, verified 2026-08-20) in a
folder people empty. Override with --src or GRAVITY_FW_INO when the clone lives
elsewhere.
"""
import argparse, os, re, json, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SRC = ROOT.parent / "GravityFW" / "src" / "Gravity" / "Gravity.ino"
DEFAULT_OUT = ROOT / "sim" / "src" / "sim" / "velvetscreen.font.json"


def resolve_src(explicit):
    """--src, sinon $GRAVITY_FW_INO, sinon le clone GravityFW voisin.

    Une source DESIGNEE (option ou variable) est honoree ou echoue : pas de
    repli silencieux, qui decoderait un autre fichier que celui demande.
    """
    for value, origin in ((explicit, "--src"),
                          (os.environ.get("GRAVITY_FW_INO"), "$GRAVITY_FW_INO")):
        if value:
            path = Path(value).expanduser()
            if not path.is_file():
                sys.exit(f"{origin} designe un fichier inexistant : {path}")
            return path

    if DEFAULT_SRC.is_file():
        return DEFAULT_SRC

    sys.exit(
        f"Gravity.ino introuvable : {DEFAULT_SRC}\n\n"
        "Cloner le firmware d'origine a cote de ce depot :\n"
        "  git clone https://git.sitkainstruments.com/Oleksiy/GravityFW\n"
        "ou designer le fichier : --src <chemin>/Gravity.ino"
    )

def extract_literal(text, varname):
    # Find `... varname[NNN] ... = <adjacent string literals> ;`
    # NB: the font data contains literal ';' bytes, so we cannot naively
    # search for the terminating ';'. Instead we collect consecutive
    # "..." literals and stop when a ';' appears OUTSIDE the strings.
    i = text.index(varname + "[")
    eq = text.index("=", i)
    parts = []
    last_end = eq + 1
    for m in re.finditer(r'"((?:[^"\\]|\\.)*)"', text[eq+1:], re.S):
        start = eq + 1 + m.start()
        gap = text[last_end:start]
        if ";" in gap:
            break
        parts.append(m.group(1))
        last_end = eq + 1 + m.end()
    return "".join(parts)

def c_unescape(s):
    out = bytearray()
    i = 0
    while i < len(s):
        c = s[i]
        if c == "\\":
            i += 1
            d = s[i]
            if d in "01234567":
                oct_digits = d
                i += 1
                while i < len(s) and len(oct_digits) < 3 and s[i] in "01234567":
                    oct_digits += s[i]; i += 1
                out.append(int(oct_digits, 8) & 0xFF)
                continue
            else:
                simple = {"n":10,"t":9,"r":13,"\\":92,'"':34,"'":39,"0":0}
                out.append(simple.get(d, ord(d))); i += 1; continue
        else:
            out.append(ord(c)); i += 1
    return bytes(out)

class BitReader:
    def __init__(self, data, bytepos):
        self.data = data; self.pos = bytepos * 8
    def u(self, cnt):
        v = 0
        for k in range(cnt):
            byte = self.data[self.pos >> 3]
            v |= ((byte >> (self.pos & 7)) & 1) << k
            self.pos += 1
        return v
    def s(self, cnt):
        return self.u(cnt) - (1 << (cnt - 1))

def decode(data):
    h = data
    glyph_cnt = h[0]
    bits0, bits1 = h[2], h[3]
    bw, bh, bx, by, bd = h[4], h[5], h[6], h[7], h[8]
    glyphs = {}
    p = 23
    for _ in range(glyph_cnt):
        enc = data[p]
        jump = data[p+1]
        if jump == 0:
            break
        r = BitReader(data, p + 2)
        w = r.u(bw); ht = r.u(bh)
        g = {"w": w, "h": ht, "xoff": 0, "yoff": 0, "advance": 0, "rows": []}
        if w > 0 and ht > 0:
            g["xoff"] = r.s(bx); g["yoff"] = r.s(by); g["advance"] = r.s(bd)
            total = w * ht
            grid = [0] * total
            cur = 0
            while cur < total:
                a = r.u(bits0); b = r.u(bits1)
                while True:
                    for _ in range(a):
                        if cur < total: grid[cur] = 0; cur += 1
                    for _ in range(b):
                        if cur < total: grid[cur] = 1; cur += 1
                    if r.u(1) == 0: break
            g["rows"] = ["".join(str(grid[y*w + x]) for x in range(w)) for y in range(ht)]
        else:
            # blank glyph (e.g. space): width/height are 0, advance follows.
            g["advance"] = r.s(bd)
        glyphs[enc] = g
        p += jump
    return glyph_cnt, glyphs

def show(g):
    for row in g["rows"]:
        print("  " + row.replace("0", "·").replace("1", "█"))

parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
parser.add_argument("--src", help="chemin de Gravity.ino (defaut : clone GravityFW voisin)")
parser.add_argument("--out", default=DEFAULT_OUT, type=Path,
                    help=f"atlas JSON a ecrire (defaut : {DEFAULT_OUT.relative_to(ROOT)})")
args = parser.parse_args()

src = resolve_src(args.src)
print(f"source : {src}")

text = src.read_text()
raw = c_unescape(extract_literal(text, "velvetscreen"))
print(f"velvetscreen raw bytes = {len(raw)}")
cnt, glyphs = decode(raw)
print(f"declared glyph_cnt={cnt}, decoded={len(glyphs)}")
for ch in "pqADEIT1":
    code = ord(ch)
    if code in glyphs:
        g = glyphs[code]
        print(f"\n'{ch}' (0x{code:02x}) w={g['w']} h={g['h']} adv={g['advance']}")
        show(g)
    else:
        print(f"\n'{ch}' MISSING")

# emit atlas
atlas = {str(k): v for k, v in glyphs.items()}
with args.out.open("w") as fh:
    json.dump({"name": "velvetscreen", "glyphs": atlas}, fh, indent=0)
print(f"\nwrote {args.out}")
