#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include "FS.h"

/*Configuring Acces point with unique SSID*/
void setWifiAccesPoint(){
  Serial.println("Configuring access point...");
  WiFi.disconnect();
  delay(100);
  WiFi.softAP("accesPointTrain", "accesPointTrain");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
}


void setup() {
  Serial.begin(115200);
  setWifiAccesPoint();
}

void loop() {
}
