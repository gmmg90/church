"""Rigenera tutto il progetto della scheda campane con un solo comando.

    /usr/bin/python3 hardware/scheda-campane/tools/make.py

Passi: posizionamento -> instradamento -> DRC -> file JLCPCB -> schema elettrico (PDF)
-> verifica che schema e PCB abbiano le stesse reti -> immagini di anteprima.
Richiede KiCad 7 (pcbnew + kicad-cli) e, per le immagini, Node.js con Playwright.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.dirname(HERE)
NAME = "scheda-campane"
PY = sys.executable
ENV = dict(os.environ,
           KICAD7_FOOTPRINT_DIR=os.environ.get("KICAD7_FOOTPRINT_DIR", "/usr/share/kicad/footprints"),
           KICAD7_SYMBOL_DIR=os.environ.get("KICAD7_SYMBOL_DIR", "/usr/share/kicad/symbols"))


def run(*cmd):
    print("$", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True, env=ENV)


def check_netlist(net_file):
    sys.path.insert(0, HERE)
    import board_def as B
    txt = open(net_file).read()
    sch = {}
    for m in re.finditer(r'\(net \(code "\d+"\) \(name "([^"]*)"\)(.*?)\)\n\s*(?=\(net |\)\))', txt, re.S):
        name = m.group(1).lstrip("/")
        if not name.startswith("unconnected-"):
            sch[name] = set(re.findall(r'\(node \(ref "([^"]+)"\) \(pin "([^"]+)"\)', m.group(2)))
    pcb = {}
    for ref, _fpk, _part, _x, _y, _rot, nets in B.COMPONENTS:
        for pad, n in nets.items():
            pcb.setdefault(n, set()).add((ref, pad))
    diff = [n for n in set(sch) | set(pcb) if sch.get(n) != pcb.get(n)]
    if diff:
        raise SystemExit(f"schema e PCB diversi sulle reti: {diff}")
    print(f"verifica schema/PCB: {len(pcb)} reti identiche")


def main():
    tmp = tempfile.mkdtemp()
    docs = os.path.join(OUT, "docs")
    prod = os.path.join(OUT, "produzione")
    os.makedirs(docs, exist_ok=True)
    pcb = os.path.join(OUT, NAME + ".kicad_pcb")
    placed = os.path.join(tmp, "placed.kicad_pcb")

    run(PY, os.path.join(HERE, "build.py"), placed)
    run(PY, "-u", os.path.join(HERE, "router.py"), placed, pcb)

    # controllo regole di progetto (DRC)
    drc = os.path.join(docs, "drc-report.txt")
    code = ("import pcbnew,sys;b=pcbnew.LoadBoard(sys.argv[1]);pcbnew.ZONE_FILLER(b).Fill(b.Zones());"
            "pcbnew.WriteDRCReport(b,sys.argv[2],pcbnew.EDA_UNITS_MILLIMETRES,True)")
    run(PY, "-c", code, pcb, drc)
    rep = open(drc).read()
    found = re.findall(r"\*\* Found (\d+) ", rep)
    print("DRC:", found)
    if any(int(n) for n in found):
        raise SystemExit("il DRC ha trovato problemi: vedi " + drc)
    with open(drc, "w") as f:   # niente percorsi locali nel report
        f.write(rep.replace(OUT + os.sep, ""))

    run(PY, os.path.join(HERE, "export.py"), pcb, prod)

    # schema elettrico
    sch = os.path.join(OUT, NAME + ".kicad_sch")
    run(PY, os.path.join(HERE, "sch_gen.py"), sch)
    run("kicad-cli", "sch", "export", "pdf", "-o", os.path.join(docs, "schema-elettrico.pdf"), sch)
    net = os.path.join(tmp, "net.net")
    run("kicad-cli", "sch", "export", "netlist", "--format", "kicadsexpr", "-o", net, sch)
    check_netlist(net)

    # immagini di anteprima
    views = [
        ("scheda-lato-componenti", "F.Cu,F.SilkS,Edge.Cuts", False),
        ("scheda-lato-saldature", "B.Cu,B.SilkS,Edge.Cuts", True),
        ("scheda-montaggio", "F.SilkS,F.Fab,Edge.Cuts", False),
    ]
    for fname, layers, mirror in views:
        svg = os.path.join(tmp, fname + ".svg")
        cmd = ["kicad-cli", "pcb", "export", "svg", "--layers", layers, "--page-size-mode", "2",
               "--exclude-drawing-sheet", "-o", svg, pcb]
        if mirror:
            cmd.insert(4, "--mirror")
        run(*cmd)
        if shutil.which("node"):
            run("node", os.path.join(HERE, "render.js"), svg, os.path.join(docs, fname + ".png"), "1600", "#ffffff")

    for junk in (NAME + ".kicad_prl",):
        p = os.path.join(OUT, junk)
        if os.path.exists(p):
            os.remove(p)
    shutil.rmtree(tmp)
    print("FATTO")


if __name__ == "__main__":
    main()
