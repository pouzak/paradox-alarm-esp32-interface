#include <WiFi.h>
#include <ArduinoOTA.h>
#include "credentials.h"

//how many clients should be able to telnet to this ESP32
#define MAX_SRV_CLIENTS 3
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// IPAddress staticIP(10,0,0,51);
// IPAddress gateway(10,0,0,1);
// IPAddress subnet(255, 255, 255, 0);
// IPAddress dns(1,1,1,1);

WiFiServer server(23);
WiFiClient serverClients[MAX_SRV_CLIENTS];

 #define RXD2 17
 #define TXD2 16
 
 #define RXD3 25
 #define TXD3 26

void setup() {



  Serial.begin(9600);
  Serial.println("\nConnecting");

  WiFi.mode(WIFI_STA);


  // Configures static IP address
  // if (!WiFi.config(staticIP, gateway, subnet, dns, dns)) {
  //   Serial.println("STA Failed to configure");
  // }

  WiFi.begin(ssid, password);
  // // Connect to Wi-Fi network with SSID and password
  // Serial.print("Connecting to ");
  // Serial.println(ssid);
  // WiFi.begin(ssid, password);
  // while (WiFi.status() != WL_CONNECTED) {
  //   Serial.print(WiFi.status());
  //   delay(500);
  //   Serial.print(".");
  // }



  Serial.println("Connecting Wifi ");
  for (int loops = 10; loops > 0; loops--) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.print("WiFi connected ");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      break;
    }
    else {
      Serial.println(loops);
      delay(1000);
    }
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connect failed");
    delay(1000);
    ESP.restart();
  }

  //start UART and the server
  Serial2.begin(9600,SERIAL_8N1,RXD2, TXD2);
  Serial1.begin(9600, SERIAL_8N1, RXD3, TXD3);
  server.begin();
  server.setNoDelay(true);

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.print("Ready! Use 'telnet ");
  Serial.print(WiFi.localIP());
  Serial.println(" 23' to connect");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

}
unsigned long previousMillis = 0;
unsigned long interval = 30000;


void loop() {
  uint8_t i;
  unsigned long currentMillis = millis();
  
  ArduinoOTA.handle(); 

    //check if there are any new clients
    if (server.hasClient()){
      for(i = 0; i < MAX_SRV_CLIENTS; i++){
        //find free/disconnected spot
        if (!serverClients[i] || !serverClients[i].connected()){
          if(serverClients[i]) serverClients[i].stop();
          serverClients[i] = server.available();
          if (!serverClients[i]) Serial.println("available broken");
          Serial.print("New client: ");
          Serial.print(i); Serial.print(' ');
          Serial.println(serverClients[i].remoteIP());
          break;
        }
      }
      if (i >= MAX_SRV_CLIENTS) {
        //no free/disconnected spot so reject
        server.available().stop();
      }
    }
    //check clients for data
    for(i = 0; i < MAX_SRV_CLIENTS; i++){
      if (serverClients[i] && serverClients[i].connected()){
        if(serverClients[i].available()){
          //get data from the telnet client and push it to the UART
          while(serverClients[i].available()) Serial2.write(serverClients[i].read());
        }
      }
      else {
        if (serverClients[i]) {
          serverClients[i].stop();
        }
      }
    }
    //check UART for data
    if(Serial2.available()){
      size_t len = Serial2.available();
      uint8_t sbuf[len];
      Serial2.readBytes(sbuf, len);

      //passtrough data to serial1
      Serial1.write(sbuf, len);

      Serial.write(sbuf,len);
      //push UART data to all connected telnet clients
      for(i = 0; i < MAX_SRV_CLIENTS; i++){
        if (serverClients[i] && serverClients[i].connected()){
          serverClients[i].write(sbuf, len);
          delay(1);
        }
      }
    }
    if(Serial1.available()){
      size_t len = Serial1.available();
      uint8_t sbuf[len];
      Serial1.readBytes(sbuf, len);

      //passtrough data to serial2
      Serial2.write(sbuf, len);
      
      Serial.write(sbuf,len);
    }
    if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) { 
    Serial.println("WiFi not connected!");
    if (server.hasClient()){
      for(i = 0; i < MAX_SRV_CLIENTS; i++) {
        if (serverClients[i]) serverClients[i].stop();
        }
    }
      WiFi.disconnect();
      WiFi.reconnect();
      previousMillis = currentMillis;
    // delay(1000);
    // ESP.restart();
  }
}