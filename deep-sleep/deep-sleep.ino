#include <OneWire.h>
#include <DallasTemperature.h>
#include "FS.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "RTClib.h"
#include "esp_sleep.h"

#define FORMAT_LITTLEFS_IF_FAILED true
#define ONE_WIRE_BUS 4
#define DEEP_SLEEP_DURATION_US 3 * 60 * 1000000  // 3 minutes in microseconds
#define WEB_SERVER_ACTIVE_TIME 120 * 1000  // 2 minutes in milliseconds

const char* ssid = "esp-data";
const char* password = "esp-data";

IPAddress local_IP(172, 16, 16, 1);
IPAddress gateway(172, 16, 16, 1);
IPAddress subnet(255, 255, 255, 0);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress tempDeviceAddress;
WebServer server(80);

RTC_DS3231 rtc;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Check if the ESP32 is waking up from deep sleep
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Woke up from deep sleep");
  } else {
    // This code runs if the ESP32 has been reset or powered on
    Serial.println("Not a deep sleep reset");
  }

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // Setup LittleFS
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  // Setup temperature sensor
  sensors.begin();

  // Setup WiFi Access Point
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_IP, gateway, subnet);

  // Setup web server
  server.on("/", HTTP_GET, []() {
    File file = LittleFS.open("/data.txt", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }
    server.streamFile(file, "text/plain");
    file.close();
  });
  server.begin();

  // Perform measurements and log data
  performMeasurements();

  // Serve data for 2 minutes before going to deep sleep
  unsigned long startMillis = millis();
  while (millis() - startMillis < WEB_SERVER_ACTIVE_TIME) {
    server.handleClient();
  }

  // Enable deep sleep
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION_US);
  Serial.println("Entering deep sleep");
  esp_deep_sleep_start();
}

void loop() {
  // Empty. Everything is handled in setup due to deep sleep behavior.
}

void performMeasurements() {
  DateTime now = rtc.now();
  String dataString;
  dataString += String(now.year(), DEC) + "-" + String(now.month(), DEC) + "-" + String(now.day(), DEC);
  dataString += " " + String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC) + ":\n";

  for (int j = 0; j < 5; j++) {
    sensors.requestTemperatures();
    for (int i = 0; i < sensors.getDeviceCount(); i++) {
      if (sensors.getAddress(tempDeviceAddress, i)) {
        float tempC = sensors.getTempC(tempDeviceAddress);
        dataString += "Device " + String(i) + " Temperature: " + String(tempC, 2) + "°C\n";
      }
    }
  }

  // Log file system info directly after measurements
  logFileSystemInfo(dataString);

  File file = LittleFS.open("/data.txt", FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  file.print(dataString);
  file.close();
}

void logFileSystemInfo(String& dataString) {
  unsigned long fileSize = LittleFS.open("/data.txt", "r").size();
  unsigned long remainingFreeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
  float percentageDecrease = 100.0 * (LittleFS.totalBytes() - remainingFreeSpace) / LittleFS.totalBytes();

  dataString += "Free space: " + String(remainingFreeSpace) + " / " + String(LittleFS.totalBytes()) + " bytes\n";
  dataString += "File size: " + String(fileSize) + " bytes\n";
  dataString += "Space used: " + String(percentageDecrease) + "%\n\n";
}
