# Pannello touch campane su Orange Pi 2G-IoT

L'ESP32 resta il controller delle campane (relè, orari, Telegram, la sua Web UI).
L'Orange Pi aggiunge:

- il **pannello touch** a tutto schermo in chiesa
- l'**accesso da fuori** tramite Cloudflare, con login

```
Touchscreen ─┐
             ├─► nginx (Orange Pi) ──► ESP32 (solo rete locale)
Internet ─► Cloudflare (login) ─► tunnel ─┘
```

Se l'Orange Pi si spegne, **le campane continuano a suonare in orario** e restano
validi la Web UI dell'ESP32 e il bot Telegram.

## Cosa c'è nella cartella

| File | A cosa serve |
|---|---|
| `panel/index.html` | Il pannello touch (pagina unica, funziona offline in rete locale) |
| `nginx/campane.conf.template` | Proxy: porta 8080 = pannello, porta 8081 = Web UI originale ESP32 |
| `kiosk/` | Avvio automatico a schermo intero |
| `campane.conf.example` | Impostazioni da compilare (IP ESP32, password, token) |
| `install.sh` | Installa tutto in un colpo |

## Da preparare prima (5 minuti)

1. **IP fisso per l'ESP32.** Nel router riserva un indirizzo all'ESP32 (prenotazione DHCP), così non cambia.
   L'IP attuale lo vedi sul suo display.
2. **Dominio su Cloudflare.** Serve un dominio gestito da Cloudflare (anche gratuito). Se non ce l'hai, domani decidiamo come fare.
3. **Accesso all'Orange Pi** via tastiera+monitor oppure SSH, con utente che può usare `sudo`, e **internet** attivo.
4. Facoltativo ma consigliato: **cambiare la password di default** `admin / LA_TUA_PASSWORD_ADMIN` in `src/include/config.h` e ricaricare il firmware. Va fatto sull'ESP32; poi aggiorni la password in `campane.conf`.

## Installazione sull'Orange Pi

1. Copia questa cartella sull'Orange Pi (chiavetta USB o `scp`).
   Se gli script danno errori strani dopo la copia da Windows, converti le righe:
   `sed -i 's/\r$//' install.sh kiosk/start-kiosk.sh`
2. Crea le impostazioni:
   ```bash
   cp campane.conf.example campane.conf
   nano campane.conf        # IP dell'ESP32, utente e password
   ```
3. Lancia l'installazione:
   ```bash
   sudo bash install.sh
   ```
   Alla fine deve comparire "OK: pannello e proxy funzionano".
4. Prova il proxy dall'Orange Pi:
   ```bash
   curl http://127.0.0.1:8080/api/status
   ```
   Deve rispondere con un JSON (`bellsEnabled`, `wifiConnected`...).
5. Riavvia (`sudo reboot`): il pannello parte da solo sullo schermo.

## Tunnel Cloudflare (accesso da fuori)

Nel pannello Cloudflare **Zero Trust**:

1. **Networks → Tunnels → Create a tunnel** (tipo *Cloudflared*), nome `campane`.
2. Scegli **Debian / ARM** e copia il **token** (la stringa lunga dopo `service install`).
   Mettilo in `campane.conf` come `CF_TOKEN` e rilancia `sudo bash install.sh`
   (oppure installa a mano `cloudflared` come mostrato dalla procedura Cloudflare).
3. Nel tunnel aggiungi due **Public hostname**:
   | Indirizzo | Servizio |
   |---|---|
   | `campane.tuodominio.it` | `HTTP` → `localhost:8080` (pannello touch) |
   | `avanzate.tuodominio.it` | `HTTP` → `localhost:8081` (Web UI completa dell'ESP32) |
4. **Importante, protezione:** *Access → Applications → Add → Self-hosted*, per **entrambi** gli indirizzi,
   con regola *Allow* solo per le tue email (login con codice via email o Google).
   **Senza questo passaggio chiunque conosca l'indirizzo potrebbe suonare le campane.**
5. Prova da telefono con i dati mobili (non il Wi-Fi della chiesa).

## Perché due indirizzi

La Web UI dell'ESP32 usa percorsi assoluti (`/api/...`), quindi non può stare sotto un
sotto-percorso tipo `/avanzate`. Due indirizzi risolvono il problema senza modificare il firmware.

## Sicurezza (importante)

- Le API dell'ESP32 (`/api/...`) **non hanno password**: la protegge solo la pagina HTML.
  Per questo nginx ascolta solo su `127.0.0.1` e sulla porta del pannello lascia passare
  **solo** i comandi necessari (stato, melodie, orari, avvio/stop, attiva/disattiva).
  Reset WiFi, restore, impostazioni di rete e relè diretti **non** sono raggiungibili dal pannello.
- Chiunque sia sulla rete Wi-Fi della chiesa può comunque parlare direttamente con l'ESP32.
  Consiglio: rete Wi-Fi con password robusta, e rete ospiti separata.
- La password dell'ESP32 resta nel file `/etc/nginx/conf.d/campane.conf` (leggibile solo da root).

## Come si usa il pannello

- **Home:** ora, prossimo evento, FUNERALE, CHIAMATA MESSA (con conferma, per evitare tocchi per errore),
  interruttore campane, STOP EMERGENZA.
- **Melodie:** elenco e prova. Per creare o modificare melodie usa la Web UI completa (`avanzate`).
- **Orari:** orari settimanali (aggiungi, attiva/disattiva, elimina) ed eventi speciali (attiva/disattiva, elimina).
  Per aggiungere un evento speciale usa la Web UI completa.
- **Sistema:** stato di ESP32, Wi-Fi, RTC, NTP, temperatura; backup; sincronizza ora.
- Da telefono la stessa pagina si adatta allo schermo verticale.

I pulsanti FUNERALE e MESSA usano le melodie con indice **0** e **1** (come nel README del progetto).

## Se qualcosa non va

| Problema | Cosa controllare |
|---|---|
| Schermo nero / nessun pannello | `sudo systemctl status campane-kiosk` e `journalctl -u campane-kiosk -b`. Controlla che sia installato un browser (`chromium` o `firefox-esr`). |
| Il pannello compare ma il touch non risponde | Dipende dal driver del display/touch dell'Orange Pi: guarda `xinput list` e dimmi l'output. |
| Banner rosso "ESP32 non raggiungibile" | Controlla l'IP in `campane.conf`, poi `sudo bash install.sh` di nuovo. Prova `curl http://IP_ESP32/api/status` dall'Orange Pi. |
| Da fuori non si apre | Stato del tunnel: `sudo systemctl status cloudflared`. Controlla i "Public hostname" e che il servizio sia `localhost:8080`. |
| Pannello lento | L'Orange Pi ha poca RAM (256 MB): chiudi altri programmi; se il browser è troppo pesante proviamo `firefox-esr` o una versione ancora più leggera. |
| Cambiata la password dell'ESP32 | Aggiorna `campane.conf` e rilancia `sudo bash install.sh`. |

## Note oneste su ciò che non ho potuto provare

Non ho l'Orange Pi davanti: il pannello l'ho scritto sulle API lette dal tuo codice, ma
lo schermo/touch/browser dell'Orange Pi (driver, risoluzione, memoria) lo verifichiamo domani sul posto.
