#!/bin/bash
# Installazione pannello campane su Orange Pi (Debian/Ubuntu).
# Uso:  sudo bash install.sh
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
[ "$(id -u)" -eq 0 ] || { echo "Esegui con sudo:  sudo bash install.sh"; exit 1; }
[ -f "$DIR/campane.conf" ] || { echo "Manca campane.conf (copia campane.conf.example e compilalo)"; exit 1; }
# shellcheck disable=SC1091
source "$DIR/campane.conf"

: "${ESP_HOST:?ESP_HOST mancante}" "${ESP_USER:?ESP_USER mancante}" "${ESP_PASS:?ESP_PASS mancante}"
KIOSK="${KIOSK:-yes}"; KIOSK_USER="${KIOSK_USER:-campane}"; CF_TOKEN="${CF_TOKEN:-}"

echo "==> 1/6  Controllo che l'ESP32 risponda su $ESP_HOST"
if curl -fs -m 6 "http://$ESP_HOST/api/status" >/dev/null; then
  echo "    OK, l'ESP32 risponde."
else
  echo "    ATTENZIONE: l'ESP32 non risponde su http://$ESP_HOST/api/status"
  echo "    Controlla l'IP in campane.conf. Continuo comunque l'installazione."
fi

echo "==> 2/6  Installo i pacchetti"
export DEBIAN_FRONTEND=noninteractive
apt-get update -y
apt-get install -y nginx curl ca-certificates
if [ "$KIOSK" = "yes" ]; then
  apt-get install -y xserver-xorg xinit x11-xserver-utils xserver-xorg-input-libinput \
    xserver-xorg-video-fbdev unclutter || true
  # browser: prova chromium, altrimenti firefox-esr
  command -v chromium >/dev/null || command -v chromium-browser >/dev/null || \
    apt-get install -y chromium || apt-get install -y chromium-browser || \
    apt-get install -y firefox-esr || echo "    Nessun browser installato: installalo a mano."
fi

echo "==> 3/6  Copio il pannello in /opt/campane"
mkdir -p /opt/campane/panel
install -m 644 "$DIR/panel/index.html" /opt/campane/panel/index.html
install -m 755 "$DIR/kiosk/start-kiosk.sh" /opt/campane/start-kiosk.sh

echo "==> 4/6  Configuro nginx"
AUTH="$(printf '%s:%s' "$ESP_USER" "$ESP_PASS" | base64 -w0)"
TPL="$(cat "$DIR/nginx/campane.conf.template")"
TPL="${TPL//@ESP_HOST@/$ESP_HOST}"
TPL="${TPL//@AUTH@/$AUTH}"
mkdir -p /var/cache/nginx/campane
printf '%s\n' "$TPL" > /etc/nginx/conf.d/campane.conf
chmod 640 /etc/nginx/conf.d/campane.conf   # contiene la password in base64
nginx -t
systemctl enable nginx
systemctl restart nginx
sleep 1
if curl -fs -m 8 http://127.0.0.1:8080/api/status >/dev/null; then
  echo "    OK: pannello e proxy funzionano (http://127.0.0.1:8080)."
else
  echo "    ATTENZIONE: il proxy non riesce a leggere lo stato dell'ESP32."
fi

echo "==> 5/6  Schermo touch (kiosk)"
if [ "$KIOSK" = "yes" ]; then
  id "$KIOSK_USER" >/dev/null 2>&1 || useradd -m -s /bin/bash "$KIOSK_USER"
  usermod -aG video,input,tty,audio "$KIOSK_USER" || true
  # permette a un utente normale di avviare Xorg dalla console
  mkdir -p /etc/X11
  printf 'allowed_users=anybody\nneeds_root_rights=yes\n' > /etc/X11/Xwrapper.config
  install -m 644 "$DIR/kiosk/campane-kiosk.service" /etc/systemd/system/campane-kiosk.service
  sed -i "s/^User=.*/User=$KIOSK_USER/" /etc/systemd/system/campane-kiosk.service
  systemctl daemon-reload
  systemctl enable campane-kiosk.service
  echo "    Kiosk installato. Parte al prossimo riavvio (oppure: sudo systemctl start campane-kiosk)."
else
  echo "    Saltato (KIOSK=no)."
fi

echo "==> 6/6  Tunnel Cloudflare"
if [ -n "$CF_TOKEN" ]; then
  case "$(uname -m)" in
    aarch64|arm64) CF_BIN="cloudflared-linux-arm64" ;;
    armv7*|armv8l) CF_BIN="cloudflared-linux-armhf" ;;
    armv6*)        CF_BIN="cloudflared-linux-arm" ;;
    x86_64)        CF_BIN="cloudflared-linux-amd64" ;;
    *) echo "    Architettura $(uname -m) non riconosciuta: installa cloudflared a mano."; CF_BIN="" ;;
  esac
  if [ -n "$CF_BIN" ]; then
    curl -fL -o /usr/local/bin/cloudflared "https://github.com/cloudflare/cloudflared/releases/latest/download/$CF_BIN"
    chmod +x /usr/local/bin/cloudflared
    /usr/local/bin/cloudflared --version
    /usr/local/bin/cloudflared service uninstall >/dev/null 2>&1 || true
    /usr/local/bin/cloudflared service install "$CF_TOKEN"
    echo "    Tunnel installato come servizio."
  fi
else
  echo "    CF_TOKEN vuoto: salto. Vedi LEGGIMI.md, passo 'Tunnel Cloudflare'."
fi

echo
echo "Fatto. Riavvia con:  sudo reboot"
