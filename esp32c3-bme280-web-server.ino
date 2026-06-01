#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// =========================
// WiFi
// =========================
const char* WIFI_SSID = "***SSID***";
const char* WIFI_PASSWORD = "***Passwpod***";

// =========================
// Web Stats
// =========================
unsigned long totalHits = 0;
int activeClients = 0;

// =========================
// Display
// =========================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// =========================
// Pinouts and Address
// =========================
#define I2C_SDA 0
#define I2C_SCL 1
#define BME280_ADDRESS 0x76


// =========================
// BME280
// =========================
Adafruit_BME280 bme;


// =========================
// Web Server
// =========================
WebServer server(80);

// =========================
// Sensor Values
// =========================
#define SEALEVELPRESSURE_HPA (1013.25)
float temperatureC = 0.0f;
float humidityPct = 0.0f;
float pressureHpa = 0.0f;

// =========================
// Rate Limiter
// =========================
struct RateLimitEntry
{
  IPAddress ip;
  unsigned long windowStart;
  int requestCount;
};

const int MAX_CLIENTS = 20;
RateLimitEntry clients[MAX_CLIENTS];

const int MAX_REQUESTS = 60;
const unsigned long WINDOW_MS = 60000;

// =========================
// Active Web Clients
// =========================
void updateActiveClients()
{
  unsigned long now = millis();
  activeClients = 0;

  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (clients[i].windowStart != 0 &&
        (now - clients[i].windowStart) <= WINDOW_MS)
    {
      activeClients++;
    }
  }
}

// =========================
// Timing
// =========================
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL_MS = 2000;

// ==================================================
// Rate Limiter
// ==================================================
bool isRateLimited()
{
  IPAddress clientIP = server.client().remoteIP();
  unsigned long now = millis();

  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (clients[i].ip == clientIP)
    {
      // Reset time window
      if (now - clients[i].windowStart > WINDOW_MS)
      {
        clients[i].windowStart = now;
        clients[i].requestCount = 1;
        return false;
      }

      clients[i].requestCount++;

      if (clients[i].requestCount > MAX_REQUESTS)
      {
        return true;
      }

      return false;
    }
  }

  // Add new client
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (clients[i].windowStart == 0)
    {
      clients[i].ip = clientIP;
      clients[i].windowStart = now;
      clients[i].requestCount = 1;
      return false;
    }
  }

  return false;
}

// ==================================================
// Read Sensor
// ==================================================
void updateSensorData()
{
  temperatureC = bme.readTemperature();
  humidityPct = bme.readHumidity();
  pressureHpa = bme.readPressure() / 100.0F;
}

// ==================================================
// OLED Display
// ==================================================
void updateDisplay()
{
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("ESP32-C3 Monitor");

  display.setCursor(0, 12);
  display.printf("T: %.1f C\n", temperatureC);

  display.setCursor(0, 22);
  display.printf("H: %.1f %%\n", humidityPct);

  display.setCursor(0, 32);
  display.printf("P: %.1f hPa\n", pressureHpa);

  display.setCursor(0, 44);
  display.printf("Clients: %d\n", activeClients);

  display.setCursor(0, 54);
  display.printf("Hits: %lu", totalHits);

  display.display();
}

// ==================================================
// HTML Page
// ==================================================
String buildPage()
{
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Weather Monitor</title>

<style>
body {
    font-family: Arial, sans-serif;
    background: #111;
    color: #eee;
    text-align: center;
    padding: 30px;
}

.card {
    background: #222;
    border-radius: 12px;
    padding: 24px;
    max-width: 400px;
    margin: auto;
}

.value {
    font-size: 2rem;
    margin: 16px 0;
}
</style>

<script>
setTimeout(() => {
    location.reload();
}, 3000);
</script>
</head>

<body>
<div class="card">
    <h1>Living Room Environment</h1>

    <div class="value">
        🌡 Temperature:
        <strong>%TEMP%</strong> °C
    </div>

    <div class="value">
        💧 Humidity:
        <strong>%HUM%</strong> %
    </div>

    <div class="value">
        📈 Pressure:
        <strong>%PRESS%</strong> hPa
    </div>
</div>
</body>
</html>
)rawliteral";

  html.replace("%TEMP%", String(temperatureC, 1));
  html.replace("%HUM%", String(humidityPct, 1));
  html.replace("%PRESS%", String(pressureHpa, 1));

  return html;
}

// ==================================================
// Base Route
// ==================================================
void handleRoot()
{
  totalHits++;

  updateActiveClients();

  if (isRateLimited())
  {
    server.send(
      429,
      "text/plain",
      "Too Many Requests"
    );
    return;
  }

  server.send(
    200,
    "text/html",
    buildPage()
  );

  updateDisplay();
}

// ==================================================
// Setup
// ==================================================
void setup()
{
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Initializing...");

  Wire.begin(I2C_SDA, I2C_SCL);

  // OLED
  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C))
  {
    Serial.println("SSD1306 failed");
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Starting...");
  display.display();

  // BME280
  bool status = bme.begin(BME280_ADDRESS);
  if (!status)
  {
    Serial.println("BME280 not found");
    while (true);
  }
  Serial.println ("BME280 found");

  // Corrections for BME readings for improved accuracy
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X16,  // temperature
                  Adafruit_BME280::SAMPLING_X1,   // pressure
                  Adafruit_BME280::SAMPLING_X1,   // humidity
                  Adafruit_BME280::FILTER_X16,
                  Adafruit_BME280::STANDBY_MS_0_5);

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // ESP32C3 is a bit crappy out of the box, so boost wifi power
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  Serial.println("Connecting to Wifi");

  while (WiFi.status() != WL_CONNECTED)
  {
    display.printf("Trying to connect to %",WIFI_SSID);
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();

  updateSensorData();
  updateDisplay();

  Serial.print("Open browser: http://");
  Serial.println(WiFi.localIP());
}

// ==================================================
// Main Loop
// ==================================================
void loop()
{
  server.handleClient();

  unsigned long now = millis();

  if (now - lastSensorRead >= SENSOR_INTERVAL_MS)
  {
    lastSensorRead = now;

    updateSensorData();
    updateDisplay();
  }
}
