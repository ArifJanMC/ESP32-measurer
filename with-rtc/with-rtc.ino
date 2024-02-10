#include <OneWire.h>
#include <DallasTemperature.h>
#include "FS.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "RTClib.h"

#define FORMAT_LITTLEFS_IF_FAILED true
#define ONE_WIRE_BUS 4

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

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // Set the RTC to February 10, 2024, 23:30:00 (UTC+10)
  // Adjust for UTC+0 as RTC does not handle time zones
  rtc.adjust(DateTime(2024, 2, 10, 13, 30, 0)); // UTC time equivalent of 23:30 UTC+10

  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  // Clear "data.txt" on boot
  LittleFS.remove("/data.txt");

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
  server.begin();
}

void loop() {
  DateTime now = rtc.now();
  String dataString = "";
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
  unsigned long fileSize = LittleFS.open("/data.txt", "r").size();
  unsigned long remainingFreeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
  float percentageDecrease = 100.0 * (LittleFS.totalBytes() - remainingFreeSpace) / LittleFS.totalBytes();

  dataString += "Free space: " + String(remainingFreeSpace) + " / " + String(LittleFS.totalBytes()) + " bytes\n";
  dataString += "File size: " + String(fileSize) + " bytes\n";
  dataString += "Space used: " + String(percentageDecrease) + "%\n\n";

  File file = LittleFS.open("/data.txt", FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  file.print(dataString);
  file.close();

  server.handleClient();
  delay(30000); // Sleep for 30 seconds
}

void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
