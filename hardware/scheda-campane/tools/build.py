"""Crea il circuito stampato (componenti posizionati, reti, contorno, serigrafie).

Uso:  /usr/bin/python3 build.py <uscita.kicad_pcb>
Richiede KiCad 7 (modulo pcbnew).
"""
import math
import os
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import board_def as B  # noqa: E402

FP_ROOT = os.environ.get("KICAD7_FOOTPRINT_DIR", "/usr/share/kicad/footprints")


def mm(v):
    return pcbnew.FromMM(v)


def pt(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


def new_board(path):
    board = pcbnew.NewBoard(path)
    ds = board.GetDesignSettings()
    ds.SetCopperLayerCount(2)
    ds.m_TrackMinWidth = mm(0.2)
    ds.m_ViasMinSize = mm(0.5)
    ds.m_MinThroughDrill = mm(0.3)
    ds.m_CopperEdgeClearance = mm(0.5)
    ds.m_HoleClearance = mm(0.25)
    ds.m_HoleToHoleMin = mm(0.25)
    ds.m_MinClearance = mm(B.CLEARANCE)
    nc = ds.m_NetSettings.m_DefaultNetClass
    nc.SetClearance(mm(B.CLEARANCE))
    nc.SetTrackWidth(mm(B.DEFAULT_WIDTH))
    nc.SetViaDiameter(mm(B.VIA_D))
    nc.SetViaDrill(mm(B.VIA_DRILL))
    return board


def add_outline(board):
    w, h, r = B.BOARD_W, B.BOARD_H, B.CORNER_R
    lines = [((r, 0), (w - r, 0)), ((w, r), (w, h - r)),
             ((w - r, h), (r, h)), ((0, h - r), (0, r))]
    for (x1, y1), (x2, y2) in lines:
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pt(x1, y1))
        s.SetEnd(pt(x2, y2))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(mm(0.1))
        board.Add(s)
    # angoli arrotondati: (centro, punto iniziale)
    corners = [((r, r), (0, r)), ((w - r, r), (w - r, 0)),
               ((w - r, h - r), (w, h - r)), ((r, h - r), (r, h))]
    for (cx, cy), (sx, sy) in corners:
        a = pcbnew.PCB_SHAPE(board)
        a.SetShape(pcbnew.SHAPE_T_ARC)
        a.SetCenter(pt(cx, cy))
        a.SetStart(pt(sx, sy))
        a.SetArcAngleAndEnd(pcbnew.EDA_ANGLE(90, pcbnew.DEGREES_T), True)
        a.SetLayer(pcbnew.Edge_Cuts)
        a.SetWidth(mm(0.1))
        board.Add(a)


def rule_area(board, poly, name):
    z = pcbnew.ZONE(board)
    z.SetIsRuleArea(True)
    z.SetZoneName(name)
    ls = pcbnew.LSET()
    ls.AddLayer(pcbnew.F_Cu)
    ls.AddLayer(pcbnew.B_Cu)
    z.SetLayerSet(ls)
    z.SetDoNotAllowCopperPour(True)
    z.SetDoNotAllowTracks(True)
    z.SetDoNotAllowVias(True)
    z.SetDoNotAllowPads(False)
    z.SetDoNotAllowFootprints(False)
    ol = z.Outline()
    ol.NewOutline()
    for x, y in poly:
        ol.Append(mm(x), mm(y))
    board.Add(z)


def add_keepouts(board):
    x1, y1, x2, y2 = B.ANTENNA_KEEPOUT
    rule_area(board, [(x1, y1), (x2, y1), (x2, y2), (x1, y2)], "ANTENNA_WIFI")
    for i, (cx, cy) in enumerate(B.MOUNTING_HOLES, start=1):
        poly = [(cx + B.MH_KEEPOUT_R * math.cos(2 * math.pi * k / 24),
                 cy + B.MH_KEEPOUT_R * math.sin(2 * math.pi * k / 24)) for k in range(24)]
        rule_area(board, poly, f"MH{i}_KEEPOUT")


def add_text(board, text, x, y, size, angle, side):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(pt(x, y))
    t.SetLayer(pcbnew.F_SilkS if side == "F" else pcbnew.B_SilkS)
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
    t.SetTextThickness(mm(max(0.15, size * 0.15)))
    if angle:
        t.SetTextAngle(pcbnew.EDA_ANGLE(angle, pcbnew.DEGREES_T))
    if side == "B":
        t.SetMirrored(True)
    board.Add(t)


def add_tdisplay_outline(board):
    x1, y1, x2, y2 = B.TD_RECT
    x2 = min(x2, B.BOARD_W - 0.3)
    y1, y2 = y1 - 0.6, y2 + 0.6      # leggermente fuori, per non toccare la serigrafia dei connettori
    for (a, b) in [((x1, y1), (x2, y1)), ((x2, y1), (x2, y2)),
                   ((x2, y2), (x1, y2)), ((x1, y2), (x1, y1))]:
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pt(*a))
        s.SetEnd(pt(*b))
        s.SetLayer(pcbnew.F_SilkS)
        s.SetWidth(mm(0.15))
        board.Add(s)


def main(out):
    board = new_board(out)
    nets = {}

    def net(name):
        if name not in nets:
            ni = pcbnew.NETINFO_ITEM(board, name)
            board.Add(ni)
            nets[name] = ni
        return nets[name]

    for ref, fpk, part, x, y, rot, pad_nets in B.COMPONENTS:
        lib, name = B.FP[fpk]
        fp = pcbnew.FootprintLoad(os.path.join(FP_ROOT, lib + ".pretty"), name)
        if fp is None:
            raise SystemExit(f"footprint mancante: {lib}:{name}")
        value, _descr, lcsc = B.PARTS[part]
        fp.SetFPID(pcbnew.LIB_ID(lib, name))
        fp.SetReference(ref)
        fp.SetValue(value)
        fp.SetPosition(pt(x, y))
        fp.SetOrientationDegrees(rot)
        if lcsc:
            fp.SetProperty("LCSC", lcsc)
        if fpk == "MH":
            fp.SetBoardOnly(True)
            fp.SetExcludedFromPosFiles(True)
            fp.SetExcludedFromBOM(True)
        fp.Reference().SetTextSize(pcbnew.VECTOR2I(mm(0.8), mm(0.8)))
        fp.Reference().SetTextThickness(mm(0.12))
        fp.Reference().SetVisible(False)
        fp.Value().SetVisible(False)
        board.Add(fp)
        used = set()
        for pad in fp.Pads():
            num = pad.GetNumber()
            if num in pad_nets:
                pad.SetNet(net(pad_nets[num]))
                used.add(num)
        missing = set(pad_nets) - used
        if missing:
            raise SystemExit(f"{ref}: pad inesistenti {missing}")

    add_outline(board)
    add_keepouts(board)
    add_tdisplay_outline(board)
    for args in B.SILK:
        add_text(board, *args)
    pcbnew.SaveBoard(out, board)
    print("salvato", out, "componenti:", len(B.COMPONENTS), "reti:", len(nets))


if __name__ == "__main__":
    main(sys.argv[1])
