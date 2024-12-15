#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

const char* ssid = "ESP32-Access-Point";
const char* password = "123456789";
const char* authPassword = "1234";  // Passwort für die Authentifizierung

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

DNSServer dnsServer;
WebServer server(80);

bool isAuthenticated = false;
unsigned long authenticatedTimestamp = 0;  // Zeitpunkt der Authentifizierung
const unsigned long timeoutDuration = 30000;  // 30 Sekunden Timeout für Abmeldung
const int ledPin = 15;  // GPIO15 für LED

void setup() {
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW); // LED initial ausschalten

    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(ssid, password);

    dnsServer.start(53, "*", local_IP);
    setupWebServer();

    Serial.println("ESP32 Captive Portal gestartet");
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
}

void setupWebServer() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/generate_204", HTTP_GET, handleRoot);  // Android Captive Portal
    server.on("/fwlink", HTTP_GET, handleRoot);        // Windows Captive Portal
    server.on("/hotspot-detect.html", HTTP_GET, handleRoot); // macOS/iOS Captive Portal
    server.on("/authenticate", HTTP_POST, handleAuthenticate);
    server.on("/success", HTTP_GET, handleSuccessPage);
    server.on("/failed", HTTP_GET, handleFailedPage);
    server.begin();
}

void handleRoot() {
    if (isAuthenticated) {
        server.sendHeader("Location", "/success", true);
        server.send(302, "text/plain", "Redirecting...");
    } else {
        showLoginPage();
    }
}

void handleAuthenticate() {
    if (server.hasArg("password") && server.arg("password") == authPassword) {
        isAuthenticated = true;
        authenticatedTimestamp = millis();
        digitalWrite(ledPin, HIGH);  // LED einschalten
        server.sendHeader("Location", "/success", true);
        server.send(302, "text/plain", "Redirecting...");
    } else {
        server.sendHeader("Location", "/failed", true);
        server.send(302, "text/plain", "Redirecting...");
    }
}

void handleSuccessPage() {
    String page = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Success</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <script>
        setTimeout(function() {
            location.href = "/"; // Zurück zur Hauptseite nach 15 Sekunden
        }, 15000);
    </script>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin-top: 20%; color: #333; }
        h1 { font-size: 2em; }
    </style>
</head>
<body>
    <h1>Authentication Successful!</h1>
    <p>LED is ON for 15 seconds.</p>
</body>
</html>
    )";
    server.send(200, "text/html", page);
}

void handleFailedPage() {
    String page = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Failed</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <script>
        setTimeout(function() {
            location.href = "/"; // Nach 5 Sekunden zurück zur Login-Seite
        }, 5000);
    </script>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin-top: 20%; color: #721c24; background-color: #f8d7da; }
        h1 { font-size: 2em; }
    </style>
</head>
<body>
    <h1>Authentication Failed</h1>
    <p>Incorrect password. Returning to login...</p>
</body>
</html>
    )";
    server.send(200, "text/html", page);
}

void showLoginPage() {
    String page = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Login</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin-top: 20%; }
        .container { max-width: 300px; margin: auto; }
        input { padding: 10px; margin: 10px; width: 100%; box-sizing: border-box; }
        input[type="submit"] { background-color: #000; color: #fff; border: none; cursor: pointer; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Login</h1>
        <form action="/authenticate" method="POST">
            <input type="password" name="password" placeholder="Password" required>
            <input type="submit" value="Authenticate">
        </form>
    </div>
</body>
</html>
    )";
    server.send(200, "text/html", page);
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();

    // Timeout-Mechanismus für Abmeldung und LED ausschalten
    if (isAuthenticated && (millis() - authenticatedTimestamp > 15000)) {
        isAuthenticated = false;
        digitalWrite(ledPin, LOW);  // LED ausschalten
        Serial.println("LED ausgeschaltet nach 15 Sekunden.");
    }
}
