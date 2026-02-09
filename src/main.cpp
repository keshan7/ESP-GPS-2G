// #include <Arduino.h>
// #include <SoftwareSerial.h>
// #include <TinyGPS++.h>

// // Pin definitions
// #define SIM800_RX 5
// #define SIM800_TX 4
// #define NEO7M_RX 7
// #define NEO7M_TX 6

// // Server configuration
// const char* server = "";
// const int port = 80;
// const char* endpoint = "";
// const char* apn = "internet"; // Change to your carrier's APN

// // GPS data structure
// struct GPSData {
//   unsigned long timestamp;
//   float lat;
//   float lng;
//   float speed;
//   float altitude;
//   int satellites;
// };

// // Large buffer to store as many GPS readings as possible
// #define MAX_READINGS 10  // Reduced to keep payload 
// GPSData gpsBuffer[MAX_READINGS];
// int readingCount = 0;

// // Serial connections
// SoftwareSerial sim800(SIM800_RX, SIM800_TX);
// SoftwareSerial neo7m(NEO7M_RX, NEO7M_TX);
// TinyGPSPlus gps;

// // Timing variables
// unsigned long lastSendTime = 0;
// const unsigned long sendInterval = 20000; // 30 seconds

// // Function declarations
// bool waitForResponse(const char* expected, unsigned long timeout);
// bool initSIM800();
// bool sendDataToServer();
// void collectGPSData();

// // Function to wait for SIM800 response
// bool waitForResponse(const char* expected, unsigned long timeout) {
//   unsigned long start = millis();
//   String response = "";
  
//   while (millis() - start < timeout) {
//     while (sim800.available()) {
//       char c = sim800.read();
//       response += c;
//       Serial.print(c);
//     }
//     if (response.indexOf(expected) != -1) {
//       return true;
//     }
//   }
//   return false;
// }

// bool initSIM800() {
//   Serial.println("Initializing SIM800...");
//   sim800.begin(9600);
//   delay(3000);
  
//   // Check if module is ready
//   sim800.println("AT");
//   delay(1000);
//   if (!waitForResponse("OK", 5000)) {
//     Serial.println("SIM800 not responding");
//     return false;
//   }
  
//   // Disable echo
//   sim800.println("AT+CMEE=2");
//   delay(500);
  
//   // Check SIM card
//   sim800.println("AT+CPIN?");
//   delay(1000);
//   if (!waitForResponse("READY", 5000)) {
//     Serial.println("SIM card not ready");
//     return false;
//   }
  
//   // Set APN
//   sim800.print("AT+CSTT=\"");
//   sim800.print(apn);
//   sim800.println("\"");
//   delay(1000);
  
//   // Start GPRS
//   sim800.println("AT+CIICR");
//   delay(3000);
  
//   // Get IP
//   sim800.println("AT+CIFSR");
//   delay(2000);
  
//   Serial.println("SIM800 initialized");
//   return true;
// }

// void collectGPSData() {
//   // Continuously read GPS and store every valid location update
//   while (neo7m.available() > 0) {
//     char c = neo7m.read();
    
//     if (gps.encode(c)) {
//       // Store reading whenever location is updated
//       if (gps.location.isValid() && gps.location.isUpdated()) {
//         // If buffer is full, reset and start fresh
//         if (readingCount >= MAX_READINGS) {
//           Serial.println("\n!!! BUFFER FULL - Resetting and storing new data !!!");
//           readingCount = 0;
//         }
        
//         gpsBuffer[readingCount].timestamp = millis();
//         gpsBuffer[readingCount].lat = gps.location.lat();
//         gpsBuffer[readingCount].lng = gps.location.lng();
//         gpsBuffer[readingCount].speed = gps.speed.kmph();
//         gpsBuffer[readingCount].altitude = gps.altitude.meters();
//         gpsBuffer[readingCount].satellites = gps.satellites.value();
        
//         readingCount++;
        
//         // Print every 10th reading to avoid serial overflow
//         if (readingCount % 10 == 0) {
//           Serial.print("Readings collected: ");
//           Serial.print(readingCount);
//           Serial.print(" | Latest: ");
//           Serial.print(gpsBuffer[readingCount-1].lat, 6);
//           Serial.print(", ");
//           Serial.println(gpsBuffer[readingCount-1].lng, 6);
//         }
//       }
//     }
//   }
// }

// bool sendDataToServer() {
//   if (readingCount == 0) {
//     Serial.println("No GPS data to send");
//     return false;
//   }
  
//   Serial.println("\n=== Sending data to server ===");
//   Serial.print("Total readings to send: ");
//   Serial.println(readingCount);
  
//   // Close any existing connection first
//   sim800.println("AT+CIPCLOSE");
//   delay(1000);
  
//   // Clear any pending data
//   while(sim800.available()) sim800.read();
  
//   // Start TCP connection
//   sim800.print("AT+CIPSTART=\"TCP\",\"");
//   sim800.print(server);
//   sim800.print("\",\"");
//   sim800.print(port);
//   sim800.println("\"");
  
//   // Wait for connection
//   unsigned long connStart = millis();
//   bool connected = false;
//   while (millis() - connStart < 15000) {
//     if (sim800.available()) {
//       String resp = sim800.readString();
//       Serial.print(resp);
//       if (resp.indexOf("CONNECT OK") != -1) {
//         connected = true;
//         break;
//       }
//       if (resp.indexOf("ALREADY CONNECT") != -1) {
//         connected = true;
//         break;
//       }
//       if (resp.indexOf("ERROR") != -1) {
//         Serial.println("Connection error");
//         return false;
//       }
//     }
//   }
  
//   if (!connected) {
//     Serial.println("Connection timeout");
//     return false;
//   }
  
//   delay(2000);
  
//   // Build JSON payload
//   String header = "{\"device_id\":\"ESP_GPS_001\",\"count\":";
//   header += String(readingCount);
//   header += ",\"readings\":[";
  
//   String footer = "]}";
  
//   // Build complete JSON
//   String jsonData = header;
//   for (int i = 0; i < readingCount; i++) {
//     jsonData += "{";
//     jsonData += "\"ts\":" + String(gpsBuffer[i].timestamp) + ",";
//     jsonData += "\"lat\":" + String(gpsBuffer[i].lat, 6) + ",";
//     jsonData += "\"lng\":" + String(gpsBuffer[i].lng, 6) + ",";
//     jsonData += "\"spd\":" + String(gpsBuffer[i].speed, 2) + ",";
//     jsonData += "\"alt\":" + String(gpsBuffer[i].altitude, 1) + ",";
//     jsonData += "\"sat\":" + String(gpsBuffer[i].satellites);
//     jsonData += "}";
//     if (i < readingCount - 1) jsonData += ",";
//   }
//   jsonData += footer;
  
//   // Prepare HTTP request
//   String httpHeader = "POST ";
//   httpHeader += endpoint;
//   httpHeader += " HTTP/1.1\r\n";
//   httpHeader += "Host: ";
//   httpHeader += server;
//   httpHeader += "\r\n";
//   httpHeader += "Content-Type: application/json\r\n";
//   httpHeader += "Content-Length: ";
//   httpHeader += String(jsonData.length());
//   httpHeader += "\r\n\r\n";
  
//   String fullRequest = httpHeader + jsonData;
  
//   Serial.print("Total request size: ");
//   Serial.print(fullRequest.length());
//   Serial.println(" bytes");
  
//   // Clear receive buffer
//   while(sim800.available()) sim800.read();
  
//   // Send data length command
//   sim800.print("AT+CIPSEND=");
//   sim800.println(fullRequest.length());
  
//   // Wait for prompt '>'
//   unsigned long promptStart = millis();
//   bool gotPrompt = false;
//   while (millis() - promptStart < 10000) {
//     if (sim800.available()) {
//       char c = sim800.read();
//       Serial.write(c);
//       if (c == '>') {
//         gotPrompt = true;
//         break;
//       }
//     }
//   }
  
//   if (!gotPrompt) {
//     Serial.println("\nFailed to get send prompt");
//     sim800.println("AT+CIPCLOSE");
//     return false;
//   }
  
//   delay(100);
  
//   // Send the complete HTTP request
//   sim800.print(fullRequest);
//   delay(500);
  
//   // Wait for SEND OK
//   unsigned long sendStart = millis();
//   bool sendOk = false;
//   while (millis() - sendStart < 20000) {
//     if (sim800.available()) {
//       String resp = sim800.readString();
//       Serial.print(resp);
//       if (resp.indexOf("SEND OK") != -1) {
//         sendOk = true;
//         break;
//       }
//     }
//   }
  
//   if (!sendOk) {
//     Serial.println("Send failed - no SEND OK");
//     sim800.println("AT+CIPCLOSE");
//     return false;
//   }
  
//   // Wait for server response
//   Serial.println("\nWaiting for server response...");
//   delay(3000);
  
//   while (sim800.available()) {
//     Serial.write(sim800.read());
//   }
  
//   // Close connection
//   sim800.println("AT+CIPCLOSE");
//   delay(1000);
  
//   Serial.println("\n=== Data sent successfully! ===\n");
//   return true;
// }

// void setup() {
//   Serial.begin(115200);
//   neo7m.begin(9600);  // Start GPS immediately
  
//   delay(2000);
//   Serial.println("\n=== High-Frequency GPS Data Logger ===");
//   Serial.println("Collecting maximum GPS points from moving vehicle");
  
//   // Initialize SIM800
//   if (!initSIM800()) {
//     Serial.println("Failed to initialize SIM800");
//     Serial.println("Will continue collecting GPS data...");
//   }
  
//   Serial.println("System ready! Collecting GPS data...\n");
//   lastSendTime = millis();
//   readingCount = 0;
// }

// void loop() {
//   unsigned long currentTime = millis();
  
//   // Continuously collect GPS data (runs as fast as possible)
//   collectGPSData();
  
//   // Check if it's time to send data (every 30 seconds)
//   if (currentTime - lastSendTime >= sendInterval) {
//     Serial.println("\n=== 30 seconds elapsed ===");
    
//     // Send all collected data to server
//     if (sendDataToServer()) {
//       // Clear buffer after successful send
//       readingCount = 0;
//       Serial.println("Buffer cleared. Starting new collection cycle...\n");
//     } else {
//       Serial.println("Send failed. Will retry in next cycle.");
//       Serial.println("Keeping existing data and continuing collection...\n");
//     }
    
//     // Reset timer
//     lastSendTime = currentTime;
//   }
  
//   // Show status every 5 seconds
//   static unsigned long lastStatus = 0;
//   if (currentTime - lastStatus >= 5000) {
//     unsigned long remaining = sendInterval - (currentTime - lastSendTime);
//     Serial.print("[Status] Readings: ");
//     Serial.print(readingCount);
//     Serial.print("/");
//     Serial.print(MAX_READINGS);
//     Serial.print(" | Next send in: ");
//     Serial.print(remaining / 1000);
//     Serial.println("s");
//     lastStatus = currentTime;
//   }

// }
