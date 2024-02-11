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
#define SPACE_UPDATE_DELAY 15000  // 15 seconds in milliseconds

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

String spaceData; // To store the filesystem space data

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Woke up from deep sleep");
  } else {
    Serial.println("Not a deep sleep reset, clearing data.txt");
    LittleFS.remove("/data.txt"); // Clear "data.txt" on boot but not after waking up from deep sleep
  }

  sensors.begin();

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_IP, gateway, subnet);

  server.on("/", HTTP_GET, []() {
    File file = LittleFS.open("/data.txt", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }
    server.streamFile(file, "text/plain");
    file.close();
  });

  server.on("/space", HTTP_GET, []() {
    updateSpaceData(); // Update space data for serving
    server.send(200, "text/plain", spaceData);
  });

  server.begin();

  performMeasurements();

  // Delay to ensure space data is updated after 15 seconds
  if (millis() < SPACE_UPDATE_DELAY) {
    delay(SPACE_UPDATE_DELAY - millis());
  }
  updateSpaceData(); // Initial update of space data

  unsigned long startMillis = millis();
  while (millis() - startMillis < WEB_SERVER_ACTIVE_TIME) {
    server.handleClient();
  }

  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION_US);
  Serial.println("Entering deep sleep");
  esp_deep_sleep_start();
}

void loop() {
  // Empty. Everything is handled in setup due to deep sleep behavior.
}

void performMeasurements() {
  DateTime now = rtc.now();
  String dataString = String(now.year(), DEC) + "-" + String(now.month(), DEC) + "-" + String(now.day(), DEC) +
                      " " + String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC) + "\n";

  for (int j = 0; j < 5; j++) {
    sensors.requestTemperatures();
    for (int i = 0; i < sensors.getDeviceCount(); i++) {
      if (sensors.getAddress(tempDeviceAddress, i)) {
        char sensorId[5]; // Enough space for two hex bytes and null terminator
        sprintf(sensorId, "%02X%02X", tempDeviceAddress[6], tempDeviceAddress[7]);
        float tempC = sensors.getTempC(tempDeviceAddress);
        dataString += String(sensorId) + ";" + String(tempC, 2) + "\n";
     }
    }
  }

  File file = LittleFS.open("/data.txt", FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  file.print(dataString);
  file.close();
}

void updateSpaceData() {
  unsigned long fileSize = LittleFS.open("/data.txt", "r").size();
  unsigned long remainingFreeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
  float percentageDecrease = 100.0 * (LittleFS.totalBytes() - remainingFreeSpace) / LittleFS.totalBytes();

  spaceData = "Free space: " + String(remainingFreeSpace) + " / " + String(LittleFS.totalBytes()) + " bytes\n";
  spaceData += "File size: " + String(fileSize) + " bytes\n";
  spaceData += "Space used: " + String(percentageDecrease) + "%\n";
}
