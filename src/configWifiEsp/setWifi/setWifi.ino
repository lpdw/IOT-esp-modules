#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "FS.h"

String config[5] = "";
bool isServer = false;

char ssid[] = "SSIDEXEMPLEMODULE";
char password[] = "PASSWORDEXEMPLEMODULE";

ESP8266WebServer server(80);

bool readConfig(){
  File f = SPIFFS.open("/config.txt", "r");
  if (!f) {
      Serial.println("file open failed");
      return false;
  }else{
    for(int i = 0; i < 2; i++){
      String s=f.readStringUntil('\n');
      config[i] = s;
      Serial.println("Line : " + i);
      Serial.println(s);
    }
    if(config[0].length() == 0 || config[1].length() == 0 ){
      Serial.println("Config not ok");
      return false;
    }else{
     return true; 
    }
  }
}

void setWifiAccesPoint(){
  Serial.println("Configuring access point...");
  WiFi.disconnect();
  delay(100);
  WiFi.softAP("AutoConfigWifi", "1234567890");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
}

void setServer(){
  isServer = true;
  server.on("/configwifi", HTTP_POST, handleConfig);
  server.begin();
  Serial.println("HTTP server started");
}

void handleConfig(){
  int args = server.args();
  String requestData;
  if(server.argName(0) != "ssid" || server.argName(1) != "password" ){
    server.send(400, "text/plain", "Arguments must be \"ssid\" or \"password\"");
  }else{
  for (uint8_t i = 0; i < server.args(); i++) {
    String param = server.argName(i);
    requestData += " " + param + ": " + server.arg(i) + "\n";
  }
  Serial.println(requestData);
  String __ssid = server.arg(0);
  String __password = server.arg(1);
  __ssid.toCharArray(ssid, __ssid.length()+1);
  Serial.println("*******");
  Serial.println(ssid);
  __password.toCharArray(password, __password.length()+1);
  Serial.println("--------");
  Serial.println(ssid);
  Serial.println(password);
  Serial.println("--------");
  server.send(200, "text/plain", requestData);
  setWifiClient();
  }
}

void setWifiClient(){
  Serial.println("Set client");
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  
  SPIFFS.begin();
  if(readConfig()){
    setWifiClient();
  }else{
    setWifiAccesPoint();
    setServer();
  }
}

void loop() {
  if(isServer){
    server.handleClient();    
  }
}
