# Istruzioni per Claude Code: installare il pannello campane sull'Orange Pi 2G-IoT

Incolla tutto questo a Claude Code, aperto nella cartella `orangepi-pannello`
(dentro `church - Telegram`). Rispondi in italiano.

---

## Contesto

Giuseppe ha un controller per le campane di una chiesa: un **ESP32** (repo `gmmg90/church`, firmware
v2.3.10) che comanda due relè, ha le programmazioni, un bot Telegram e una Web UI. **Non va modificato.**

Vogliamo aggiungere un **Orange Pi 2G-IoT** (Cortex-A5, 256 MB RAM, nessuna Ethernet, solo Wi-Fi) che faccia:

1. **Pannello touch** a tutto schermo in chiesa (pagina già pronta: `panel/index.html`)
2. **Accesso da fuori** tramite tunnel **Cloudflare** + Cloudflare Access (login)

Architettura:

```
Touchscreen ─┐
             ├─► nginx (Orange Pi) ──► ESP32 (solo rete locale)
Internet ─► Cloudflare Access ─► cloudflared ─┘
```

Nella cartella ci sono già: `panel/index.html`, `nginx/campane.conf.template`, `kiosk/`,
`campane.conf.example`, `install.sh`, `LEGGIMI.md`. **Leggili prima di fare qualsiasi cosa.**

## Stato di partenza

- L'Orange Pi **non ha ancora un sistema operativo installato** (lo sta preparando Giuseppe).
- **Non c'è uno schermo collegato**: si lavora solo via **SSH**. Il kiosk (schermo) si installa ma
  non si può provare adesso.
- Windows con Claude Code sul PC di Giuseppe; dal PC si raggiunge la stessa rete locale.

## Cose da sapere prima (verificate)

- Le immagini disponibili per questa scheda sono **vecchie** (Ubuntu 16.04 Server, Raspbian/Debian Jessie,
  Armbian 5.x con kernel 3.10). Gli archivi dei repository di quelle versioni possono essere spostati
  (`old-releases`, `archive.debian.org`): se `apt update` fallisce, sistema le sorgenti prima di tutto.
- Il **Wi-Fi** di questa scheda viene riportato come **poco affidabile** (cadute, ping alto).
  Se serve, valutare un adattatore USB Ethernet o USB Wi-Fi supportato.
- Per avviare da microSD bisogna mettere lo switch di boot su SD (se parte senza SD sta usando la NAND).
- **256 MB di RAM**: serve un browser leggero; Chromium potrebbe non essere disponibile su un sistema così vecchio.
- Le API dell'ESP32 (`/api/...`) **non hanno password**: per questo nginx ascolta solo su `127.0.0.1`
  e filtra i percorsi. Non cambiare questo comportamento.

## Regole

- **Non modificare il firmware o i file in `src/`** del progetto ESP32.
- **Non pubblicare né committare** password, token Cloudflare o il file `campane.conf`.
- **Non esporre mai** le porte 8080/8081 sulla rete: solo `127.0.0.1`.
- Prima di ogni comando che cambia qualcosa sull'Orange Pi, di' cosa stai per fare in una riga.
- Dopo ogni passo, verifica con un comando e mostra il risultato. Se fallisce, ferma e spiega.
- Se qualcosa richiede una scelta di Giuseppe (IP, utente, dominio, token), **chiedila** e aspetta.

## Cosa ti serve da Giuseppe (chiedi quando serve)

1. IP dell'Orange Pi, utente e password SSH
2. IP dell'ESP32 (visibile sul suo display) + utente/password Web UI (default `admin` / `LA_TUA_PASSWORD_ADMIN`, da `src/include/config.h`)
3. Dominio su Cloudflare e, quando si arriva lì, il **token del tunnel**

## Piano

### Fase A: diagnosi (sola lettura)
Collegati via SSH e raccogli: `/etc/os-release`, `uname -mr`, `free -m`, `df -h /`, servizi attivi, porte in ascolto,
connessione internet (`curl https://github.com`), se `apt update` funziona.
Riassumi a Giuseppe cosa hai trovato e segnala i problemi (repo vecchi, RAM, Wi-Fi) **prima** di installare.

### Fase B: sistema di base
- Sistema le sorgenti `apt` se necessario, `apt update`, installa `nginx curl ca-certificates`.
- Imposta data/ora e fuso `Europe/Rome` (`timedatectl` o equivalente).
- Verifica che l'Orange Pi raggiunga l'ESP32: `curl -m 6 http://IP_ESP32/api/status` deve dare un JSON.

### Fase C: pannello + proxy
- Copia la cartella sull'Orange Pi, crea `campane.conf` da `campane.conf.example` con i valori reali.
- Esegui `install.sh` con `KIOSK="no"` (niente schermo per ora). Se il sistema è troppo vecchio per lo script,
  adattalo e spiega le modifiche.
- Verifica: `curl http://127.0.0.1:8080/api/status` risponde; `curl -I http://127.0.0.1:8081/` chiede/gestisce l'auth;
  `curl http://127.0.0.1:8080/api/configure-wifi` deve dare **403** (percorso non permesso).
- Verifica che le porte siano solo su 127.0.0.1: `ss -ltnp | grep -E '8080|8081'`.
- Controlla che nginx riparta da solo dopo un riavvio.

### Fase D: tunnel Cloudflare
- Guida Giuseppe nel pannello Cloudflare Zero Trust: creare il tunnel `campane`, copiare il token.
- Installa `cloudflared` per la giusta architettura (`uname -m`), con il token, come servizio.
- Public hostname: `campane.<dominio>` → `http://localhost:8080` e `avanzate.<dominio>` → `http://localhost:8081`.
- **Obbligatorio:** Cloudflare Access su **entrambi** gli indirizzi, con regola Allow solo per le email di Giuseppe.
  Non considerare finito il lavoro finché non è fatto e provato da rete esterna (dati mobili).

### Fase E: affidabilità
- nginx e cloudflared devono ripartire da soli dopo un riavvio o un calo di corrente: provare con `sudo reboot`.
- Controlla uso RAM a riposo (`free -m`) e dimmi quanto margine resta.
- Se il Wi-Fi cade, proponi una soluzione (USB Ethernet, riconnessione automatica, ecc.).

### Fase F: schermo (solo quando Giuseppe ha lo schermo)
- Abilita `KIOSK="yes"` / `install.sh`, verifica che parta un browser a schermo intero, che il touch funzioni
  (`xinput list`) e che la pagina sia leggibile a 800×480 (o alla risoluzione reale: se serve, adatta il CSS).
- Se Chromium non c'è o è troppo pesante, prova `firefox-esr` o un browser più leggero.

## Consegna finale

Riepilogo breve: cosa funziona, cosa è stato provato davvero, cosa resta da fare (es. schermo), indirizzi usati,
come riavviare/aggiornare il pannello (`panel/index.html` in `/opt/campane/panel/`).
Ricorda a Giuseppe di **cambiare la password di default** dell'ESP32 (`ADMIN_PASSWORD` in `config.h`, con
nuovo upload del firmware) e di aggiornare poi `campane.conf`.
