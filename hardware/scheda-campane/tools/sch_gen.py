"""Genera lo schema elettrico KiCad (.kicad_sch) dalla stessa definizione usata per il PCB.

Ogni piedino è collegato con un'etichetta di rete (stesso nome usato sul circuito stampato),
quindi schema e PCB descrivono esattamente gli stessi collegamenti.

Uso:  /usr/bin/python3 sch_gen.py <uscita.kicad_sch>
"""
import os
import re
import sys
import uuid

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import board_def as B  # noqa: E402

SYM_ROOT = os.environ.get("KICAD7_SYMBOL_DIR", "/usr/share/kicad/symbols")
GRID = 1.27
STUB = 2.54
CHAR_W = 1.05

SYMBOL_FOR_PART = {
    "TB2": "Connector:Screw_Terminal_01x02",
    "SOCK12": "Connector_Generic:Conn_01x12",
    "PTC": "Device:Polyfuse",
    "SS34": "Device:D_Schottky",
    "TVS": "Device:D_Zener",
    "1N4148W": "Device:D",
    "C100u": "Device:C_Polarized",
    "C10u": "Device:C", "C22u": "Device:C", "C100n": "Device:C",
    "R1k": "Device:R", "R10k": "Device:R", "R4k7": "Device:R",
    "R10k0805": "Device:R", "R4k7_1206": "Device:R",
    "LEDG": "Device:LED", "LEDR": "Device:LED",
    "TSR": "Regulator_Switching:TSR_1-2450",
    "ULN": "Transistor_Array:ULN2003",
    "PC817": "Isolator:PC817",
    "DS3231": "Timer_RTC:DS3231M",
    "BAT": "Device:Battery_Cell",
}

BLOCKS = [
    ("ALIMENTAZIONE 24V -> 5V", ["J1", "F1", "D1", "D2", "C2", "C1", "U1", "C3", "D3", "C4", "R1", "LED1"]),
    ("T-DISPLAY (connettori)", ["J10", "J11", "C11", "R20", "LED10"]),
    ("USCITE CAMPANE (ULN2003, attive HIGH)", ["U2", "R2", "R3", "R4", "R5",
                                              "J2", "R6", "LED2", "J3", "R7", "LED3",
                                              "J4", "R8", "LED4", "J5", "R9", "LED5"]),
    ("INGRESSI 24V OPTOISOLATI", ["J6", "R10", "LED6", "D4", "U3", "R14", "C5",
                                   "J7", "R11", "LED7", "D5", "U4", "R15", "C6",
                                   "J8", "R12", "LED8", "D6", "U5", "R16", "C7",
                                   "J9", "R13", "LED9", "D7", "U6", "R17", "C8"]),
    ("OROLOGIO RTC DS3231", ["U7", "BT1", "C9", "C10", "R18", "R19"]),
]
NOTES = [
    "Uscite: ULN2003 = interruttore verso 0V. Collegare la bobina del rele' d'interfaccia tra +24V e OUTx.",
    "GPIO25 -> CAMPANA 1, GPIO26 -> CAMPANA 2, GPIO33 -> USCITA 3, GPIO17 -> USCITA 4 (firmware: env scheda_v1).",
    "Ingressi: pulsante tra +24V e INx. Premuto = GPIO a livello basso (come il cablaggio originale).",
    "GPIO27 = FUNERALE, GPIO32 = MESSA, GPIO36 = IN 3, GPIO39 = IN 4.",
    "Non collegare nulla a GPIO12 (pin di avvio). Zona antenna del T-Display senza rame.",
]


def uid():
    return str(uuid.uuid4())


def snap(v):
    return round(round(v / GRID) * GRID, 4)


def extract_symbol(lib, name):
    path = os.path.join(SYM_ROOT, lib + ".kicad_sym")
    txt = open(path, encoding="utf-8").read()
    start = txt.find(f'\n  (symbol "{name}"')
    if start < 0:
        raise SystemExit(f"simbolo mancante {lib}:{name}")
    start += 1
    depth, i = 0, start
    while True:
        c = txt[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                break
        elif c == '"':
            i += 1
            while txt[i] != '"':
                if txt[i] == "\\":
                    i += 1
                i += 1
        i += 1
    block = txt[start:i + 1]
    if "(extends" in block.split("\n")[0]:
        raise SystemExit(f"simbolo derivato non supportato: {name}")
    pins = []
    for m in re.finditer(r'\(pin \S+ \S+ \(at ([-\d.]+) ([-\d.]+) (\d+)\) \(length ([-\d.]+)\).*?'
                         r'\(name "([^"]*)".*?\(number "([^"]*)"', block, re.S):
        x, y, a, L, pname, num = m.groups()
        pins.append((num, float(x), float(y), int(a), float(L)))
    full = block.replace(f'(symbol "{name}"', f'(symbol "{lib}:{name}"', 1)
    return full, pins


DIRS = {0: (1, 0), 90: (0, -1), 180: (-1, 0), 270: (0, 1)}   # direzione sullo schermo


class Placed:
    def __init__(self, ref, part, nets):
        self.ref, self.part, self.nets = ref, part, nets
        self.lib_id = SYMBOL_FOR_PART[part]
        lib, name = self.lib_id.split(":")
        self.lib_text, self.pins = extract_symbol(lib, name)
        # pin: endpoint sullo schermo (relativo), direzione esterna
        self.spins = []
        seen = set()
        bx = []
        for num, x, y, a, L in self.pins:
            ex, ey = x, -y
            ux, uy = DIRS[(a + 180) % 360]
            inner = (ex - ux * L, ey - uy * L)
            bx.append(inner)
            key = (round(ex, 3), round(ey, 3))
            net = nets.get(num)
            self.spins.append((num, ex, ey, (a + 180) % 360, net, key in seen))
            seen.add(key)
        xs = [p[0] for p in bx]
        ys = [p[1] for p in bx]
        self.body = (min(xs) - 1.27, min(ys) - 1.27, max(xs) + 1.27, max(ys) + 1.27)
        value, descr, lcsc = B.PARTS[part]
        self.value, self.lcsc = value, lcsc
        outs = {o for (_n, _x, _y, o, _net, _d) in self.spins}
        l, t, r, b = self.body
        if 90 not in outs:
            self.fields = [(0.5 * (l + r), t - 3.81, "center"), (0.5 * (l + r), t - 1.27, "center")]
        elif 0 not in outs:
            self.fields = [(r + 1.27, 0.5 * (t + b) - 1.27, "left"), (r + 1.27, 0.5 * (t + b) + 1.27, "left")]
        else:
            self.fields = [(r + 1.27, t - 3.81, "left"), (r + 1.27, t - 1.27, "left")]
        self.extent = self.compute_extent()

    def compute_extent(self):
        l, t, r, b = self.body
        pts = [(l, t), (r, b)]
        for (_num, ex, ey, o, net, dup) in self.spins:
            ux, uy = DIRS[o]
            length = STUB + (CHAR_W * len(net) + 1.5 if net else 1.0)
            pts.append((ex + ux * length, ey + uy * length + (1.3 if ux else 0)))
            pts.append((ex + ux * length, ey + uy * length - (1.3 if ux else 0)))
            if not ux:
                pts.append((ex + 1.5, ey + uy * length))
                pts.append((ex - 1.5, ey + uy * length))
        for (fx, fy, just), txt in zip(self.fields, (self.ref, self.value)):
            w = CHAR_W * len(txt)
            x0 = fx - w / 2 if just == "center" else fx
            pts += [(x0, fy - 1.3), (x0 + w, fy + 0.4)]
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        return min(xs), min(ys), max(xs), max(ys)


def pack(items, x0, y0, width, gap=5.08):
    """Disposizione a scaffale: riempie le righe da sinistra a destra."""
    x, y, row_h = x0, y0, 0.0
    for it in items:
        l, t, r, b = it.extent
        w, h = r - l, b - t
        if x + w > x0 + width and x > x0:
            x, y, row_h = x0, y + row_h + gap, 0.0
        it.sx = snap(x - l)
        it.sy = snap(y - t)
        x += w + gap
        row_h = max(row_h, h)
    return y + row_h


def font(size=1.27, bold=False):
    return f"(font (size {size} {size}){' bold' if bold else ''})"


def emit(out_path):
    comps = {c[0]: c for c in B.COMPONENTS}
    placed = {}
    for title, refs in BLOCKS:
        for ref in refs:
            _r, _fpk, part, *_rest, nets = comps[ref]
            placed[ref] = Placed(ref, part, nets)
    missing = [c[0] for c in B.COMPONENTS if c[1] != "MH" and c[0] not in placed]
    if missing:
        raise SystemExit(f"componenti non nello schema: {missing}")

    # colonne: sinistra (alimentazione, ingressi), destra (T-Display, uscite, RTC)
    left = [BLOCKS[0], BLOCKS[3]]
    right = [BLOCKS[1], BLOCKS[2], BLOCKS[4]]
    frames = []
    col_w = 250.0
    y_end = []
    for cx, col in ((15.0, left), (15.0 + col_w + 15.0, right)):
        y = 25.0
        for title, refs in col:
            ytop = y
            yb = pack([placed[r] for r in refs], cx + 4, y + 9, col_w - 8)
            frames.append((title, cx, ytop, cx + col_w, yb + 4))
            y = yb + 12
        y_end.append(y)
    page_w = 15.0 + 2 * col_w + 30.0
    page_h = max(y_end) + 40.0
    notes_y = max(y_end) + 2

    root = uid()
    lines = [
        '(kicad_sch (version 20230121) (generator eeschema)',
        f'  (uuid {root})',
        f'  (paper "User" {page_w:.0f} {page_h:.0f})',
        '  (title_block (title "Scheda base campane ESP32 T-Display") (date "2026-11-02") (rev "1.0")',
        '    (company "Controller campane chiesa")',
        '    (comment 1 "Alimentazione 24 VDC - uscite ULN2003 - ingressi 24 V optoisolati - RTC DS3231"))',
        '  (lib_symbols',
    ]
    done = set()
    for p in placed.values():
        if p.lib_id not in done:
            done.add(p.lib_id)
            lines.append("    " + p.lib_text.replace("\n", "\n    "))
    lines.append("  )")

    for title, x1, y1, x2, y2 in frames:
        lines.append(f'  (rectangle (start {x1:.2f} {y1:.2f}) (end {x2:.2f} {y2:.2f}) '
                     f'(stroke (width 0.3) (type dash) (color 0 0 0 0)) (fill (type none)) (uuid {uid()}))')
        lines.append(f'  (text "{title}" (at {x1 + 3:.2f} {y1 + 6:.2f} 0) '
                     f'(effects {font(2.5, True)} (justify left bottom)) (uuid {uid()}))')
    for k, note in enumerate(NOTES):
        lines.append(f'  (text "{note}" (at 19 {notes_y + 6 + k * 4.5:.2f} 0) '
                     f'(effects {font(2.0)} (justify left bottom)) (uuid {uid()}))')

    for ref, p in placed.items():
        lib, name = p.lib_id.split(":")
        fp_lib, fp_name = B.FP[comps[ref][1]]
        props = []
        for k, ((fx, fy, just), (key, val)) in enumerate(zip(p.fields, (("Reference", ref), ("Value", p.value)))):
            j = "" if just == "center" else f" (justify {just})"
            props.append(f'    (property "{key}" "{val}" (at {p.sx + fx:.2f} {p.sy + fy:.2f} 0) '
                         f'(effects {font()}{j}))')
        props.append(f'    (property "Footprint" "{fp_lib}:{fp_name}" (at {p.sx:.2f} {p.sy:.2f} 0) '
                     f'(effects {font()} hide))')
        props.append(f'    (property "Datasheet" "~" (at {p.sx:.2f} {p.sy:.2f} 0) (effects {font()} hide))')
        props.append(f'    (property "LCSC" "{p.lcsc}" (at {p.sx:.2f} {p.sy:.2f} 0) (effects {font()} hide))')
        lines.append(f'  (symbol (lib_id "{p.lib_id}") (at {p.sx:.2f} {p.sy:.2f} 0) (unit 1)')
        lines.append('    (in_bom yes) (on_board yes) (dnp no)')
        lines.append(f'    (uuid {uid()})')
        lines += props
        for num, *_ in p.pins:
            lines.append(f'    (pin "{num}" (uuid {uid()}))')
        lines.append(f'    (instances (project "scheda-campane" (path "/{root}" (reference "{ref}") (unit 1))))')
        lines.append('  )')
        # collegamenti: filo corto + etichetta oppure crocetta "non collegato"
        for (num, ex, ey, o, net, dup) in p.spins:
            if dup:
                continue
            px, py = p.sx + ex, p.sy + ey
            if not net:
                lines.append(f'  (no_connect (at {px:.2f} {py:.2f}) (uuid {uid()}))')
                continue
            ux, uy = DIRS[o]
            qx, qy = px + ux * STUB, py + uy * STUB
            lines.append(f'  (wire (pts (xy {px:.2f} {py:.2f}) (xy {qx:.2f} {qy:.2f})) '
                         f'(stroke (width 0) (type default)) (uuid {uid()}))')
            just = "left bottom" if o in (0, 90) else "right bottom"
            lines.append(f'  (label "{net}" (at {qx:.2f} {qy:.2f} {o}) (fields_autoplaced) '
                         f'(effects {font()} (justify {just})) (uuid {uid()}))')

    lines.append('  (sheet_instances (path "/" (page "1")))')
    lines.append(")")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("schema salvato:", out_path, f"({page_w:.0f}x{page_h:.0f} mm)")


if __name__ == "__main__":
    emit(sys.argv[1])
