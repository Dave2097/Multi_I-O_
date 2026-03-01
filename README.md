# Multi I/O ESP8266 (PlatformIO)

Multifunktionales ESP8266 I/O-Modul mit:
- 2 Relais
- 4 digitale Eingänge inkl. Debounce + Flankenzähler
- Analog IN (A0) oder Analog OUT (PWM+RC oder MCP4725)
- Web-Dashboard + WebSocket Live-Updates
- AP-Provisioning + Setup-Mode (Taste / Token)

## Projektstruktur

- `platformio.ini`
- `src/main.cpp`
- `include/*.h`
- `src/*.cpp`
- `data/index.html, data/setup.html, data/app.js, data/styles.css`
- `data/config.json` (sealed/hardware config)
- `data/net.json` (endkunden Netzwerkdaten)

## Build / Flash / FS Upload

```bash
pio run
pio run -t upload
pio run -t uploadfs
pio device monitor
```

Hinweis: `monitor_speed` ist auf `74880` gesetzt, damit Boot- und Firmware-Logs im gleichen Takt lesbar sind.

## Setup-Ablauf

1. Kein `/net.json` mit gültiger SSID vorhanden (oder Setup-Mode aktiv) → Gerät startet AP:
   - SSID: `MF-IO-SETUP-<chipid>`
   - PASS: `setup1234`
   - IP: `192.168.4.1`
2. Browser: `http://192.168.4.1/setup`
3. WLAN wählen, Zugangsdaten eintragen, DHCP oder statisch setzen.
4. `Save & Reboot`.
5. Gerät startet STA und Dashboard unter `/`.

Wenn die STA-Verbindung trotz gespeicherter Daten nicht innerhalb von ca. 30s aufgebaut werden kann, wechselt die Firmware automatisch in den Setup-AP-Modus als Fallback.

## Setup-Mode später aktivieren

- Hardware: Setup-Taste (`io.setup_button.gpio`) mindestens 5s drücken.
- API im STA-Betrieb:

```bash
curl -X POST http://<device-ip>/api/enter-setup \
  -H 'Content-Type: application/json' \
  -d '{"token":"change-me-setup-token"}'
```

Dann ist `/setup` und Netzwerk-API freigeschaltet.

## API Beispiele

State lesen:

```bash
curl http://<device-ip>/api/state
```

Relais schalten:

```bash
curl -X POST http://<device-ip>/api/relay/1 -H 'Content-Type: application/json' -d '{"state":1}'
curl -X POST http://<device-ip>/api/relay/2 -H 'Content-Type: application/json' -d '{"state":0}'
```

Analog OUT setzen (nur bei `analog.mode="out"`):

```bash
curl -X POST http://<device-ip>/api/analog/out -H 'Content-Type: application/json' -d '{"value":2048}'
```

WLAN Scan (nur Setup-Mode/AP):

```bash
curl http://<device-ip>/api/wifi/scan
```

Netzwerk anzeigen/speichern (Passwort wird bei GET nie ausgegeben; nur Setup-Mode):

```bash
curl http://<device-ip>/api/config/network
curl -X POST http://<device-ip>/api/config/network \
  -H 'Content-Type: application/json' \
  -d '{"ssid":"MyWiFi","password":"secret","dhcp":true}'
```

## Config & Sealing

- `/config.json` enthält IO/HW/Security-Konfiguration und wird **nicht** via API geändert.
- Nach erstem validen Boot wird `/sealed.flag` geschrieben.
- `/net.json` ist endkundenänderbar (nur im Setup-Mode).

## Analog OUT Hinweise

- `pwm_rc`: PWM auf GPIO + RC-Filter als Pseudo-DAC.
- `mcp4725`: externer I2C DAC (SDA/SCL + Adresse in `config.json`).
- Wenn MCP4725 nicht erreichbar ist, meldet API/UI `analog.status="error"`.

## Boot-kritische Pins

ESP8266 hat Boot-Strapping Pins (`GPIO0`, `GPIO2`, `GPIO15`).
Die Beispielbelegung in `data/config.json` (u.a. Setup-Taste auf GPIO0, PWM auf GPIO2) ist nur ein Muster.
Passe Pinmapping für deine Hardware so an, dass der Boot nicht gestört wird.

## Troubleshooting (serieller Monitor)

Wenn der Monitor wiederholt `Invalid /config.json` zeigt und der ESP scheinbar neu startet:

1. Sicherstellen, dass LittleFS-Dateien wirklich geflasht wurden: `pio run -t uploadfs`
2. `data/config.json` auf gültiges JSON prüfen
3. Danach neu starten und den Monitor beobachten

Die Firmware geht bei Boot-Fehler jetzt in einen sicheren Idle-Modus und gibt eine klare Meldung aus, statt die Subsysteme uninitialisiert weiterlaufen zu lassen.
