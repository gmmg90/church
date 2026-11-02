"""Definizione della scheda base campane (T-Display + 24V).

Contiene componenti, footprint, codici LCSC, posizioni e collegamenti (netlist).
Coordinate in mm, origine in alto a sinistra, asse Y verso il basso (convenzione KiCad).
Rotazione in gradi, antioraria come in KiCad.
"""

BOARD_W = 100.0
BOARD_H = 80.0
CORNER_R = 2.0

# T-Display: posizione del suo sistema di riferimento (impronta ufficiale LilyGO)
# ruotato di 90 gradi: USB-C verso il bordo destro.
TD_X0 = 53.3
TD_Y0 = 40.0
# ingombro T-Display sulla scheda (sporge 1.5 mm oltre il bordo destro)
TD_RECT = (TD_X0 - 3.302, TD_Y0 - 13.942, TD_X0 + 48.188, TD_Y0 + 11.148)
HDR_X = TD_X0 + 4.318          # x del primo pin di ogni fila
HDR_UP_Y = TD_Y0 - 12.827      # fila superiore (5V ... 3V3)
HDR_LO_Y = TD_Y0 + 10.033      # fila inferiore (GND ... 3V3)

# zona antenna WiFi: niente rame su entrambi i lati
ANTENNA_KEEPOUT = (47.0, 25.0, 56.2, 52.3)

MOUNTING_HOLES = [(4.0, 4.0), (96.0, 4.0), (4.0, 76.0), (96.0, 76.0)]
MH_KEEPOUT_R = 3.6

FP = {
    "TB2": ("TerminalBlock_Phoenix", "TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal"),
    "SOCK12": ("Connector_PinSocket_2.54mm", "PinSocket_1x12_P2.54mm_Vertical"),
    "F1812": ("Fuse", "Fuse_1812_4532Metric"),
    "SMA": ("Diode_SMD", "D_SMA"),
    "SMB": ("Diode_SMD", "D_SMB"),
    "SOD123": ("Diode_SMD", "D_SOD-123"),
    "CP8x10": ("Capacitor_SMD", "CP_Elec_8x10"),
    "C1206": ("Capacitor_SMD", "C_1206_3216Metric"),
    "C0805": ("Capacitor_SMD", "C_0805_2012Metric"),
    "C0603": ("Capacitor_SMD", "C_0603_1608Metric"),
    "R0603": ("Resistor_SMD", "R_0603_1608Metric"),
    "R0805": ("Resistor_SMD", "R_0805_2012Metric"),
    "R1206": ("Resistor_SMD", "R_1206_3216Metric"),
    "LED0603": ("LED_SMD", "LED_0603_1608Metric"),
    "LED0805": ("LED_SMD", "LED_0805_2012Metric"),
    "DCDC": ("Converter_DCDC", "Converter_DCDC_RECOM_R-78E-0.5_THT"),
    "SOIC16": ("Package_SO", "SOIC-16_3.9x9.9mm_P1.27mm"),
    "SOIC8": ("Package_SO", "SOIC-8_3.9x4.9mm_P1.27mm"),
    "SMDIP4": ("Package_DIP", "SMDIP-4_W9.53mm"),
    "CR2032": ("Battery", "BatteryHolder_Keystone_3034_1x20mm"),
    "MH": ("MountingHole", "MountingHole_3.2mm_M3"),
}

# Parti: valore, descrizione per la BOM, codice LCSC
PARTS = {
    "TB2": ("KF128-5.08-2P", "Morsetto a vite 2 poli passo 5,08 mm", "C474952"),
    "SOCK12": ("Female 1x12 2.54 H8.5", "Connettore femmina 1x12 passo 2,54 mm, h 8,5 mm, dorato", "C22373898"),
    "PTC": ("mSMD110-33V", "Fusibile ripristinabile 1,1 A 33 V (1812)", "C70119"),
    "SS34": ("SS34", "Diodo Schottky 3 A 40 V (SMA)", "C8678"),
    "TVS": ("SMBJ33A", "Diodo TVS 600 W 33 V unidirezionale (SMB)", "C224019"),
    "1N4148W": ("1N4148W", "Diodo di segnale (SOD-123)", "C81598"),
    "C10u": ("10uF 50V", "Condensatore ceramico 10 uF 50 V X5R (1206)", "C13585"),
    "C22u": ("22uF 25V", "Condensatore ceramico 22 uF 25 V X5R (0805)", "C45783"),
    "C100n": ("100nF 50V", "Condensatore ceramico 100 nF 50 V X7R (0603)", "C14663"),
    "R1k": ("1k", "Resistenza 1 kOhm 1% (0603)", "C21190"),
    "R10k": ("10k", "Resistenza 10 kOhm 1% (0603)", "C25804"),
    "R4k7": ("4.7k", "Resistenza 4,7 kOhm 1% (0603)", "C23162"),
    "R10k0805": ("10k", "Resistenza 10 kOhm 1% (0805)", "C17414"),
    "R4k7_1206": ("4.7k", "Resistenza 4,7 kOhm 1% 0,25 W (1206)", "C17936"),
    "LEDG": ("Verde", "LED verde (0805)", "C2297"),
    "LEDR": ("Rosso", "LED rosso (0603)", "C2286"),
    "TSR": ("K7805-1000R3", "Convertitore DC-DC 24 V -> 5 V 1 A, ingresso 8-36 V (YLPTEC)", "C2992391"),
    "ULN": ("ULN2003ADR", "Driver Darlington 7 canali (SOIC-16)", "C7512"),
    "PC817": ("PC817X3CSP9F", "Optoisolatore (SMD-4)", "C41807"),
    "DS3231": ("DS3231MZ+TRL", "RTC di precisione I2C DS3231M (SOIC-8)", "C107410"),
    "BAT": ("Keystone 3034", "Portabatteria CR2032 SMD", "C5213768"),
    "MH": ("M3", "Foro di fissaggio M3", None),
}

COMPONENTS = []  # (ref, fp_key, part_key, x, y, rot, {pad: net})


def add(ref, fpk, part, x, y, rot, nets):
    COMPONENTS.append((ref, fpk, part, x, y, rot, nets))


# ------------------------------------------------------------------ Alimentazione
add("J1", "TB2", "TB2", 12.0, 74.8, 0, {"1": "+24V_IN", "2": "GND"})
add("F1", "F1812", "PTC", 12.0, 61.5, 90, {"1": "+24V_IN", "2": "+24V_F"})
add("D1", "SMA", "SS34", 12.0, 54.0, 270, {"2": "+24V_F", "1": "+24V"})
add("D2", "SMB", "TVS", 17.5, 56.0, 270, {"1": "+24V", "2": "GND"})
add("C2", "C1206", "C10u", 22.0, 56.0, 90, {"1": "+24V", "2": "GND"})
add("C1", "C1206", "C10u", 29.5, 59.0, 90, {"1": "+24V", "2": "GND"})
add("U1", "DCDC", "TSR", 38.0, 62.0, 0, {"1": "+24V", "2": "GND", "3": "+5V_DC"})
add("C3", "C0805", "C22u", 43.4, 67.0, 0, {"1": "+5V_DC", "2": "GND"})
add("D3", "SMA", "SS34", 36.0, 68.0, 0, {"2": "+5V_DC", "1": "+5V"})
add("C4", "C0603", "C100n", 31.0, 68.0, 90, {"1": "+5V", "2": "GND"})
add("R1", "R0603", "R1k", 24.0, 66.0, 0, {"1": "+5V_DC", "2": "LED_PWR"})
add("LED1", "LED0805", "LEDG", 24.0, 69.0, 180, {"2": "LED_PWR", "1": "GND"})

# ------------------------------------------------------------------ T-Display
UP_NETS = ["+3V3", "GPIO36", None, None, "GPIO39", "GPIO32", "GPIO33",
           "GPIO25", "GPIO26", "GPIO27", "GND", "+5V"]
LO_NETS = ["GND", "GND", "I2C_SDA", "I2C_SCL", "GPIO17", "GPIO2", None,
           None, None, "GND", "GND", "+3V3"]
add("J10", "SOCK12", "SOCK12", HDR_X, HDR_UP_Y, 90,
    {str(i + 1): n for i, n in enumerate(UP_NETS) if n})
add("J11", "SOCK12", "SOCK12", HDR_X, HDR_LO_Y, 90,
    {str(i + 1): n for i, n in enumerate(LO_NETS) if n})

# ------------------------------------------------------------------ Uscite (ULN2003)
# ULN canale k: ingresso pin k, uscita pin 17-k
add("U2", "SOIC16", "ULN", 66.0, 59.5, 270, {
    "1": "GPIO17", "2": "GPIO33", "3": "GPIO26", "4": "GPIO25",
    "5": "GND", "6": "GND", "7": "GND", "8": "GND", "9": "+24V",
    "13": "OUT1", "14": "OUT2", "15": "OUT3", "16": "OUT4"})
# pull-down sotto il T-Display, vicino ai rispettivi pin (tengono spente le uscite a ESP32 spento)
for ref, x, y, rot, gpio in (("R2", HDR_X + 7 * 2.54, 31.0, 270, "GPIO25"),
                             ("R3", HDR_X + 8 * 2.54, 31.0, 270, "GPIO26"),
                             ("R4", HDR_X + 6 * 2.54, 31.0, 270, "GPIO33"),
                             ("R5", HDR_X + 4 * 2.54, 46.0, 90, "GPIO17")):
    add(ref, "R0603", "R10k", x, y, rot, {"1": gpio, "2": "GND"})

OUTPUTS = [  # terminale, x pin1, resistenza LED, LED, net, etichetta
    ("J2", 47.0, "R6", "LED2", "OUT1", "CAMPANA 1"),
    ("J3", 59.0, "R7", "LED3", "OUT2", "CAMPANA 2"),
    ("J4", 71.0, "R8", "LED4", "OUT3", "USCITA 3"),
    ("J5", 83.0, "R9", "LED5", "OUT4", "USCITA 4"),
]
for k, (jref, xp, rref, lref, net, _lbl) in enumerate(OUTPUTS, start=1):
    add(jref, "TB2", "TB2", xp, 74.8, 0, {"1": "+24V", "2": net})
    add(rref, "R0805", "R10k0805", xp + 0.5, 66.2, 0, {"1": "+24V", "2": f"OUTLED{k}"})
    add(lref, "LED0603", "LEDR", xp + 4.6, 66.2, 180, {"2": f"OUTLED{k}", "1": net})

# ------------------------------------------------------------------ Ingressi optoisolati
INPUTS = [  # x0 cella, terminale, R ingresso, LED, diodo, opto, pull-up, C, nome, gpio
    (8.0, "J6", "R10", "LED6", "D4", "U3", "R14", "C5", "IN 3", "GPIO36"),
    (30.0, "J7", "R11", "LED7", "D5", "U4", "R15", "C6", "IN 4", "GPIO39"),
    (52.0, "J8", "R12", "LED8", "D6", "U5", "R16", "C7", "MESSA", "GPIO32"),
    (74.0, "J9", "R13", "LED9", "D7", "U6", "R17", "C8", "FUNERALE", "GPIO27"),
]
for k, (x0, jref, rin, led, dio, opto, rpu, cf, _name, gpio) in enumerate(INPUTS, start=1):
    n24, na, nk = f"IN{k}_24", f"IN{k}_A", f"IN{k}_K"
    add(jref, "TB2", "TB2", x0 + 12.0, 5.2, 180, {"1": "+24V", "2": n24})
    add(rin, "R1206", "R4k7_1206", x0 + 7.2, 15.0, 270, {"1": n24, "2": na})
    add(led, "LED0805", "LEDG", x0 + 4.0, 16.4, 0, {"2": na, "1": nk})
    add(dio, "SOD123", "1N4148W", x0 + 3.3, 12.9, 180, {"1": na, "2": "GND"})
    add(opto, "SMDIP4", "PC817", x0 + 8.0, 20.5, 0, {"1": nk, "2": "GND", "3": "GND", "4": gpio})
    add(rpu, "R0603", "R10k", x0 + 16.0, 19.5, 90, {"1": gpio, "2": "+3V3"})
    add(cf, "C0603", "C100n", x0 + 18.5, 19.5, 90, {"1": gpio, "2": "GND"})

# ------------------------------------------------------------------ RTC
add("BT1", "CR2032", "BAT", 17.0, 40.0, 0, {"1": "VBAT", "2": "GND"})
u7 = {"2": "+3V3", "5": "GND", "6": "VBAT", "7": "I2C_SDA", "8": "I2C_SCL"}
add("U7", "SOIC8", "DS3231", 37.5, 40.0, 0, u7)
add("C9", "C0603", "C100n", 32.5, 33.2, 0, {"1": "+3V3", "2": "GND"})
add("C10", "C0603", "C100n", 45.0, 38.0, 90, {"1": "VBAT", "2": "GND"})
add("R18", "R0603", "R4k7", 40.0, 32.6, 0, {"1": "+3V3", "2": "I2C_SDA"})
add("R19", "R0603", "R4k7", 44.2, 32.6, 0, {"1": "+3V3", "2": "I2C_SCL"})

# ------------------------------------------------------------------ LED di stato e 3V3
add("R20", "R0603", "R1k", 36.0, 50.0, 0, {"1": "LED_STAT", "2": "GPIO2"})
add("LED10", "LED0805", "LEDG", 32.0, 50.0, 0, {"2": "LED_STAT", "1": "GND"})
add("C11", "C0603", "C100n", 88.5, 54.0, 90, {"1": "+3V3", "2": "GND"})

# ------------------------------------------------------------------ Fori di fissaggio
for i, (x, y) in enumerate(MOUNTING_HOLES, start=1):
    add(f"MH{i}", "MH", "MH", x, y, 0, {})

# Larghezza piste per rete (mm); default per le altre
NET_WIDTH = {
    "+24V_IN": 1.0, "+24V_F": 1.0, "+24V": 0.8, "GND": 0.6,
    "+5V_DC": 0.8, "+5V": 0.8, "+3V3": 0.4,
    "OUT1": 0.6, "OUT2": 0.6, "OUT3": 0.6, "OUT4": 0.6,
}
DEFAULT_WIDTH = 0.3
CLEARANCE = 0.2
VIA_D = 0.6
VIA_DRILL = 0.3

# Testi serigrafia: (testo, x, y, altezza, angolo, lato)
SILK = []
for x0, jref, *_rest in INPUTS:
    name, gpio = _rest[-2], _rest[-1]
    SILK.append(("IN", x0 + 6.92, 11.7, 0.8, 0, "F"))
    SILK.append(("+24V", x0 + 12.0, 11.7, 0.8, 0, "F"))
    SILK.append((f"{name} ({gpio})", x0 + 8.0, 24.4, 0.8, 0, "F"))
for jref, xp, _r, _l, net, lbl in OUTPUTS:
    SILK.append((lbl, xp + 3.6, 64.3, 0.9, 0, "F"))
    SILK.append(("+24V", xp, 68.25, 0.8, 0, "F"))
    SILK.append(("OUT", xp + 5.08, 68.25, 0.8, 0, "F"))
SILK += [
    ("24V DC", 16.5, 66.8, 0.9, 0, "F"),
    ("+24V", 12.0, 68.25, 0.8, 0, "F"),
    ("0V", 17.08, 68.25, 0.8, 0, "F"),
    ("CAMPANE ESP32  v1.0", 26.0, 28.6, 1.3, 0, "F"),
    ("T-DISPLAY (display verso l'alto)", 73.0, 37.5, 1.0, 0, "F"),
    ("USB-C >", 94.5, 39.8, 0.9, 0, "F"),
    ("ANTENNA", 51.7, 38.6, 0.9, 90, "F"),
    ("RTC", 37.5, 46.6, 0.8, 0, "F"),
    ("PWR", 27.0, 69.0, 0.8, 0, "F"),
    ("STATO", 34.0, 51.6, 0.8, 0, "F"),
    ("Scheda campane ESP32 T-Display v1.0", 50.0, 34.0, 1.5, 0, "B"),
    ("Alimentazione 24 VDC - fusibile ripristinabile 1,1 A", 50.0, 38.0, 1.1, 0, "B"),
    ("Uscite ULN2003: NPN verso 0V, max 500 mA per canale", 50.0, 41.0, 1.1, 0, "B"),
    ("Ingressi 24 V optoisolati (pulsante tra +24V e IN)", 50.0, 44.0, 1.1, 0, "B"),
]
