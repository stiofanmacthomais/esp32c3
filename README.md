# esp32c3-based environmental sensor

A simple project using an ESP32C3-Supermini node connected to a BME/BMP280 sensor that can measure temperature, humidity and pressure.  Data is read intermittently and updated on an SSD1306 OLED display.  The code provides a simple web-server showing the same results, and the webserver is implemented with basic rate limiting.
