#include <WiFi.h>
#include <WebServer.h>
#include <DHTesp.h>

const char* ssid = "LB_ADSL_YUYU";
const char* password = "1965@1970";

DHTesp dht;
const int DHT_PIN = 15;
const int PIR_PIN = 18;
const int LED_PIN = 2;  // LED intégrée de l'ESP32
const unsigned long SENSOR_READ_INTERVAL = 2000;

// Variables pour les statistiques
unsigned long pirDetectionCount = 0;
unsigned long lastPirDetection = 0;
unsigned long ledOnTime = 0;
unsigned long ledOnCount = 0;
bool lastPirState = false;
bool ledState = false;
unsigned long ledLastStateChange = 0;

WebServer server(80);
unsigned long lastSensorRead = 0;
float temperature = 0;
float humidity = 0;
bool pirState = false;

void setup() {
  Serial.begin(115200);
  dht.setup(DHT_PIN, DHTesp::DHT22);
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connexion au WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnecté au WiFi");
  Serial.print("Adresse IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/toggle", handleLedToggle);
  server.on("/stats", handleStats);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Serveur HTTP démarré");
}

void loop() {
  server.handleClient();
  
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
    readSensors();
    updateStats();
    lastSensorRead = currentMillis;
  }
}

void readSensors() {
  TempAndHumidity data = dht.getTempAndHumidity();
  if (!dht.getStatus()) {
    temperature = data.temperature;
    humidity = data.humidity;
  }
  pirState = digitalRead(PIR_PIN);
}

void updateStats() {
  // Mise à jour des statistiques PIR
  if (pirState && !lastPirState) {
    pirDetectionCount++;
    lastPirDetection = millis();
  }
  lastPirState = pirState;

  // Mise à jour du temps d'allumage LED
  if (ledState) {
    ledOnTime += SENSOR_READ_INTERVAL;
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>ESP32 Control Panel</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f0f0f0; }";
  html += ".container { max-width: 800px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }";
  html += ".sensor-value { font-size: 24px; margin: 10px 0; padding: 15px; border-radius: 5px; background-color: #f8f9fa; }";
  html += ".button { background-color: #4CAF50; border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 4px 2px; cursor: pointer; border-radius: 4px; }";
  html += ".button.off { background-color: #f44336; }";
  html += ".stats { margin-top: 20px; padding: 15px; background-color: #e9ecef; border-radius: 5px; }";
  html += "</style>";
  html += "<script>";
  html += "function updateData() {";
  html += "  fetch('/data').then(response => response.json()).then(data => {";
  html += "    document.getElementById('temperature').textContent = data.temperature.toFixed(1) + ' °C';";
  html += "    document.getElementById('humidity').textContent = data.humidity.toFixed(1) + ' %';";
  html += "    document.getElementById('pir').textContent = data.motion ? 'Mouvement détecté' : 'Pas de mouvement';";
  html += "    document.getElementById('led').textContent = data.led ? 'ON' : 'OFF';";
  html += "    document.getElementById('toggleBtn').textContent = data.led ? 'Éteindre LED' : 'Allumer LED';";
  html += "    document.getElementById('toggleBtn').className = 'button ' + (data.led ? 'off' : '');";
  html += "  });";
  html += "  fetch('/stats').then(response => response.json()).then(stats => {";
  html += "    document.getElementById('pirCount').textContent = stats.pirCount;";
  html += "    document.getElementById('ledOnTime').textContent = (stats.ledOnTime / 1000).toFixed(1);";
  html += "    document.getElementById('ledToggleCount').textContent = stats.ledToggleCount;";
  html += "  });";
  html += "}";
  html += "function toggleLed() {";
  html += "  fetch('/toggle').then(() => updateData());";
  html += "}";
  html += "setInterval(updateData, 2000);";
  html += "</script>";
  html += "</head><body onload='updateData()'>";
  html += "<div class='container'>";
  html += "<h1>Panneau de contrôle ESP32</h1>";
  html += "<div class='sensor-value'>Température: <span id='temperature'>--</span></div>";
  html += "<div class='sensor-value'>Humidité: <span id='humidity'>--</span></div>";
  html += "<div class='sensor-value'>Détection: <span id='pir'>--</span></div>";
  html += "<div class='sensor-value'>État LED: <span id='led'>--</span></div>";
  html += "<button id='toggleBtn' onclick='toggleLed()' class='button'>--</button>";
  html += "<div class='stats'>";
  html += "<h2>Statistiques</h2>";
  html += "<p>Nombre de détections PIR: <span id='pirCount'>--</span></p>";
  html += "<p>Temps d'allumage LED (secondes): <span id='ledOnTime'>--</span></p>";
  html += "<p>Nombre de changements d'état LED: <span id='ledToggleCount'>--</span></p>";
  html += "</div>";
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"humidity\":" + String(humidity, 1) + ",";
  json += "\"motion\":" + String(pirState ? "true" : "false") + ",";
  json += "\"led\":" + String(ledState ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleStats() {
  String json = "{";
  json += "\"pirCount\":" + String(pirDetectionCount) + ",";
  json += "\"ledOnTime\":" + String(ledOnTime) + ",";
  json += "\"ledToggleCount\":" + String(ledOnCount);
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleLedToggle() {
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState);
  ledOnCount++;
  ledLastStateChange = millis();
  server.send(200, "text/plain", "OK");
}

void handleNotFound() {
  server.send(404, "text/plain", "Page non trouvée");
}