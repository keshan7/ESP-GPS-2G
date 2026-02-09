#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <Adafruit_NeoPixel.h>

// Pin definitions
#define SIM800_RX 5
#define SIM800_TX 4
#define NEO7M_RX 7
#define NEO7M_TX 6
#define RGB_LED_PIN 8  // Built-in RGB LED on ESP32-C3

// RGB LED setup
Adafruit_NeoPixel led(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// RGB Color definitions
#define COLOR_OFF 0, 0, 0
#define COLOR_GREEN 0, 255, 0
#define COLOR_BLUE 0, 0, 255
#define COLOR_RED 255, 0, 0
#define COLOR_YELLOW 255, 255, 0

// Server configuration
const char* server = "";
const int port = 80;
const char* endpoint = "";
const char* apn = "internet";

// GPS data structure
struct GPSData {
  unsigned long timestamp;
  String datetime;
  float lat;
  float lng;
  float speed;
  float altitude;
  int satellites;
  bool valid;
};

// Store last known good position
struct {
  float lat = 0;
  float lng = 0;
  float speed = 0;
  float altitude = 0;
  int satellites = 0;
  String datetime = "";
  bool hasPosition = false;
} lastKnownPosition;

// Fixed buffer for exactly 10 readings
#define MAX_READINGS 10
GPSData gpsBuffer[MAX_READINGS];
int currentSlot = 0;

// Serial connections
SoftwareSerial sim800(SIM800_RX, SIM800_TX);
SoftwareSerial neo7m(NEO7M_RX, NEO7M_TX);
TinyGPSPlus gps;

// Timing variables
unsigned long lastCollectionTime = 0;
const unsigned long collectionInterval = 10000; // 10 seconds
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 60000; // 60 seconds

// LED Functions
void setLED(uint8_t r, uint8_t g, uint8_t b) {
  led.setPixelColor(0, led.Color(r, g, b));
  led.show();
}

void ledOff() {
  setLED(COLOR_OFF);
}

// Fast blink green (success) - 2 quick flashes
void ledSuccessBlink() {
  for (int i = 0; i < 2; i++) {
    setLED(COLOR_GREEN);
    delay(50);
    ledOff();
    delay(50);
  }
}

// Fast blink blue twice (attempting to send)
void ledAttemptBlink() {
  for (int i = 0; i < 2; i++) {
    setLED(COLOR_BLUE);
    delay(50);
    ledOff();
    delay(50);
  }
}

// Solid red for 1 second (error/failure)
void ledError() {
  setLED(COLOR_RED);
  delay(1000);
  ledOff();
}

// Yellow blink (GPS collection in progress)
void ledGPSCollecting() {
  setLED(COLOR_YELLOW);
  delay(50);
  ledOff();
}

// Function declarations
bool waitForResponse(const char* expected, unsigned long timeout);
bool initSIM800();
bool sendDataToServer();
void collectSingleReading();
void clearBuffer();

// Function to wait for SIM800 response
bool waitForResponse(const char* expected, unsigned long timeout) {
  unsigned long start = millis();
  String response = "";
  
  while (millis() - start < timeout) {
    while (sim800.available()) {
      char c = sim800.read();
      response += c;
      Serial.print(c);
    }
    if (response.indexOf(expected) != -1) {
      return true;
    }
  }
  return false;
}

bool initSIM800() {
  Serial.println("Initializing SIM800...");
  sim800.begin(9600);
  delay(3000);
  
  sim800.println("AT");
  delay(1000);
  if (!waitForResponse("OK", 5000)) {
    Serial.println("SIM800 not responding");
    ledError();  // Red LED on failure
    return false;
  }
  
  sim800.println("AT+CMEE=2");
  delay(500);
  
  sim800.println("AT+CPIN?");
  delay(1000);
  if (!waitForResponse("READY", 5000)) {
    Serial.println("SIM card not ready");
    ledError();  // Red LED on failure
    return false;
  }
  
  sim800.print("AT+CSTT=\"");
  sim800.print(apn);
  sim800.println("\"");
  delay(1000);
  
  sim800.println("AT+CIICR");
  delay(3000);
  
  sim800.println("AT+CIFSR");
  delay(2000);
  
  Serial.println("SIM800 initialized");
  return true;
}

void clearBuffer() {
  for (int i = 0; i < MAX_READINGS; i++) {
    gpsBuffer[i].valid = false;
    gpsBuffer[i].timestamp = 0;
    gpsBuffer[i].datetime = "";
    gpsBuffer[i].lat = 0;
    gpsBuffer[i].lng = 0;
    gpsBuffer[i].speed = 0;
    gpsBuffer[i].altitude = 0;
    gpsBuffer[i].satellites = 0;
  }
  currentSlot = 0;
}

void collectSingleReading() {
  Serial.print("\n[Collection #");
  Serial.print(currentSlot + 1);
  Serial.print("/10] Attempting to get GPS fix...");
  
  ledGPSCollecting();  // Quick yellow flash when collecting
  
  unsigned long startTime = millis();
  bool gotFix = false;
  
  // Try for up to 9 seconds to get a GPS reading
  while (millis() - startTime < 9000) {
    while (neo7m.available() > 0) {
      char c = neo7m.read();
      
      if (gps.encode(c)) {
        if (gps.location.isValid() && gps.location.isUpdated()) {
          // Got valid GPS data
          gpsBuffer[currentSlot].timestamp = millis();
          
          // Build datetime string from GPS (YYYY-MM-DD HH:MM:SS)
          if (gps.date.isValid() && gps.time.isValid()) {
            char datetime[20];
            sprintf(datetime, "%04d-%02d-%02d %02d:%02d:%02d",
                    gps.date.year(),
                    gps.date.month(),
                    gps.date.day(),
                    gps.time.hour(),
                    gps.time.minute(),
                    gps.time.second());
            gpsBuffer[currentSlot].datetime = String(datetime);
          } else {
            gpsBuffer[currentSlot].datetime = "N/A";
          }
          
          gpsBuffer[currentSlot].lat = gps.location.lat();
          gpsBuffer[currentSlot].lng = gps.location.lng();
          gpsBuffer[currentSlot].speed = gps.speed.kmph();
          gpsBuffer[currentSlot].altitude = gps.altitude.meters();
          gpsBuffer[currentSlot].satellites = gps.satellites.value();
          gpsBuffer[currentSlot].valid = true;
          
          // Update last known position
          lastKnownPosition.lat = gpsBuffer[currentSlot].lat;
          lastKnownPosition.lng = gpsBuffer[currentSlot].lng;
          lastKnownPosition.speed = gpsBuffer[currentSlot].speed;
          lastKnownPosition.altitude = gpsBuffer[currentSlot].altitude;
          lastKnownPosition.satellites = gpsBuffer[currentSlot].satellites;
          lastKnownPosition.datetime = gpsBuffer[currentSlot].datetime;
          lastKnownPosition.hasPosition = true;
          
          Serial.print(" SUCCESS!");
          Serial.print(" Time: ");
          Serial.print(gpsBuffer[currentSlot].datetime);
          Serial.print(", Lat: ");
          Serial.print(gpsBuffer[currentSlot].lat, 6);
          Serial.print(", Lng: ");
          Serial.print(gpsBuffer[currentSlot].lng, 6);
          Serial.print(", Sats: ");
          Serial.println(gpsBuffer[currentSlot].satellites);
          
          gotFix = true;
          break;
        }
      }
    }
    
    if (gotFix) break;
  }
  
  if (!gotFix) {
    // Use last known position if available
    if (lastKnownPosition.hasPosition) {
      gpsBuffer[currentSlot].timestamp = millis();
      gpsBuffer[currentSlot].datetime = lastKnownPosition.datetime + " (cached)";
      gpsBuffer[currentSlot].lat = lastKnownPosition.lat;
      gpsBuffer[currentSlot].lng = lastKnownPosition.lng;
      gpsBuffer[currentSlot].speed = 0;
      gpsBuffer[currentSlot].altitude = lastKnownPosition.altitude;
      gpsBuffer[currentSlot].satellites = 0;
      gpsBuffer[currentSlot].valid = true;
      
      Serial.print(" USING CACHED POSITION");
      Serial.print(" (Last: ");
      Serial.print(lastKnownPosition.lat, 6);
      Serial.print(", ");
      Serial.print(lastKnownPosition.lng, 6);
      Serial.println(")");
    } else {
      // No GPS fix and no cached position
      gpsBuffer[currentSlot].valid = false;
      gpsBuffer[currentSlot].timestamp = millis();
      gpsBuffer[currentSlot].datetime = "N/A";
      Serial.println(" FAILED (No GPS fix, no cached position)");
    }
  }
  
  currentSlot++;
}

bool sendDataToServer() {
  // Blink blue twice to indicate sending attempt
  ledAttemptBlink();
  
  // Count valid readings
  int validCount = 0;
  for (int i = 0; i < MAX_READINGS; i++) {
    if (gpsBuffer[i].valid) validCount++;
  }
  
  Serial.println("\n=== Sending data to server ===");
  Serial.print("Valid readings: ");
  Serial.print(validCount);
  Serial.print("/");
  Serial.println(MAX_READINGS);
  
  // Close any existing connection
  sim800.println("AT+CIPCLOSE");
  delay(1000);
  while(sim800.available()) sim800.read();
  
  // Start TCP connection
  sim800.print("AT+CIPSTART=\"TCP\",\"");
  sim800.print(server);
  sim800.print("\",\"");
  sim800.print(port);
  sim800.println("\"");
  
  unsigned long connStart = millis();
  bool connected = false;
  while (millis() - connStart < 15000) {
    if (sim800.available()) {
      String resp = sim800.readString();
      Serial.print(resp);
      if (resp.indexOf("CONNECT OK") != -1 || resp.indexOf("ALREADY CONNECT") != -1) {
        connected = true;
        break;
      }
      if (resp.indexOf("ERROR") != -1) {
        Serial.println("Connection error");
        ledError();  // Red LED on connection error
        return false;
      }
    }
  }
  
  if (!connected) {
    Serial.println("Connection timeout");
    ledError();  // Red LED on timeout
    return false;
  }
  
  delay(2000);
  
  // Build JSON payload
  String jsonData = "{\"device_id\":\"ESP_GPS_001\",\"count\":";
  jsonData += String(validCount);
  jsonData += ",\"readings\":[";
  
  bool first = true;
  for (int i = 0; i < MAX_READINGS; i++) {
    if (gpsBuffer[i].valid) {
      if (!first) jsonData += ",";
      
      jsonData += "{";
      jsonData += "\"datetime\":\"" + gpsBuffer[i].datetime + "\",";
      jsonData += "\"ts\":" + String(gpsBuffer[i].timestamp) + ",";
      jsonData += "\"lat\":" + String(gpsBuffer[i].lat, 6) + ",";
      jsonData += "\"lng\":" + String(gpsBuffer[i].lng, 6) + ",";
      jsonData += "\"spd\":" + String(gpsBuffer[i].speed, 2) + ",";
      jsonData += "\"alt\":" + String(gpsBuffer[i].altitude, 1) + ",";
      jsonData += "\"sat\":" + String(gpsBuffer[i].satellites);
      jsonData += "}";
      
      first = false;
    }
  }
  jsonData += "]}";
  
  // Prepare HTTP request
  String httpHeader = "POST ";
  httpHeader += endpoint;
  httpHeader += " HTTP/1.1\r\n";
  httpHeader += "Host: ";
  httpHeader += server;
  httpHeader += "\r\n";
  httpHeader += "Content-Type: application/json\r\n";
  httpHeader += "Content-Length: ";
  httpHeader += String(jsonData.length());
  httpHeader += "\r\n\r\n";
  
  String fullRequest = httpHeader + jsonData;
  
  Serial.print("Request size: ");
  Serial.print(fullRequest.length());
  Serial.println(" bytes");
  
  // Clear receive buffer
  while(sim800.available()) sim800.read();
  
  // Send data
  sim800.print("AT+CIPSEND=");
  sim800.println(fullRequest.length());
  
  // Wait for prompt
  unsigned long promptStart = millis();
  bool gotPrompt = false;
  while (millis() - promptStart < 10000) {
    if (sim800.available()) {
      char c = sim800.read();
      Serial.write(c);
      if (c == '>') {
        gotPrompt = true;
        break;
      }
    }
  }
  
  if (!gotPrompt) {
    Serial.println("\nFailed to get send prompt");
    sim800.println("AT+CIPCLOSE");
    ledError();  // Red LED on send prompt failure
    return false;
  }
  
  delay(100);
  sim800.print(fullRequest);
  delay(500);
  
  // Wait for SEND OK
  unsigned long sendStart = millis();
  bool sendOk = false;
  while (millis() - sendStart < 20000) {
    if (sim800.available()) {
      String resp = sim800.readString();
      Serial.print(resp);
      if (resp.indexOf("SEND OK") != -1) {
        sendOk = true;
        break;
      }
    }
  }
  
  if (!sendOk) {
    Serial.println("Send failed - no SEND OK");
    sim800.println("AT+CIPCLOSE");
    ledError();  // Red LED on send failure
    return false;
  }
  
  Serial.println("\nWaiting for server response...");
  delay(3000);
  
  while (sim800.available()) {
    Serial.write(sim800.read());
  }
  
  sim800.println("AT+CIPCLOSE");
  delay(1000);
  
  Serial.println("\n=== Data sent successfully! ===\n");
  ledSuccessBlink();  // Green fast blink on success!
  return true;
}

void setup() {
  Serial.begin(115200);
  neo7m.begin(9600);
  
  // Initialize RGB LED
  led.begin();
  led.setBrightness(50);  // Set brightness (0-255)
  ledOff();
  
  delay(2000);
  Serial.println("\n=== GPS Tracker - 10 Readings/Minute ===");
  Serial.println("Collects GPS every 10 seconds, sends every 1 minute");

  delay(5000);
  if (!initSIM800()) {
    Serial.println("Failed to initialize SIM800");
    Serial.println("Will continue collecting GPS data...");
    // LED error already shown in initSIM800()
  }
  
  clearBuffer();
  
  Serial.println("System ready!\n");
  lastCollectionTime = millis();
  lastSendTime = millis();

  delay(5000);
  if (sendDataToServer()) {
      Serial.println("Transmission successful!");
    } else {
      Serial.println("Transmission failed. Will retry in 1 minute.");
    }


}

void loop() {
  unsigned long currentTime = millis();
  
  // Collect one GPS reading every 10 seconds
  if (currentSlot < MAX_READINGS && currentTime - lastCollectionTime >= collectionInterval) {
    collectSingleReading();
    lastCollectionTime = currentTime;
  }
  
  // Send data every 60 seconds
  if (currentTime - lastSendTime >= sendInterval) {
    Serial.println("\n=== 1 Minute Elapsed - Sending Data ===");
    
    if (sendDataToServer()) {
      Serial.println("Transmission successful!");
    } else {
      Serial.println("Transmission failed. Will retry in 1 minute.");
    }
    
    // Clear buffer and start fresh
    clearBuffer();
    lastSendTime = currentTime;
    lastCollectionTime = currentTime;
  }
  
  // Show countdown every 5 seconds
  static unsigned long lastStatus = 0;
  if (currentTime - lastStatus >= 5000) {
    unsigned long nextCollection = collectionInterval - (currentTime - lastCollectionTime);
    unsigned long nextSend = sendInterval - (currentTime - lastSendTime);
    
    Serial.print("[Status] Slot: ");
    Serial.print(currentSlot);
    Serial.print("/10");
    
    if (currentSlot < MAX_READINGS) {
      Serial.print(" | Next reading in: ");
      Serial.print(nextCollection / 1000);
      Serial.print("s");
    }
    
    Serial.print(" | Next send in: ");
    Serial.print(nextSend / 1000);
    Serial.println("s");
    
    lastStatus = currentTime;
  }
  
  delay(100);
}


