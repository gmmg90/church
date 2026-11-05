#!/bin/bash
# Avvia il pannello a schermo intero. Eseguito da xinit (vedi campane-kiosk.service).
URL="http://127.0.0.1:8080/"

# schermo sempre acceso, niente salvaschermo
xset s off 2>/dev/null
xset -dpms 2>/dev/null
xset s noblank 2>/dev/null

# aspetta che nginx risponda
for i in $(seq 1 60); do
  curl -fs -o /dev/null "$URL" && break
  sleep 1
done

# nasconde il cursore (se installato)
command -v unclutter >/dev/null && unclutter -idle 0.3 -root &

run_browser() {
  if command -v chromium >/dev/null; then B=chromium
  elif command -v chromium-browser >/dev/null; then B=chromium-browser
  fi
  if [ -n "$B" ]; then
    exec "$B" --kiosk --noerrdialogs --disable-infobars --disable-session-crashed-bubble \
      --no-first-run --disable-translate --disable-pinch --overscroll-history-navigation=0 \
      --user-data-dir="$HOME/.chromium-kiosk" --disk-cache-dir=/tmp/chromium-cache \
      --renderer-process-limit=1 --disable-dev-shm-usage --touch-events=enabled "$URL"
  fi
  if command -v firefox-esr >/dev/null; then exec firefox-esr --kiosk "$URL"; fi
  if command -v firefox >/dev/null; then exec firefox --kiosk "$URL"; fi
  if command -v midori >/dev/null; then exec midori -e Fullscreen "$URL"; fi
  echo "Nessun browser trovato (chromium / firefox-esr / midori)" >&2
  sleep 30
}

# se il browser si chiude o si blocca, lo riapre
while true; do
  ( run_browser )
  sleep 3
done
