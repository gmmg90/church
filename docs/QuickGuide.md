# Guida Rapida — Controller Campane Chiesa

Questa è una guida A4 pronta da stampare con i passaggi essenziali.

## Accesso Rapido
- IP: mostra su display o in testata UI
- AP di configurazione: SSID `ChurchBells-Config`, Password `campanile123`
- Interfaccia web: `http://<IP-dispositivo>`

## Comandi Principali
- Abilita/Disabilita campane: scheda Stato
- STOP di emergenza: pulsante rosso
- Test relè: scheda Relè (ON/OFF e test durata)
- Riproduci melodia: scheda Melodie (seleziona → Riproduci)
- Imposta ora: scheda Orario (datetime-local)
- Resync NTP: scheda Impostazioni

## Diagnostica
- /api/status: stato completo
- /api/i2c-scan: scan I2C (0x68 = RTC)
- /api/relay-status: livelli pin relè/LED

## Aggiornamenti (opzionali)
- Firmware: PlatformIO Upload
- UI (SPIFFS): PlatformIO Upload Filesystem Image

## Cablaggio (sintesi)
- RTC: SDA=21, SCL=22, 3.3V, GND
- Relè: IN1=25, IN2=26 (active-low)

## Note di Sicurezza
- Relè active-low: ON = livello LOW (0)
- Verifica sempre cablaggio e alimentazione
- Cambia le password in produzione
