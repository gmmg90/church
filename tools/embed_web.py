# Script PlatformIO (pre-build): comprime le pagine web di data/ e le incorpora nel
# firmware come array PROGMEM in src/include/web_assets.h.
# Così aggiornare la pagina non richiede di ricaricare la SPIFFS (che contiene
# melodie, programmazioni e utenti Telegram salvati sul dispositivo).
import gzip
import os

try:
    Import("env")  # noqa: F821 - fornito da PlatformIO
    ROOT = env["PROJECT_DIR"]  # noqa: F821
except NameError:
    ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PAGES = [("index.html", "WEB_INDEX"), ("diag.html", "WEB_DIAG")]
OUT = os.path.join(ROOT, "src", "include", "web_assets.h")


def build():
    parts = [
        "// File generato da tools/embed_web.py: non modificare a mano.\n",
        "#pragma once\n#include <Arduino.h>\n\n",
    ]
    for name, sym in PAGES:
        with open(os.path.join(ROOT, "data", name), "rb") as f:
            gz = gzip.compress(f.read(), compresslevel=9, mtime=0)
        body = ",".join(str(b) for b in gz)
        parts.append(f"// {name}: {len(gz)} byte compressi\n")
        parts.append(f"const uint8_t {sym}_GZ[] PROGMEM = {{{body}}};\n")
        parts.append(f"const size_t {sym}_GZ_LEN = {len(gz)};\n\n")
    text = "".join(parts)
    old = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
    if text != old:  # riscrive solo se cambia, per non forzare ricompilazioni
        with open(OUT, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)


build()
