// RadioLib POCSAG Transmitter för ESP32-Stick-PoE-P med webbgränssnitt
// Stöd för både PoE och WiFi
//
// VIKTIGT: I Arduino IDE, välj:
// Board: "Olimex ESP32-POE-ISO" eller "ESP32 Dev Module"
// Under "Tools" -> "Board" -> "ESP32 Arduino"
//
// Om du saknar ESP32-stöd, lägg till i Board Manager:
// File -> Preferences -> Additional Board Manager URLs:
// https://espressif.github.io/arduino-esp32/package_esp32_index.json

#include <RadioLib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <ETH.h>

// CC1101 anslutningar för ESP32-Stick-PoE-P
// Välj GPIO-pinnar som passar dina behov från tillgängliga:
// IO5, IO16, IO4, IO0, IO2, IO15, IO13, IO12, IO14, IO33, IO35, IO34
//
// VIKTIGT: ESP32 använder VSPI som standard:
// VSPI: SCK=18, MISO=19, MOSI=23, CS=5 (standard)
// Men dessa pinnar kanske inte är tillgängliga på ESP32-Stick-PoE-P
//
// Vi använder HSPI istället med följande pinnar:
// HSPI: SCK=14, MISO=12, MOSI=13, CS=15
#define CC1101_CS    15   // CS/NSS pin
#define CC1101_GDO0  4    // GDO0 pin (interrupt)
#define CC1101_GDO2  2    // GDO2 pin

// SPI pinnar för HSPI på ESP32
#define CC1101_SCK   14   // SCK
#define CC1101_MISO  12   // MISO
#define CC1101_MOSI  13   // MOSI

CC1101 radio = new Module(CC1101_CS, CC1101_GDO0, RADIOLIB_NC, CC1101_GDO2);
PagerClient pager(&radio);

// LED (kan användas för status)
#define LED_PIN 5

// PTT (Push-To-Talk) pin för externt slutsteg
#define PTT_PIN 33
#define PTT_PRE_DELAY 500   // ms före sändning
#define PTT_POST_DELAY 500  // ms efter sändning

// Webserver
WebServer server(80);

// Preferences för att spara inställningar
Preferences preferences;

// Konfigurationsvariabler
String deviceHostname = "pocsag-tx";
String adminPassword = "password";
bool useWiFiBackup = false;  // WiFi används bara om LAN misslyckas
String wifiSSID = "";
String wifiPassword = "";
long defaultRIC = 123456;

// Access Point inställningar
const char* apSSID = "PocsagTX-Setup";
const char* apPassword = "Password";
bool apMode = false;
bool usingEthernet = false;

// Status variabler
bool radioInitialized = false;
String lastMessage = "";
String lastStatus = "";

// HTML sidor
const char* htmlHeader = R"rawliteral(
<!DOCTYPE html>
<html lang='sv'>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>POCSAG Sändare</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 50px auto;
            padding: 20px;
            background-color: #f0f0f0;
        }
        .container {
            background-color: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            text-align: center;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
            color: #555;
        }
        input[type='text'], input[type='password'], input[type='number'], textarea {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 5px;
            box-sizing: border-box;
            font-size: 14px;
        }
        textarea {
            resize: vertical;
            min-height: 80px;
        }
        button {
            background-color: #4CAF50;
            color: white;
            padding: 12px 30px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 16px;
            width: 100%;
            margin-top: 10px;
        }
        button:hover {
            background-color: #45a049;
        }
        .settings-btn {
            background-color: #2196F3;
        }
        .settings-btn:hover {
            background-color: #0b7dda;
        }
        .status {
            padding: 15px;
            margin: 20px 0;
            border-radius: 5px;
            text-align: center;
        }
        .success {
            background-color: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .error {
            background-color: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .info {
            background-color: #d1ecf1;
            color: #0c5460;
            border: 1px solid #bee5eb;
        }
        .nav-links {
            text-align: center;
            margin-top: 20px;
        }
        .nav-links a {
            color: #2196F3;
            text-decoration: none;
            margin: 0 15px;
        }
        .nav-links a:hover {
            text-decoration: underline;
        }
        .help-text {
            font-size: 12px;
            color: #666;
            margin-top: 5px;
        }
    </style>
</head>
<body>
    <div class='container'>
)rawliteral";

const char* htmlFooter = R"rawliteral(
    </div>
</body>
</html>
)rawliteral";

// Huvudsida för att skicka meddelanden
void handleRoot() {
  if (!server.authenticate("admin", adminPassword.c_str())) {
    return server.requestAuthentication();
  }

  String html = htmlHeader;
  html += "<h1>📡 POCSAG Sändare</h1>";
  
  // Varning om radio inte är initierad
  if (!radioInitialized) {
    html += "<div class='status error'>";
    html += "⚠️ CC1101-modul ej ansluten eller hittad!<br>";
    html += "Anslut CC1101 och starta om enheten för att kunna skicka meddelanden.";
    html += "</div>";
  }
  
  if (lastStatus != "") {
    html += "<div class='status ";
    html += lastStatus.startsWith("SUCCESS") ? "success" : "error";
    html += "'>" + lastStatus + "</div>";
  }
  
  html += "<form action='/send' method='POST'>";
  html += "<div class='form-group'>";
  html += "<label for='ric'>RIC Nummer:</label>";
  html += "<input type='number' id='ric' name='ric' value='" + String(defaultRIC) + "' min='1' max='2097151' required";
  if (!radioInitialized) html += " disabled";
  html += ">";
  html += "<div class='help-text'>POCSAG RIC (1-2097151)</div>";
  html += "</div>";
  
  html += "<div class='form-group'>";
  html += "<label for='message'>Meddelande:</label>";
  html += "<textarea id='message' name='message' maxlength='80' required placeholder='Skriv ditt meddelande här...'";
  if (!radioInitialized) html += " disabled";
  html += "></textarea>";
  html += "<div class='help-text'>Max 80 tecken</div>";
  html += "</div>";
  
  html += "<button type='submit'";
  if (!radioInitialized) html += " disabled style='background-color: #ccc; cursor: not-allowed;'";
  html += ">📤 Skicka Meddelande</button>";
  html += "</form>";
  
  html += "<div class='nav-links'>";
  html += "<a href='/settings'>⚙️ Inställningar</a> | ";
  html += "<a href='/status'>📊 Status</a>";
  html += "</div>";
  
  html += htmlFooter;
  server.send(200, "text/html", html);
}

// Hantera meddelandesändning
void handleSend() {
  if (!server.authenticate("admin", adminPassword.c_str())) {
    return server.requestAuthentication();
  }

  // Kontrollera att radio är initierad
  if (!radioInitialized) {
    lastStatus = "ERROR: CC1101-modul ej ansluten. Kan inte skicka meddelanden.";
    server.sendHeader("Location", "/");
    server.send(303);
    return;
  }

  if (server.hasArg("ric") && server.hasArg("message")) {
    long ricNumber = server.arg("ric").toInt();
    String message = server.arg("message");
    
    // Validera RIC
    if (ricNumber <= 0 || ricNumber > 2097151) {
      lastStatus = "ERROR: Ogiltigt RIC-nummer (måste vara 1-2097151)";
      server.sendHeader("Location", "/");
      server.send(303);
      return;
    }
    
    // Validera meddelande
    if (message.length() == 0) {
      lastStatus = "ERROR: Meddelandet kan inte vara tomt";
      server.sendHeader("Location", "/");
      server.send(303);
      return;
    }
    
    if (message.length() > 80) {
      message = message.substring(0, 80);
    }
    
    // Aktivera PTT för externt slutsteg
    Serial.println("PTT ON - Aktiverar externt slutsteg");
    digitalWrite(PTT_PIN, HIGH);
    digitalWrite(LED_PIN, LOW);
    delay(PTT_PRE_DELAY);  // Vänta 500ms innan sändning
    
    // Skicka meddelande
    Serial.printf("Skickar: '%s' till RIC %ld\n", message.c_str(), ricNumber);
    
    int state = pager.transmit(message.c_str(), ricNumber, RADIOLIB_PAGER_ASCII);
    
    // Vänta efter sändning innan PTT släpps
    delay(PTT_POST_DELAY);  // Vänta 500ms efter sändning
    digitalWrite(PTT_PIN, LOW);
    Serial.println("PTT OFF - Stänger av externt slutsteg");
    
    if (state == RADIOLIB_ERR_NONE) {
      digitalWrite(LED_PIN, HIGH);
      lastStatus = "SUCCESS: Meddelandet skickat till RIC " + String(ricNumber);
      lastMessage = message;
      Serial.println("Meddelande skickat!");
    } else {
      lastStatus = "ERROR: Kunde inte skicka (felkod " + String(state) + ")";
      Serial.printf("Sändning misslyckades, kod %d\n", state);
    }
    
    delay(100);
    digitalWrite(LED_PIN, LOW);
  } else {
    lastStatus = "ERROR: Saknar RIC eller meddelande";
  }
  
  server.sendHeader("Location", "/");
  server.send(303);
}

// HTTP API för meddelandesändning (GET eller POST)
// Användning: http://pocsag-tx.local/api/send?ric=123456&message=Hello
void handleAPISend() {
  // Kräv autentisering
  if (!server.authenticate("admin", adminPassword.c_str())) {
    server.send(401, "application/json", "{\"status\":\"error\",\"message\":\"Unauthorized\"}");
    return;
  }

  // Kontrollera att radio är initierad
  if (!radioInitialized) {
    server.send(503, "application/json", 
      "{\"status\":\"error\",\"message\":\"CC1101 module not connected or initialized\"}");
    return;
  }

  // Kontrollera parametrar
  if (!server.hasArg("ric") || !server.hasArg("message")) {
    server.send(400, "application/json", 
      "{\"status\":\"error\",\"message\":\"Missing parameters. Use: /api/send?ric=123456&message=YourMessage\"}");
    return;
  }

  long ricNumber = server.arg("ric").toInt();
  String message = server.arg("message");
  
  // Validera RIC
  if (ricNumber <= 0 || ricNumber > 2097151) {
    server.send(400, "application/json", 
      "{\"status\":\"error\",\"message\":\"Invalid RIC number (must be 1-2097151)\"}");
    return;
  }
  
  // Validera meddelande
  if (message.length() == 0) {
    server.send(400, "application/json", 
      "{\"status\":\"error\",\"message\":\"Message cannot be empty\"}");
    return;
  }
  
  bool wasTruncated = false;
  if (message.length() > 80) {
    message = message.substring(0, 80);
    wasTruncated = true;
  }
  
  // Aktivera PTT för externt slutsteg
  Serial.println("API: PTT ON - Aktiverar externt slutsteg");
  digitalWrite(PTT_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);
  delay(PTT_PRE_DELAY);  // Vänta 500ms innan sändning
  
  // Skicka meddelande
  Serial.printf("API: Skickar '%s' till RIC %ld\n", message.c_str(), ricNumber);
  
  int state = pager.transmit(message.c_str(), ricNumber, RADIOLIB_PAGER_ASCII);
  
  // Vänta efter sändning innan PTT släpps
  delay(PTT_POST_DELAY);  // Vänta 500ms efter sändning
  digitalWrite(PTT_PIN, LOW);
  Serial.println("API: PTT OFF - Stänger av externt slutsteg");
  
  String jsonResponse;
  if (state == RADIOLIB_ERR_NONE) {
    digitalWrite(LED_PIN, HIGH);
    lastMessage = message;
    jsonResponse = "{\"status\":\"success\",\"message\":\"Message sent\",\"ric\":" + 
                   String(ricNumber) + ",\"text\":\"" + message + "\"";
    if (wasTruncated) {
      jsonResponse += ",\"truncated\":true";
    }
    jsonResponse += "}";
    Serial.println("API: Meddelande skickat!");
    server.send(200, "application/json", jsonResponse);
  } else {
    jsonResponse = "{\"status\":\"error\",\"message\":\"Transmission failed\",\"error_code\":" + 
                   String(state) + "}";
    Serial.printf("API: Sändning misslyckades, kod %d\n", state);
    server.send(500, "application/json", jsonResponse);
  }
  
  delay(100);
  digitalWrite(LED_PIN, LOW);
}

// Inställningssida
void handleSettings() {
  if (!server.authenticate("admin", adminPassword.c_str())) {
    return server.requestAuthentication();
  }

  String html = htmlHeader;
  html += "<h1>⚙️ Inställningar</h1>";
  
  if (lastStatus.startsWith("SETTINGS")) {
    html += "<div class='status success'>" + lastStatus + "</div>";
    lastStatus = "";
  }
  
  html += "<form action='/save-settings' method='POST'>";
  
  html += "<h2>Enhetsidentitet</h2>";
  html += "<div class='form-group'>";
  html += "<label for='hostname'>Hostname:</label>";
  html += "<input type='text' id='hostname' name='hostname' value='" + deviceHostname + "' required>";
  html += "<div class='help-text'>Enhetsnamn på nätverket (t.ex. pocsag-tx.local)</div>";
  html += "</div>";
  
  html += "<div class='form-group'>";
  html += "<label for='password'>Admin Lösenord:</label>";
  html += "<input type='password' id='password' name='password' value='" + adminPassword + "' required>";
  html += "</div>";
  
  html += "<h2>Standardvärden</h2>";
  html += "<div class='form-group'>";
  html += "<label for='default_ric'>Standard RIC:</label>";
  html += "<input type='number' id='default_ric' name='default_ric' value='" + String(defaultRIC) + "' min='1' max='2097151' required>";
  html += "</div>";
  
  html += "<h2>Nätverksinställningar</h2>";
  html += "<div class='info'>";
  html += "<strong>Primär anslutning:</strong> Ethernet/LAN (DHCP)<br>";
  html += "Enheten försöker alltid ansluta via Ethernet först.";
  html += "</div>";
  
  html += "<h3>WiFi Backup</h3>";
  html += "<div class='form-group'>";
  html += "<label for='use_wifi_backup'>Aktivera WiFi backup (om Ethernet misslyckas):</label>";
  html += "<input type='checkbox' id='use_wifi_backup' name='use_wifi_backup' " + String(useWiFiBackup ? "checked" : "") + ">";
  html += "</div>";
  
  html += "<div class='form-group'>";
  html += "<label for='wifi_ssid'>WiFi SSID:</label>";
  html += "<input type='text' id='wifi_ssid' name='wifi_ssid' value='" + wifiSSID + "'>";
  html += "<div class='help-text'>Används endast om Ethernet misslyckas</div>";
  html += "</div>";
  
  html += "<div class='form-group'>";
  html += "<label for='wifi_pass'>WiFi Lösenord:</label>";
  html += "<input type='password' id='wifi_pass' name='wifi_pass' value='" + wifiPassword + "'>";
  html += "</div>";
  
  html += "<button type='submit' class='settings-btn'>💾 Spara Inställningar</button>";
  html += "</form>";
  
  html += "<div class='nav-links'>";
  html += "<a href='/'>🏠 Hem</a> | ";
  html += "<a href='/restart'>🔄 Starta om</a>";
  html += "</div>";
  
  html += htmlFooter;
  server.send(200, "text/html", html);
}

// Spara inställningar
void handleSaveSettings() {
  if (!server.authenticate("admin", adminPassword.c_str())) {
    return server.requestAuthentication();
  }

  bool needsRestart = false;

  if (server.hasArg("hostname")) {
    String newHostname = server.arg("hostname");
    if (newHostname != deviceHostname) {
      deviceHostname = newHostname;
      preferences.putString("hostname", deviceHostname);
      needsRestart = true;
    }
  }

  if (server.hasArg("password")) {
    adminPassword = server.arg("password");
    preferences.putString("password", adminPassword);
  }

  if (server.hasArg("default_ric")) {
    defaultRIC = server.arg("default_ric").toInt();
    preferences.putLong("defaultRIC", defaultRIC);
  }

  bool newUseWiFiBackup = server.hasArg("use_wifi_backup");
  if (newUseWiFiBackup != useWiFiBackup) {
    useWiFiBackup = newUseWiFiBackup;
    preferences.putBool("useWiFiBackup", useWiFiBackup);
    needsRestart = true;
  }

  if (server.hasArg("wifi_ssid")) {
    wifiSSID = server.arg("wifi_ssid");
    preferences.putString("wifiSSID", wifiSSID);
    if (useWiFiBackup) needsRestart = true;
  }

  if (server.hasArg("wifi_pass")) {
    wifiPassword = server.arg("wifi_pass");
    preferences.putString("wifiPass", wifiPassword);
    if (useWiFiBackup) needsRestart = true;
  }

  lastStatus = needsRestart ? 
    "SETTINGS: Inställningar sparade! Starta om enheten för att aktivera ändringar." :
    "SETTINGS: Inställningar sparade!";

  server.sendHeader("Location", "/settings");
  server.send(303);
}

// Statussida
void handleStatus() {
  if (!server.authenticate("admin", adminPassword.c_str())) {
    return server.requestAuthentication();
  }

  String html = htmlHeader;
  html += "<h1>📊 Systemstatus</h1>";
  
  html += "<div class='info'>";
  html += "<strong>Hostname:</strong> " + deviceHostname + ".local<br>";
  
  if (apMode) {
    html += "<strong>Läge:</strong> Access Point (Setup-läge)<br>";
    html += "<strong>AP SSID:</strong> " + String(apSSID) + "<br>";
    html += "<strong>AP IP:</strong> " + WiFi.softAPIP().toString() + "<br>";
    html += "<strong>Anslutna klienter:</strong> " + String(WiFi.softAPgetStationNum()) + "<br>";
  } else if (usingEthernet) {
    html += "<strong>Anslutningstyp:</strong> Ethernet/LAN (DHCP)<br>";
    html += "<strong>IP-adress:</strong> " + ETH.localIP().toString() + "<br>";
    html += "<strong>Gateway:</strong> " + ETH.gatewayIP().toString() + "<br>";
    html += "<strong>DNS:</strong> " + ETH.dnsIP().toString() + "<br>";
    html += "<strong>MAC-adress:</strong> " + ETH.macAddress() + "<br>";
    html += "<strong>Link Speed:</strong> " + String(ETH.linkSpeed()) + " Mbps<br>";
    html += "<strong>Full Duplex:</strong> " + String(ETH.fullDuplex() ? "Ja" : "Nej") + "<br>";
  } else {
    html += "<strong>Anslutningstyp:</strong> WiFi (Backup)<br>";
    html += "<strong>IP-adress:</strong> " + WiFi.localIP().toString() + "<br>";
    html += "<strong>WiFi SSID:</strong> " + wifiSSID + "<br>";
    html += "<strong>Signal:</strong> " + String(WiFi.RSSI()) + " dBm<br>";
  }
  
  html += "<strong>WiFi Backup:</strong> " + String(useWiFiBackup ? "Aktiverad" : "Inaktiverad") + "<br>";
  html += "<strong>Radio status:</strong> " + String(radioInitialized ? "Initierad ✓" : "Ej ansluten ✗") + "<br>";
  
  if (radioInitialized) {
    html += "<strong>Frekvens:</strong> 433.92 MHz<br>";
    html += "<strong>Effekt:</strong> +10 dBm (~10mW)<br>";
  }
  
  html += "<strong>Senaste meddelande:</strong> " + (lastMessage != "" ? lastMessage : "Inget") + "<br>";
  html += "<strong>Upptid:</strong> " + String(millis() / 1000) + " sekunder<br>";
  html += "<strong>Fritt minne:</strong> " + String(ESP.getFreeHeap() / 1024) + " KB<br>";
  html += "<br><strong>API Endpoint:</strong> /api/send?ric=XXXXX&message=TEXT";
  html += "</div>";
  
  html += "<div class='nav-links'>";
  html += "<a href='/'>🏠 Hem</a>";
  html += "</div>";
  
  html += htmlFooter;
  server.send(200, "text/html", html);
}

// Starta om enhet
void handleRestart() {
  if (!server.authenticate("admin", adminPassword.c_str())) {
    return server.requestAuthentication();
  }

  String html = htmlHeader;
  html += "<h1>🔄 Startar om...</h1>";
  html += "<div class='info'>Enheten startar om. Vänta 10 sekunder och ladda sedan om sidan.</div>";
  html += "<script>setTimeout(function(){ window.location.href = '/'; }, 10000);</script>";
  html += htmlFooter;
  
  server.send(200, "text/html", html);
  delay(1000);
  ESP.restart();
}

void loadSettings() {
  preferences.begin("pocsag-config", false);
  
  deviceHostname = preferences.getString("hostname", "pocsag-tx");
  adminPassword = preferences.getString("password", "admin123");
  defaultRIC = preferences.getLong("defaultRIC", 123456);
  useWiFiBackup = preferences.getBool("useWiFiBackup", false);
  wifiSSID = preferences.getString("wifiSSID", "");
  wifiPassword = preferences.getString("wifiPass", "");
  
  Serial.println("Inställningar laddade:");
  Serial.println("Hostname: " + deviceHostname);
  Serial.println("WiFi Backup: " + String(useWiFiBackup ? "Aktiverad" : "Inaktiverad"));
  if (useWiFiBackup && wifiSSID.length() > 0) {
    Serial.println("WiFi SSID: " + wifiSSID);
  }
}

// Ethernet event handler
void onEthEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      ETH.setHostname(deviceHostname.c_str());
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.println("ETH Got IP");
      Serial.print("IP: ");
      Serial.println(ETH.localIP());
      Serial.print("Gateway: ");
      Serial.println(ETH.gatewayIP());
      Serial.print("DNS: ");
      Serial.println(ETH.dnsIP());
      usingEthernet = true;
      apMode = false;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      usingEthernet = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      usingEthernet = false;
      break;
    default:
      break;
  }
}

void setupNetwork() {
  Serial.println("\n=== NÄTVERKSKONFIGURATION ===");
  
  // Registrera Ethernet event handler
  WiFi.onEvent(onEthEvent);
  
  // Försök starta Ethernet (primär anslutning)
  Serial.println("Försöker ansluta via Ethernet/LAN (DHCP)...");
  
  // För ESP32-Stick-PoE-P / ESP32-Stick-Eth (allexoK/Prokyber)
  // Baserat på officiella exempel och ESPHome-konfiguration:
  // PHY Type: LAN8720A
  // PHY Address: 1 (VIKTIGT: Inte 0!)
  // MDC: GPIO23
  // MDIO: GPIO18
  // Clock: GPIO17, mode CLK_OUT
  // Power: Ingen power pin behövs
  
  Serial.println("Konfigurerar för ESP32-Stick-PoE-P (allexoK)...");
  
  if (ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, -1, ETH_CLOCK_GPIO17_OUT)) {
    Serial.println("Ethernet initierad, väntar på IP från DHCP...");
    
    // Vänta max 10 sekunder på IP
    int ethTimeout = 0;
    while (!usingEthernet && ethTimeout < 40) {  // 40 * 250ms = 10 sekunder
      delay(250);
      Serial.print(".");
      ethTimeout++;
    }
    Serial.println();
    
    if (usingEthernet) {
      Serial.println("✓ Ethernet ansluten via DHCP!");
      Serial.print("IP-adress: ");
      Serial.println(ETH.localIP());
    } else {
      Serial.println("✗ Fick ingen IP från DHCP på Ethernet");
      Serial.println("Kontrollera:");
      Serial.println("- Att nätverkskabeln är inkopplad och fungerar");
      Serial.println("- Att routern/switchen tilldelar IP via DHCP");
      Serial.println("- Att link-ljuset på RJ45-porten lyser");
    }
  } else {
    Serial.println("✗ Kunde inte initiera Ethernet");
    Serial.println("Möjliga orsaker:");
    Serial.println("- Nätverkskabel saknas eller trasig");
    Serial.println("- Fel board-typ vald (ska vara ESP32 Dev Module eller Olimex ESP32-POE-ISO)");
    Serial.println("- Hårdvaruproblem med LAN8720A-chip");
  }
  
  // Om Ethernet misslyckades, försök WiFi backup
  if (!usingEthernet) {
    if (useWiFiBackup && wifiSSID.length() > 0) {
      Serial.println("\nEthernet misslyckades, försöker WiFi backup...");
      WiFi.mode(WIFI_STA);
      WiFi.setHostname(deviceHostname.c_str());
      WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
      
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
      }
      Serial.println();
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✓ WiFi backup ansluten!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        apMode = false;
      } else {
        Serial.println("✗ WiFi backup misslyckades");
        Serial.println("\nIngen nätverksanslutning! Startar Access Point...");
        startAccessPoint();
      }
    } else {
      // Ingen WiFi backup konfigurerad
      Serial.println("\nIngen WiFi backup konfigurerad.");
      Serial.println("Startar Access Point för konfiguration...");
      startAccessPoint();
    }
  }
  
  // Starta mDNS (fungerar för både Ethernet och WiFi)
  if (!apMode) {
    if (MDNS.begin(deviceHostname.c_str())) {
      Serial.println("mDNS startad: " + deviceHostname + ".local");
      MDNS.addService("http", "tcp", 80);
    }
  }
  
  Serial.println("==============================\n");
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword);
  apMode = true;
  usingEthernet = false;
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("\n=== ACCESS POINT AKTIVERAT ===");
  Serial.println("SSID: " + String(apSSID));
  Serial.println("Lösenord: " + String(apPassword));
  Serial.println("IP-adress: " + IP.toString());
  Serial.println("Öppna webbläsaren på: http://" + IP.toString());
  Serial.println("Användarnamn: admin");
  Serial.println("Lösenord: " + adminPassword);
  Serial.println("===============================\n");
}

void setupRadio() {
  // Initiera SPI med anpassade pinnar för HSPI
  Serial.print(F("[SPI] Initierar HSPI med SCK="));
  Serial.print(CC1101_SCK);
  Serial.print(F(", MISO="));
  Serial.print(CC1101_MISO);
  Serial.print(F(", MOSI="));
  Serial.print(CC1101_MOSI);
  Serial.println(F("..."));
  
  SPI.begin(CC1101_SCK, CC1101_MISO, CC1101_MOSI, CC1101_CS);
  delay(100);
  
  Serial.print(F("[CC1101] Initierar... "));
  int state = radio.begin();

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Lyckades!"));
    
    // Initiera Pager client
    Serial.print(F("[Pager] Initierar... "));
    state = pager.begin(433.92, 1200);
    
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("Lyckades!"));
      
      // Sätt maximal effekt
      Serial.print(F("[CC1101] Sätter maximal effekt... "));
      state = radio.setOutputPower(10);
      
      if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("MAX EFFEKT (+10 dBm)"));
        radioInitialized = true;
      } else {
        Serial.print(F("Effektinställning misslyckades, kod "));
        Serial.println(state);
        radioInitialized = false;
      }
    } else {
      Serial.print(F("Pager initiering misslyckades, kod "));
      Serial.println(state);
      radioInitialized = false;
    }
  } else {
    Serial.print(F("Misslyckades, kod "));
    Serial.println(state);
    Serial.println(F("\n⚠️  CC1101-modul hittades inte!"));
    Serial.println(F("Webbservern startar ändå för konfiguration."));
    Serial.println(F("\nNär du är redo att ansluta CC1101:"));
    Serial.println(F("   CS   -> GPIO15"));
    Serial.println(F("   SCK  -> GPIO14"));
    Serial.println(F("   MISO -> GPIO12"));
    Serial.println(F("   MOSI -> GPIO13"));
    Serial.println(F("   GDO0 -> GPIO4"));
    Serial.println(F("   GDO2 -> GPIO2"));
    Serial.println(F("   VCC  -> 3.3V"));
    Serial.println(F("   GND  -> GND"));
    Serial.println(F("Starta sedan om enheten.\n"));
    radioInitialized = false;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nRadioLib POCSAG Sändare för ESP32-Stick-PoE-P");
  Serial.println("==============================================");
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initiera PTT pin för externt slutsteg
  pinMode(PTT_PIN, OUTPUT);
  digitalWrite(PTT_PIN, LOW);
  Serial.println("PTT pin (GPIO34) initierad för externt slutsteg");
  
  // Ladda inställningar
  loadSettings();
  
  // Konfigurera nätverk
  setupNetwork();
  
  // Initiera radio
  setupRadio();
  
  // Konfigurera webbserver
  server.on("/", handleRoot);
  server.on("/send", HTTP_POST, handleSend);
  server.on("/api/send", HTTP_ANY, handleAPISend);  // API endpoint - GET eller POST
  server.on("/settings", handleSettings);
  server.on("/save-settings", HTTP_POST, handleSaveSettings);
  server.on("/status", handleStatus);
  server.on("/restart", handleRestart);
  
  server.begin();
  Serial.println("Webbserver startad!");
  
  if (apMode) {
    Serial.println("Öppna http://192.168.4.1 i din webbläsare");
  } else {
    Serial.println("Öppna http://" + deviceHostname + ".local i din webbläsare");
    Serial.print("Eller använd IP: ");
    if (usingEthernet) {
      Serial.println(ETH.localIP());
    } else {
      Serial.println(WiFi.localIP());
    }
  }
  
  Serial.println("\n=== HTTP API ===");
  Serial.println("Endpoint: /api/send");
  Serial.println("Parametrar: ric=XXXXX&message=TEXT");
  Serial.println("Exempel: http://" + deviceHostname + ".local/api/send?ric=123456&message=Hello");
  Serial.println("Autentisering krävs: admin / " + adminPassword);
  Serial.println("================\n");
  
  Serial.println("Användarnamn: admin");
  Serial.println("Lösenord: " + adminPassword);
}

void loop() {
  server.handleClient();
  delay(2);
}
