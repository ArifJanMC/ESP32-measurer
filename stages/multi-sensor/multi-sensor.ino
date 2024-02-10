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
int numberOfDevices;
DeviceAddress tempDeviceAddress; 

WebServer server(80);

void setup() {
  Serial.begin(115200);
  
  // Start the DS18B20 sensor
  sensors.begin();
  
  // Count the number of temperature devices found
  numberOfDevices = sensors.getDeviceCount();
  
  // Locate devices on the bus
  Serial.print("Locating devices... Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" devices.");
  
  // Print out each device's address
  for (int i = 0; i < numberOfDevices; i++) {
    if (sensors.getAddress(tempDeviceAddress, i)) {
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      Serial.println();
    } else {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.println(" but could not detect address. Check power and cabling");
    }
  }
  
  // Initialize LittleFS
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  
  // Create or open the file
  File file = LittleFS.open("/data.txt", FILE_WRITE);
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print("Sensor data:\n")) {
    Serial.println("- test file written");
  } else {
    Serial.println("- write failed");
  }
  file.close();

  // Set up WiFi access point
  if (!WiFi.softAP(ssid, password)) {
    Serial.println("Failed to start AP");
    return;
  }
  if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
    Serial.println("AP Config Failed");
    return;
  }
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Start web server
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
  
  for (int i = 0; i < numberOfDevices; i++) {
    if (sensors.getAddress(tempDeviceAddress, i)) {
      float tempC = sensors.getTempC(tempDeviceAddress);
      Serial.printf("Device %d Temperature: %.2f°C\n", i, tempC);
      file.printf("Device %d Temperature: %.2f°C\n", i, tempC);
    }
  }
  file.close();
  
  server.handleClient();
  delay(5000); // Wait for 5 seconds before reading the temperature again
}

// Function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
