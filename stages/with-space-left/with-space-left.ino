#include <OneWire.h>
#include <DallasTemperature.h>
#include "FS.h"
#include <LittleFS.h>
#include <WiFi.h> // For ESP32
// #include <ESP8266WiFi.h> // Uncomment this for ESP8266 and comment out the above line
#include <WebServer.h> // For ESP32
// #include <ESP8266WebServer.h> // Uncomment this for ESP8266 and comment out the above line

#define FORMAT_LITTLEFS_IF_FAILED true
#define ONE_WIRE_BUS 4

// WiFi settings
const char* ssid = "esp-data";
const char* password = "esp-data";

IPAddress local_IP(172, 16, 16, 1);
IPAddress gateway(172, 16, 16, 1);
IPAddress subnet(255, 255, 255, 0);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress tempDeviceAddress; 
WebServer server(80);

// Global variable for initial free space
unsigned long initialFreeSpace = 0;

void setup() {
  Serial.begin(115200);
  sensors.begin();
  int numberOfDevices = sensors.getDeviceCount();

  // Locate and print devices
  for (int i = 0; i < numberOfDevices; i++) {
    if (sensors.getAddress(tempDeviceAddress, i)) {
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      Serial.println();
    } else {
      Serial.println("Found ghost device but could not detect address. Check power and cabling");
    }
  }
  
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  
  // Get initial free space
  initialFreeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
  Serial.print("Initial free space: ");
  Serial.println(initialFreeSpace);

  // Setup WiFi
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
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
}

void loop() {
  sensors.requestTemperatures();
  File file = LittleFS.open("/data.txt", FILE_APPEND);
  if (!file) {
    Serial.println("- failed to open file for appending");
    return;
  }

  for (int i = 0; i < sensors.getDeviceCount(); i++) {
    if (sensors.getAddress(tempDeviceAddress, i)) {
      float tempC = sensors.getTempC(tempDeviceAddress);
      Serial.printf("Device %d Temperature: %.2f°C\n", i, tempC);
      file.printf("Device %d Temperature: %.2f°C\n", i, tempC);
    }
  }
  file.close();

  // Log file size and free space
  logFileSystemInfo();

  server.handleClient();
  delay(5000);
}

void logFileSystemInfo() {
  File file = LittleFS.open("/data.txt", "r");
  unsigned long fileSize = file.size();
  file.close();

  unsigned long remainingFreeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
  float percentageDecrease = 100.0 * (initialFreeSpace - remainingFreeSpace) / initialFreeSpace;

  Serial.print("File size: ");
  Serial.print(fileSize);
  Serial.println(" bytes");

  Serial.print("Remaining free space: ");
  Serial.print(remainingFreeSpace);
  Serial.println(" bytes");

  Serial.print("Percentage decrease in free space: ");
  Serial.print(percentageDecrease);
  Serial.println("%");
}

void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
