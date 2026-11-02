"""Esporta i file di produzione per JLCPCB: Gerber + forature (zip), BOM e CPL.

Uso:  /usr/bin/python3 export.py <scheda.kicad_pcb> <cartella_uscita>
"""
import csv
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import board_def as B  # noqa: E402

LAYERS = "F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts"


def run(cmd):
    print("$", " ".join(cmd))
    subprocess.run(cmd, check=True)


def main(pcb, outdir):
    os.makedirs(outdir, exist_ok=True)
    name = os.path.splitext(os.path.basename(pcb))[0]
    tmp = tempfile.mkdtemp()
    gdir = os.path.join(tmp, "gerber")
    os.makedirs(gdir)
    run(["kicad-cli", "pcb", "export", "gerbers", "--layers", LAYERS,
         "--subtract-soldermask", "-o", gdir + "/", pcb])
    run(["kicad-cli", "pcb", "export", "drill", "--format", "excellon",
         "--drill-origin", "absolute", "--excellon-units", "mm",
         "--excellon-separate-th",
         "-o", gdir + "/", pcb])
    zpath = os.path.join(outdir, f"{name}-gerber-jlcpcb.zip")
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(os.listdir(gdir)):
            z.write(os.path.join(gdir, f), f)
    print("gerber:", zpath)

    # posizioni dei componenti (CPL)
    pos = os.path.join(tmp, "pos.csv")
    run(["kicad-cli", "pcb", "export", "pos", "--format", "csv", "--units", "mm",
         "--side", "front", "-o", pos, pcb])
    board = pcbnew.LoadBoard(pcb)
    fps = {fp.GetReference(): fp for fp in board.GetFootprints()}
    cpl_rows = []
    with open(pos, newline="") as f:
        for row in csv.DictReader(f):
            ref = row["Ref"]
            fp = fps[ref]
            if not fp.HasProperty("LCSC"):
                continue
            cpl_rows.append([ref, row["PosX"], row["PosY"], "Top", row["Rot"]])
    cpl_rows.sort(key=lambda r: (r[0].rstrip("0123456789"), int(r[0][len(r[0].rstrip("0123456789")):] or 0)))
    cpl = os.path.join(outdir, f"{name}-CPL-jlcpcb.csv")
    with open(cpl, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        w.writerows(cpl_rows)
    print("cpl:", cpl, len(cpl_rows), "componenti")

    # distinta materiali (BOM) raggruppata per codice LCSC
    groups = {}
    for ref, fpk, part, *_ in B.COMPONENTS:
        value, descr, lcsc = B.PARTS[part]
        if not lcsc:
            continue
        key = (lcsc, value, B.FP[fpk][1])
        groups.setdefault(key, (descr, []))[1].append(ref)

    def refkey(r):
        p = r.rstrip("0123456789")
        return p, int(r[len(p):])

    bom = os.path.join(outdir, f"{name}-BOM-jlcpcb.csv")
    rows = sorted(groups.items(), key=lambda kv: refkey(sorted(kv[1][1], key=refkey)[0]))
    with open(bom, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #", "Quantity", "Descrizione"])
        for (lcsc, value, fpname), (descr, refs) in rows:
            refs = sorted(refs, key=refkey)
            w.writerow([value, ",".join(refs), fpname, lcsc, len(refs), descr])
    print("bom:", bom, len(rows), "righe")
    shutil.rmtree(tmp)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
