#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <time.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// =========================
// WiFi
// =========================
const char* WIFI_SSID = "***ESSID***";
const char* WIFI_PASSWORD = "***Password***";

// =========================
// NTP
// =========================
const char* NTP_SERVER = "***NTPSERVERDOMAINORIP";
const long GMT_OFFSET_SEC = 0;
const int DAYLIGHT_OFFSET_SEC = 0;

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

String lastReadISO = "Not synced";

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
// Sensor re-read timing
// =========================
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL_MS = 30000;

// ==================================================
// Active Web Clients
// ==================================================
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

// ==================================================
// Super-basic Rate Limiter
// ==================================================
bool isRateLimited()
{
  IPAddress clientIP = server.client().remoteIP();
  unsigned long now = millis();

  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (clients[i].ip == clientIP)
    {
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
// ISO8601 Time
// ==================================================
String getISOTime()
{
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo))
  {
    return "Time unavailable";
  }

  char buffer[32];

  strftime(
    buffer,
    sizeof(buffer),
    "%Y-%m-%dT%H:%M:%SZ",
    &timeinfo
  );

  return String(buffer);
}

// ==================================================
// Read Sensor
// ==================================================
void updateSensorData()
{
  bme.takeForcedMeasurement();

  temperatureC = bme.readTemperature();
  humidityPct = bme.readHumidity();
  pressureHpa = bme.readPressure() / 100.0F;

  lastReadISO = getISOTime();
}

// ==================================================
// OLED Display
// ==================================================
void updateDisplay()
{
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ESP32-C3 Monitor");

  display.setCursor(0, 12);
  display.printf("T: %.1f C\n", temperatureC);

  display.setCursor(0, 22);
  display.printf("H: %.1f %%\n", humidityPct);

  display.setCursor(0, 32);
  display.printf("P: %.1f hPa\n", pressureHpa);

  display.setCursor(0, 42);
  display.printf("Hits:%lu C:%d\n",
                 totalHits,
                 activeClients);

  // Tiny timestamp
  display.setTextSize(1);
  display.setCursor(0, 54);

  if (lastReadISO.length() > 20)
  {
    display.print(lastReadISO.substring(11, 19));
  }
  else
  {
    display.print(lastReadISO);
  }

  display.display();
}

// ==================================================
// HTML Page
// ==================================================
String buildPage()
{
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport"
      content="width=device-width, initial-scale=1.0">

<meta http-equiv="Cache-Control"
      content="no-cache, no-store, must-revalidate">
<meta http-equiv="Pragma"
      content="no-cache">
<meta http-equiv="Expires"
      content="0">

<title>ESP32 Environment</title>

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
    box-sizing: border-box;
}

.value {
    font-size: 2rem;
    margin: 16px 0;
}

.last-read {
    margin-top: 20px;
    font-size: 0.75rem;
    color: #aaa;
}
</style>

<script>
async function loadEnvironment()
{
    try
    {
        const response =
            await fetch('/api/environment');

        if (!response.ok)
        {
            throw new Error(
                'HTTP ' + response.status
            );
        }

        const data =
            await response.json();

        document.getElementById(
            'temp'
        ).textContent =
            data.temperature_c.toFixed(1);

        document.getElementById(
            'hum'
        ).textContent =
            data.humidity_pct.toFixed(1);

        document.getElementById(
            'press'
        ).textContent =
            data.pressure_hpa.toFixed(1);

        document.getElementById(
            'lastread'
        ).textContent =
            data.last_read;
    }
    catch (err)
    {
        console.error(err);
    }
}

window.onload = () =>
{
    loadEnvironment();

    setInterval(
        loadEnvironment,
        30000
    );
};
</script>
</head>

<body>
<div class="card">

<h1>Living Room Environment</h1>

<div class="value">
🌡 Temperature:
<strong id="temp">--.-</strong> °C
</div>

<div class="value">
💧 Humidity:
<strong id="hum">--.-</strong> %
</div>

<div class="value">
📈 Pressure:
<strong id="press">--.-</strong> hPa
</div>

<div class="last-read">
Last read:
<span id="lastread">
Loading...
</span>
</div>

</div>
</body>
</html>
)rawliteral";
}

// ============================================
// API endpoint to return JSON data from sensor
// ============================================

void handleEnvironmentApi()
{
  totalHits++;

  updateActiveClients();

  if (isRateLimited())
  {
    server.send(
      429,
      "application/json",
      "{\"error\":\"Too Many Requests\"}"
    );
    return;
  }

  String json = "{";

  json += "\"temperature_c\":";
  json += String(temperatureC, 1);

  json += ",\"humidity_pct\":";
  json += String(humidityPct, 1);

  json += ",\"pressure_hpa\":";
  json += String(pressureHpa, 1);

  json += ",\"last_read\":\"";
  json += lastReadISO;
  json += "\"";

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}

// ==================================================
// Base route for home page
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
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
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

  Serial.println("BME280 found");

  bme.setSampling(
    Adafruit_BME280::MODE_FORCED,
    Adafruit_BME280::SAMPLING_X16,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::FILTER_X16,
    Adafruit_BME280::STANDBY_MS_0_5
  );

  // WiFi
  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  WiFi.setTxPower(
    WIFI_POWER_8_5dBm
  );

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());

  // NTP
  configTime(
    GMT_OFFSET_SEC,
    DAYLIGHT_OFFSET_SEC,
    NTP_SERVER
  );

  Serial.println("Waiting for NTP...");

  struct tm timeinfo;

  while (!getLocalTime(&timeinfo))
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Time synced");

  // handlers for home page at / and the API endpoint
  server.on("/", HTTP_GET, handleRoot);

  server.on("/api/environment", HTTP_GET, handleEnvironmentApi);

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
    updateActiveClients();
    updateDisplay();
  }
}
