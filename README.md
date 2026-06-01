# esp32c3-based environment sensor with web server

A simple project using an ESP32C3-Supermini node connected to a BME/BMP280 sensor that can measure temperature, humidity and pressure.  Data is read intermittently and updated on an SSD1306 OLED display.  The code provides a simple web-server showing the same results, and the webserver is implemented with basic rate limiting.

- The display and BME are wired for SDA and SCL to the ESP's pin 0 and 1 respectively
- The devices are powered over the 3.3v power pin on the board.

Of note was the need to boost the power of the ESP32C3 to get a wifi connection
