#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "FS.h"

String config[5] = "";
bool isServer = false;

char ssid[] = "SSIDEXEMPLEMODULE";
char password[] = "PASSWORDEXEMPLEMODULE";
String uuid = String(ESP.getChipId(), HEX);
String __ap_default_ssid = "esp8266-"+uuid;
const char* ap_default_ssid = (const char *)__ap_default_ssid.c_str();
const char* ap_default_psk = ap_default_ssid;
/* if module is output module (0) or input module (1) */
int type = 0;
/* Description of the module */
String label = "Test Module";
String ipHub;

ESP8266WebServer server(80);

/*Open file config*/
bool readConfig(){
  File f = SPIFFS.open("/config.txt", "r");
  if (!f) {
      Serial.println("file open failed");
      return false;
  }else{
    String content = f.readString();
    f.close();
    content.trim();
    
    int8_t pos = content.indexOf("\r\n");
    uint8_t le = 2;
    if (pos == -1)
    {
      le = 1;
      pos = content.indexOf("\n");
      if (pos == -1)
      {
        pos = content.indexOf("\r");
      }
    }
  
    // If there is no second line: Some information is missing.
    if (pos == -1)
    {
      Serial.println("Infvalid content.");
      Serial.println(content);
      return false;
    }
    Serial.println("Ok config");
    String test = content.substring(0, pos);
    Serial.println(test);
    return false;
  }
}

/*Configuring Acces point with unique SSID*/
void setWifiAccesPoint(){
  Serial.println("Configuring access point...");
  WiFi.disconnect();
  delay(100);
  WiFi.softAP(ap_default_ssid, ap_default_psk);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
}

/* Configuring http server and listen for POST request on /configwifi */
void setServer(){
  isServer = true;
  server.on("/configwifi", HTTP_POST, handleConfig);
  server.begin();
  Serial.println("HTTP server started");
}

/* Handling POST request on /configwifi */
void handleConfig(){
  int args = server.args();
  String requestData;
  /*If request params are not SSID or PASSWORD return HTTP error 400 */
  if(server.argName(0) != "ssid" || server.argName(1) != "password" ){
    server.send(400, "text/plain", "Arguments must be \"ssid\" or \"password\"");
  }else{
    /* if everything is OK, send response 200 with the data pass with the request*/
    for (uint8_t i = 0; i < server.args(); i++) {
      String param = server.argName(i);
      requestData += " " + param + ": " + server.arg(i) + "\n";
    }
    server.send(200, "text/plain", requestData);
    Serial.println(requestData);
    /* Store request params with  temporary varaibles*/
    String __ssid = server.arg(0);
    String __password = server.arg(1);
    /* Convert String to char array because wifi.begin() method accept only char array */
    __ssid.toCharArray(ssid, __ssid.length()+1);
    __password.toCharArray(password, __password.length()+1);
    saveConfig();
    setWifiClient();
  }
}

/*Connect module to hub with the data stored before*/
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

void saveConfig(){
  File f = SPIFFS.open("/config.txt", "w");
  Serial.println(f);
  if (!f) {
      Serial.println("file open failed");
  }else{
    Serial.println("Save config");
    f.println(ssid);
    f.println(password);
    f.println(uuid);
    f.println(type);
    f.println(label);
    Serial.println("config");
    for(int i = 0; i < 5; i++){
      String s=f.readStringUntil('\n');
      Serial.println(s);
    }
  }
  f.close();
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
