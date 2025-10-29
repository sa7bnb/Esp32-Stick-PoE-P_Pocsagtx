ESP32-Stick-PoE-P POCSAG Transmitter
Ett komplett POCSAG-sändarsystem med webbgränssnitt för ESP32-Stick-PoE-P, byggt med RadioLib och CC1101 RF-modul.
🚀 Funktioner
Nätverk

Ethernet (LAN) primär anslutning - Automatisk DHCP-konfiguration
WiFi backup - Aktiveras automatiskt om Ethernet misslyckas
Access Point fallback - Setup-läge (SSID: PocsagTX-Setup, lösenord: Password)
mDNS support - Åtkomst via hostname.local

Webbgränssnitt

Lösenordsskyddat - HTTP Basic Authentication
Skicka POCSAG-meddelanden - Ange RIC (1-2097151) och meddelande (max 80 tecken)
Konfigurerbart - Hostname, lösenord, WiFi-inställningar, standard RIC
Statussida - Nätverksinfo, systemstatus, upptid

HTTP API
GET/POST: http://ip-adress/api/send?ric=123456&message=Hello%20World
Autentisering: admin / ditt-lösenord
Svar: JSON
```

### Radio
- **CC1101 RF-modul** på 433.92 MHz
- **POCSAG 1200 bps** alphanumeriskt läge
- **+10 dBm output** (~10mW)
- **PTT-kontroll** - GPIO33 aktiveras 500ms före/efter sändning för externt slutsteg

## 🔌 Hårdvara

### ESP32-Stick-PoE-P
- Ethernet: LAN8720A PHY
- Strömförsörjning: PoE (9-57V) eller USB

### CC1101 Anslutning
```
CC1101   →  ESP32-Stick-PoE-P
CS       →  GPIO15
SCK      →  GPIO14
MISO     →  GPIO12
MOSI     →  GPIO13
GDO0     →  GPIO4
GDO2     →  GPIO2
VCC      →  3.3V
GND      →  GND
```

### PTT Output
```
GPIO33   →  Externt slutsteg (PTT Input)
GND      →  GND

Signal: 3.3V logik, aktiv HÖG
Timing: 500ms pre-TX + sändningstid + 500ms post-TX

📦 Bibliotek

RadioLib
WiFi (inbyggd)
WebServer (inbyggd)
Preferences (inbyggd)
ESPmDNS (inbyggd)
SPI (inbyggd)
ETH (inbyggd)

🛠️ Installation

Installera ESP32 board support i Arduino IDE:

https://espressif.github.io/arduino-esp32/package_esp32_index.json


Välj board: "Olimex ESP32-POE-ISO" eller "ESP32 Dev Module"
Installera RadioLib bibliotek
Ladda upp kod
Anslut Ethernet-kabel
Öppna http://pocsag-tx.local eller använd IP-adressen

🔐 Standardinställningar

Hostname: pocsag-tx
Användarnamn: admin
Lösenord: Password (byt i inställningarna!)
Standard RIC: 123456
Frekvens: 433.92 MHz
Hastighet: 1200 bps

📡 Användning
Via Webbläsare

Gå till http://pocsag-tx.local eller IP-adressen
Logga in med admin-uppgifter
Ange RIC-nummer och meddelande
Klicka "Skicka Meddelande"

Via HTTP API (curl exempel)
bashcurl -u admin:Password "http://pocsag-tx.local/api/send?ric=123456&message=Hello%20World"
API Svar
json{
  "status": "success",
  "message": "Message sent",
  "ric": 123456,
  "text": "Hello World"
}
⚙️ Konfiguration

Hostname - För mDNS (hostname.local)
Admin-lösenord - För webbgränssnitt och API
WiFi backup - SSID och lösenord för reservanslutning
Standard RIC - Förvalt RIC-nummer

Alla inställningar sparas i ESP32:s EEPROM och bevaras vid omstart.

// SA7BNB
