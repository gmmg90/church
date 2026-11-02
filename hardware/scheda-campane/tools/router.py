"""Instradatore automatico semplice (A* su griglia, 2 strati) per la scheda campane.

Legge la scheda con i componenti posizionati, traccia tutte le reti, aggiunge i piani
di massa (GND) su entrambi i lati e salva il risultato.

Uso:  /usr/bin/python3 router.py <ingresso.kicad_pcb> <uscita.kicad_pcb>
"""
import heapq
import math
import os
import sys
import time

import numpy as np
import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import board_def as B  # noqa: E402

G = 0.2                 # passo griglia (mm)
MARGIN = 0.04           # margine di sicurezza sulla distanza di isolamento
KEEPOUT_MARGIN = 0.2    # margine verso zone vietate e fori
CLR = B.CLEARANCE
VIA_R = B.VIA_D / 2
EDGE_CLR = 0.5
VIA_COST = 6.0
BOTTOM_FACTOR = 2.0
TURN_COST = 0.3
NX = int(round(B.BOARD_W / G)) + 1
NY = int(round(B.BOARD_H / G)) + 1
XS = np.arange(NX) * G
YS = np.arange(NY) * G
R_MAX = 0.5 + CLR + MARGIN + 0.05


def mm(v):
    return pcbnew.FromMM(v)


def tomm(v):
    return pcbnew.ToMM(v)


# ------------------------------------------------------------------ geometria
def dist_seg(px, py, x1, y1, x2, y2):
    dx, dy = x2 - x1, y2 - y1
    L2 = dx * dx + dy * dy
    if L2 == 0:
        return np.hypot(px - x1, py - y1)
    t = np.clip(((px - x1) * dx + (py - y1) * dy) / L2, 0.0, 1.0)
    return np.hypot(px - (x1 + t * dx), py - (y1 + t * dy))


class Obj:
    """Oggetto di rame (o ostacolo) con il campo di distanza precalcolato su una finestra."""

    def __init__(self, net, kind, prm):
        self.net, self.kind, self.prm = net, kind, prm
        bx0, by0, bx1, by1 = self.bbox()
        self.i0 = max(0, int(math.floor((bx0 - R_MAX) / G)))
        self.i1 = min(NX, int(math.ceil((bx1 + R_MAX) / G)) + 1)
        self.j0 = max(0, int(math.floor((by0 - R_MAX) / G)))
        self.j1 = min(NY, int(math.ceil((by1 + R_MAX) / G)) + 1)
        px, py = np.meshgrid(XS[self.i0:self.i1], YS[self.j0:self.j1])
        self.d = self.dist(px, py)

    def bbox(self):
        if self.kind == "seg":
            x1, y1, x2, y2, r = self.prm
            return min(x1, x2) - r, min(y1, y2) - r, max(x1, x2) + r, max(y1, y2) + r
        cx, cy, hx, hy, ang, cr = self.prm
        e = math.hypot(hx, hy)
        return cx - e, cy - e, cx + e, cy + e

    def dist(self, px, py):
        if self.kind == "seg":
            x1, y1, x2, y2, r = self.prm
            return dist_seg(px, py, x1, y1, x2, y2) - r
        cx, cy, hx, hy, ang, cr = self.prm
        c, s = math.cos(ang), math.sin(ang)
        qx = (px - cx) * c + (py - cy) * s
        qy = -(px - cx) * s + (py - cy) * c
        dx = np.abs(qx) - (hx - cr)
        dy = np.abs(qy) - (hy - cr)
        out = np.hypot(np.maximum(dx, 0), np.maximum(dy, 0))
        return out + np.minimum(np.maximum(dx, dy), 0) - cr

    def mark(self, mask, R):
        mask[self.j0:self.j1, self.i0:self.i1] |= self.d < R


def pad_obj(pad, netname):
    pos = pad.GetPosition()
    cx, cy = tomm(pos.x), tomm(pos.y)
    sx, sy = tomm(pad.GetSize().x), tomm(pad.GetSize().y)
    ang = -math.radians(pad.GetOrientationDegrees())
    shape = pad.GetShape()
    if shape == pcbnew.PAD_SHAPE_CIRCLE:
        return Obj(netname, "seg", (cx, cy, cx, cy, sx / 2))
    if shape == pcbnew.PAD_SHAPE_OVAL:
        if abs(sx - sy) < 1e-6:
            return Obj(netname, "seg", (cx, cy, cx, cy, sx / 2))
        if sx > sy:
            h, r = (sx - sy) / 2, sy / 2
            ux, uy = math.cos(ang), -math.sin(ang)
        else:
            h, r = (sy - sx) / 2, sx / 2
            ux, uy = math.sin(ang), math.cos(ang)
        return Obj(netname, "seg", (cx - ux * h, cy - uy * h, cx + ux * h, cy + uy * h, r))
    cr = 0.0
    if shape == pcbnew.PAD_SHAPE_ROUNDRECT:
        cr = min(pad.GetRoundRectCornerRadius() and tomm(pad.GetRoundRectCornerRadius()), min(sx, sy) / 2)
    return Obj(netname, "rect", (cx, cy, sx / 2, sy / 2, ang, cr))


# ------------------------------------------------------------------ stato della scheda
class Router:
    def __init__(self, board):
        self.board = board
        self.objs = [[], []]          # oggetti di rame per strato (0 = F.Cu, 1 = B.Cu)
        self.pads = {}                # net -> lista di (obj, strati, is_smd, ref)
        self.all_pads = []
        self.vias = {}                # net -> lista (x, y)
        self.tracks = []              # (net, layer, x1, y1, x2, y2, w)
        self.via_list = []            # (net, x, y)
        self.nodes = {}               # net -> set di nodi (layer, idx) già connessi
        for fp in board.GetFootprints():
            for pad in fp.Pads():
                attr = pad.GetAttribute()
                netname = pad.GetNetname() or None
                if attr == pcbnew.PAD_ATTRIB_NPTH:
                    continue      # fori di fissaggio: gestiti dalle aree vietate
                o = pad_obj(pad, netname)
                layers = (0, 1) if attr == pcbnew.PAD_ATTRIB_PTH else (0,)
                smd = attr == pcbnew.PAD_ATTRIB_SMD
                for L in layers:
                    self.objs[L].append(o)
                self.all_pads.append((o, layers, smd))
                if netname:
                    self.pads.setdefault(netname, []).append((o, layers, smd, fp.GetReference() + "." + pad.GetNumber()))
        # ostacoli statici: bordo, antenna, fori
        self.static = []
        x1, y1, x2, y2 = B.ANTENNA_KEEPOUT
        self.static.append(Obj("#keepout", "rect", ((x1 + x2) / 2, (y1 + y2) / 2, (x2 - x1) / 2, (y2 - y1) / 2, 0.0, 0.0)))
        for cx, cy in B.MOUNTING_HOLES:
            self.static.append(Obj("#mh", "seg", (cx, cy, cx, cy, B.MH_KEEPOUT_R)))
        px, py = np.meshgrid(XS, YS)
        self.edge_d = np.minimum(np.minimum(px, B.BOARD_W - px), np.minimum(py, B.BOARD_H - py))

    # maschera delle celle vietate al centro di una pista di semilarghezza hw
    def blocked(self, net, L, hw):
        m = self.edge_d < EDGE_CLR + hw + MARGIN
        for o in self.static:
            o.mark(m, hw + KEEPOUT_MARGIN)
        R = hw + CLR + MARGIN
        for o in self.objs[L]:
            if o.net != net:
                o.mark(m, R)
        return m

    def via_blocked(self, net):
        m = self.edge_d < EDGE_CLR + VIA_R + MARGIN
        for o in self.static:
            o.mark(m, VIA_R + KEEPOUT_MARGIN)
        R = VIA_R + CLR + MARGIN
        for L in (0, 1):
            for o in self.objs[L]:
                if o.net != net:
                    o.mark(m, R)
        for o, layers, smd in self.all_pads:      # niente via dentro/vicino ai pad
            o.mark(m, R)
        for (x, y) in self.vias.get(net, []):
            i0, i1 = max(0, int((x - 1.0) / G)), min(NX, int((x + 1.0) / G) + 2)
            j0, j1 = max(0, int((y - 1.0) / G)), min(NY, int((y + 1.0) / G) + 2)
            px, py = np.meshgrid(XS[i0:i1], YS[j0:j1])
            m[j0:j1, i0:i1] |= np.hypot(px - x, py - y) < 0.65
        return m

    # ---------------------------------------------------------------- A*
    def route_net(self, net, width):
        hw = width / 2
        pads = self.pads[net]
        blk = [self.blocked(net, L, hw) for L in (0, 1)]
        vblk = self.via_blocked(net)
        bt = [b.ravel().tobytes() for b in blk]
        vb = vblk.ravel().tobytes()

        def pad_cells(o, layers):
            cells = set()
            sub = o.d <= -0.0
            jj, ii = np.nonzero(sub)
            for j, i in zip(jj + o.j0, ii + o.i0):
                idx = j * NX + i
                for L in layers:
                    if not bt[L][idx]:
                        cells.add((L, idx))
            return cells

        cells = [pad_cells(o, layers) for (o, layers, smd, ref) in pads]
        for k, c in enumerate(cells):
            if not c:
                return False, f"pad {pads[k][3]} irraggiungibile"
        connected = {0}
        start = set(cells[0])
        new_tracks, new_vias, added = [], [], []
        while len(connected) < len(pads):
            targets = {}
            centers = []
            for k, c in enumerate(cells):
                if k in connected:
                    continue
                for n in c:
                    targets[n] = k
                o = pads[k][0]
                cx = (o.i0 + o.i1) / 2.0
                cy = (o.j0 + o.j1) / 2.0
                ext = max(o.i1 - o.i0, o.j1 - o.j0) / 2.0 - R_MAX / G
                centers.append((cx, cy, max(0.0, ext)))
            path = self.astar(start, targets, centers, bt, vb)
            if path is None:
                missing = [pads[k][3] for k in range(len(pads)) if k not in connected]
                self.undo(added, net, len(new_vias))
                return False, "non raggiunti: " + ", ".join(missing[:4])
            k = targets[path[-1]]
            connected.add(k)
            start |= set(path) | cells[k]
            segs, vias = self.path_to_geometry(path)
            for (L, x1, y1, x2, y2) in segs:
                new_tracks.append((net, L, x1, y1, x2, y2, width))
                o = Obj(net, "seg", (x1, y1, x2, y2, hw))
                self.objs[L].append(o)
                added.append((L, o))
            for (x, y) in vias:
                new_vias.append((net, x, y))
                self.vias.setdefault(net, []).append((x, y))
                o = Obj(net, "seg", (x, y, x, y, VIA_R))
                self.objs[0].append(o)
                self.objs[1].append(o)
                added += [(0, o), (1, o)]
            if vias:   # aggiorna maschera via (distanza tra fori della stessa rete)
                vb = self.via_blocked(net).ravel().tobytes()
        self.tracks += new_tracks
        self.via_list += new_vias
        return True, f"{len(new_tracks)} segmenti, {len(new_vias)} via"

    def undo(self, added, net, nvias):
        for L, o in added:
            self.objs[L].remove(o)
        if nvias:
            del self.vias[net][-nvias:]

    def connect_gnd(self, width=0.4):
        """Collega ogni pad SMD di massa a una via verso il piano di massa inferiore."""
        net, hw = "GND", width / 2
        bt0 = self.blocked(net, 0, hw).ravel().tobytes()
        vb = self.via_blocked(net).ravel().tobytes()
        gnd_nodes = set()
        for (o, layers, smd, ref) in self.pads.get(net, []):   # pad passanti GND = già sul piano
            if layers == (0, 1):
                jj, ii = np.nonzero(o.d <= 0.0)
                gnd_nodes |= {(j + o.j0) * NX + (i + o.i0) for j, i in zip(jj, ii)}
        failed = []
        for (o, layers, smd, ref) in self.pads.get(net, []):
            if layers != (0,):
                continue                      # pad passanti: già sul piano inferiore
            jj, ii = np.nonzero(o.d <= 0.0)
            start = {(j + o.j0) * NX + (i + o.i0) for j, i in zip(jj, ii)}
            start = {idx for idx in start if not bt0[idx]}
            if not start:
                failed.append(ref)
                continue
            if start & gnd_nodes:
                continue
            path = self.dijkstra_via(start, bt0, vb, gnd_nodes)
            if path is None:
                failed.append(ref)
                continue
            end = path[-1]
            reached_existing = end in gnd_nodes
            for (L, x1, y1, x2, y2) in simplify([(0, i) for i in path]):
                self.tracks.append((net, 0, x1, y1, x2, y2, width))
                self.objs[0].append(Obj(net, "seg", (x1, y1, x2, y2, hw)))
            gnd_nodes |= set(path) | start
            if not reached_existing:
                x, y = (end % NX) * G, (end // NX) * G
                if not vb[end]:
                    self.via_list.append((net, x, y))
                    self.vias.setdefault(net, []).append((x, y))
                    ov = Obj(net, "seg", (x, y, x, y, VIA_R))
                    self.objs[0].append(ov)
                    self.objs[1].append(ov)
                    vb = self.via_blocked(net).ravel().tobytes()
        return failed

    def dijkstra_via(self, start, bt0, vb, gnd_nodes):
        SQ2 = math.sqrt(2)
        dirs = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
                (1, 1, SQ2), (1, -1, SQ2), (-1, 1, SQ2), (-1, -1, SQ2)]
        g = {n: 0.0 for n in start}
        parent = {n: None for n in start}
        heap = [(0.0, n) for n in start]
        heapq.heapify(heap)
        while heap:
            gc, n = heapq.heappop(heap)
            if gc > g[n]:
                continue
            if not vb[n] or (n in gnd_nodes and n not in start):
                path = []
                while n is not None:
                    path.append(n)
                    n = parent[n]
                return path[::-1]
            if gc > 150:
                continue
            x, y = n % NX, n // NX
            for dx, dy, c in dirs:
                nx_, ny_ = x + dx, y + dy
                if nx_ < 0 or ny_ < 0 or nx_ >= NX or ny_ >= NY:
                    continue
                m = ny_ * NX + nx_
                if bt0[m]:
                    continue
                ng = gc + c
                if ng < g.get(m, 1e18):
                    g[m] = ng
                    parent[m] = n
                    heapq.heappush(heap, (ng, m))
        return None

    def astar(self, start, targets, centers, bt, vb):
        SQ2 = math.sqrt(2)
        dirs = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
                (1, 1, SQ2), (1, -1, SQ2), (-1, 1, SQ2), (-1, -1, SQ2)]

        def h(idx):
            x, y = idx % NX, idx // NX
            best = 1e18
            for cx, cy, ext in centers:
                dx, dy = abs(x - cx), abs(y - cy)
                d = max(dx, dy) + (SQ2 - 1) * min(dx, dy) - ext
                if d < best:
                    best = d
            return max(best, 0.0)

        g = {}
        parent = {}
        pdir = {}
        heap = []
        for n in start:
            g[n] = 0.0
            parent[n] = None
            pdir[n] = -1
            heapq.heappush(heap, (h(n[1]), 0.0, n))
        expanded = 0
        while heap:
            f, gc, n = heapq.heappop(heap)
            if gc > g.get(n, 1e18):
                continue
            if n in targets:
                return self.rebuild(n, parent)
            expanded += 1
            if expanded > 600000:
                return None
            L, idx = n
            x, y = idx % NX, idx // NX
            fac = BOTTOM_FACTOR if L == 1 else 1.0
            pd = pdir[n]
            btl = bt[L]
            for di, (dx, dy, c) in enumerate(dirs):
                nx_, ny_ = x + dx, y + dy
                if nx_ < 0 or ny_ < 0 or nx_ >= NX or ny_ >= NY:
                    continue
                nidx = ny_ * NX + nx_
                if btl[nidx]:
                    continue
                if dx and dy and (btl[y * NX + nx_] and btl[ny_ * NX + x]):
                    continue
                cost = c * fac
                if pd >= 0 and pd != di:
                    cost += TURN_COST
                m = (L, nidx)
                ng = gc + cost
                if ng < g.get(m, 1e18):
                    g[m] = ng
                    parent[m] = n
                    pdir[m] = di
                    heapq.heappush(heap, (ng + h(nidx), ng, m))
            if not vb[idx]:
                m = (1 - L, idx)
                if not bt[1 - L][idx]:
                    ng = gc + VIA_COST
                    if ng < g.get(m, 1e18):
                        g[m] = ng
                        parent[m] = n
                        pdir[m] = -1
                        heapq.heappush(heap, (ng + h(idx), ng, m))
        return None

    @staticmethod
    def rebuild(n, parent):
        path = []
        while n is not None:
            path.append(n)
            n = parent[n]
        return path[::-1]

    @staticmethod
    def path_to_geometry(path):
        segs, vias = [], []

        def xy(idx):
            return (idx % NX) * G, (idx // NX) * G

        run = [path[0]]
        for n in path[1:]:
            if n[0] != run[-1][0]:          # cambio strato -> via
                vias.append(xy(n[1]))
                segs += simplify(run)
                run = [n]
            else:
                run.append(n)
        segs += simplify(run)
        return segs, vias


def simplify(run):
    """Unisce i passi collineari di un tratto sullo stesso strato."""
    L = run[0][0]
    pts = [((n[1] % NX), (n[1] // NX)) for n in run]
    if len(pts) < 2:
        return []
    out = []
    a = pts[0]
    prev = pts[0]
    d = None
    for p in pts[1:]:
        nd = (p[0] - prev[0], p[1] - prev[1])
        if d is not None and nd != d:
            out.append((L, a[0] * G, a[1] * G, prev[0] * G, prev[1] * G))
            a = prev
        d = nd
        prev = p
    out.append((L, a[0] * G, a[1] * G, prev[0] * G, prev[1] * G))
    return out



# ------------------------------------------------------------------ ordine reti
def net_order(r):
    first = ["+24V_IN", "+24V_F", "+24V", "+5V_DC", "+5V", "OUT1", "OUT2", "OUT3", "OUT4"]
    last = ["+3V3"]
    rest = [n for n in r.pads if n not in first and n not in last and n != "GND" and len(r.pads[n]) > 1]

    def span(n):
        xs = [(o.i0 + o.i1) / 2 for o, *_ in r.pads[n]]
        ys = [(o.j0 + o.j1) / 2 for o, *_ in r.pads[n]]
        return (max(xs) - min(xs)) + (max(ys) - min(ys))

    rest.sort(key=span)
    return [n for n in first if n in r.pads] + rest + [n for n in last if n in r.pads]


def run_routing(board_path, max_iter=12):
    order = None
    for it in range(max_iter):
        board = pcbnew.LoadBoard(board_path)
        r = Router(board)
        if order is None:
            order = net_order(r)
        failed = []
        t0 = time.time()
        gfail = r.connect_gnd()
        if gfail:
            print(f"  [{it}] GND senza via: {gfail}", flush=True)
        for net in order:
            w = B.NET_WIDTH.get(net, B.DEFAULT_WIDTH)
            ok, msg = r.route_net(net, w)
            if not ok:
                # secondo tentativo con pista più sottile
                if w > B.DEFAULT_WIDTH:
                    ok, msg = r.route_net(net, max(B.DEFAULT_WIDTH, w / 2))
                if not ok:
                    failed.append(net)
                    print(f"  [{it}] FALLITO {net}: {msg}", flush=True)
        print(f"iterazione {it}: {len(order) - len(failed)}/{len(order)} reti, {time.time() - t0:.0f}s", flush=True)
        if not failed and not gfail:
            return board, r
        order = failed + [n for n in order if n not in failed]
    raise SystemExit("instradamento incompleto")


def commit(board, r):
    nets = {}
    for net, L, x1, y1, x2, y2, w in r.tracks:
        if (x1, y1) == (x2, y2):
            continue
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(pcbnew.VECTOR2I(mm(float(x1)), mm(float(y1))))
        t.SetEnd(pcbnew.VECTOR2I(mm(float(x2)), mm(float(y2))))
        t.SetWidth(mm(float(w)))
        t.SetLayer(pcbnew.F_Cu if L == 0 else pcbnew.B_Cu)
        t.SetNet(board.FindNet(net))
        board.Add(t)
    for net, x, y in r.via_list:
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(pcbnew.VECTOR2I(mm(float(x)), mm(float(y))))
        v.SetViaType(pcbnew.VIATYPE_THROUGH)
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetWidth(mm(B.VIA_D))
        v.SetDrill(mm(B.VIA_DRILL))
        v.SetNet(board.FindNet(net))
        board.Add(v)


def add_gnd_zones(board):
    gnd = board.FindNet("GND")
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(gnd)
        z.SetZoneName("GND_" + ("F" if layer == pcbnew.F_Cu else "B"))
        z.SetLocalClearance(mm(0.3))
        z.SetMinThickness(mm(0.25))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetThermalReliefGap(mm(0.3))
        z.SetThermalReliefSpokeWidth(mm(0.4))
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        z.SetAssignedPriority(0)
        ol = z.Outline()
        ol.NewOutline()
        for x, y in [(0, 0), (B.BOARD_W, 0), (B.BOARD_W, B.BOARD_H), (0, B.BOARD_H)]:
            ol.Append(mm(x), mm(y))
        board.Add(z)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())


def main(src, dst):
    board, r = run_routing(src)
    commit(board, r)
    add_gnd_zones(board)
    pcbnew.SaveBoard(dst, board)
    print("salvato", dst, "piste:", len(r.tracks), "via:", len(r.via_list))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
