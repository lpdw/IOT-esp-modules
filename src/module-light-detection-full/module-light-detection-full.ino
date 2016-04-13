#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include "FS.h"

const char* ssid;
const char* password;
String uuid = String(ESP.getChipId(), HEX);
String __ap_default_ssid = "tchou-tchou-module-"+uuid;
const char* ap_default_ssid = (const char *)__ap_default_ssid.c_str();
const char* ap_default_psk = ap_default_ssid;

/* if module is output module (0) or input module (1) */
String type = "0";
/* Description of the module */
String label = "Test Module";
String ipHub;

int sensorPin = A0;    // select the input pin for the potentiometer
int ledPin = 13;      // select the pin for the LED
int sensorValue = 0;  // variable to store the value coming from the sensor
int crossingTrain[5] = {0};
int i = 0;
int itrArray = 0;
bool flag = true;
double averageBrightness = 0.0;
String dataSend;

ESP8266WebServer server(80);

/*Open file config*/
bool readConfig(){
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }
  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  ssid = json["ssid"];
  password = json["password"];

  if(sizeof(json["ssid"]) < 0 || sizeof(json["password"]) < 6){
    Serial.println("too short");
    return false;
  }

  Serial.print("Loaded SSID: ");
  Serial.println(ssid);
  Serial.print("Loaded PASSWORD: ");
  Serial.println(password);

  configFile.close();
  return true;
}

/*Configuring Acces point with unique SSID*/
void setWifiAccesPoint(){
  Serial.println("Configuring access point...");
  Serial.println(ap_default_ssid);
  WiFi.disconnect();
  delay(100);
  WiFi.softAP(ap_default_ssid, ap_default_psk);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
}

/* Configuring http server and listen for POST request on /configwifi */
void setServer(){
  server.on("/configwifi", HTTP_POST, handleConfig);
  server.begin();
  Serial.println("HTTP server started");
}

/* Handling POST request on /configwifi */
void handleConfig(){
  int args = server.args();
  String requestData;
  /*If request params are not SSID or PASSWORD or IP return HTTP error 400 */
  if(server.argName(0) != "ssid" || server.argName(1) != "password" || server.argName(2) != "ip"){
    server.send(400, "text/plain", "Arguments must be \"ssid\", \"password\" and \"ip\"");
  }else{
    /* if everything is OK, send response 200 with the data pass with the request*/
    for (uint8_t i = 0; i < server.args(); i++) {
      String param = server.argName(i);
      requestData += " " + param + ": " + server.arg(i) + "\n";
    }
    /*If ssid or password are too short return error http 400*/
    if(server.arg(0).length() < 6 || server.arg(1).length() < 6 ){
      server.send(400, "text/plain", "ssid or password send are too short");
    }else{
      server.send(202, "text/plain", requestData);
      Serial.println(requestData);
      /* Store request params with  temporary varaibles*/
      String __ssid = server.arg(0);
      String __password = server.arg(1);
      //Store hub ip
      ipHub = server.arg(2);
      /* Convert String to char array because wifi.begin() method accept only char array */
      ssid = (const char *)__ssid.c_str();
      password = (const char *)__password.c_str();
      saveConfig();
      setWifiClient();
      registerHub();
      if(type == "1"){
       registerActions(); 
      }
    }
  }
}

/*Connect module to hub with the data stored before*/
void setWifiClient(){
  Serial.println("Set client");
  delay(100);
  WiFi.disconnect();
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

bool saveConfig(){
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["ssid"] = ssid;
  json["password"] = password;
  json["uuid"] = uuid;
  json["type"] = type;
  json["label"] = label;
  json["iphub"] = ipHub;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  Serial.println("Config Done");
  return true;
}

void registerHub(){
  HTTPClient http;
  Serial.print("[HTTP] begin...\n");
  // configure traged server and url
  http.begin("http://"+ipHub+":3000/api/register"); //HTTP
  Serial.print("[HTTP] POST...\n");
  // start connection and send HTTP header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST("uuid="+uuid+"&type="+type+"&label="+label);
  http.writeToStream(&Serial);
  http.end();
}

/*ligth sensor*/
void reinitArray(int arrayToClean[], int size){
  for(int p = 0; p <= size; p++)
  {
    arrayToClean[p] = 0; 
  }
}

//Calcul la average 
double averageArray(int arrayValue[], int sizeArray){
    int j = 0, sum = 0;
    double average = 0;
 
    for(j = 0; j < sizeArray; j++)
    {
        sum = sum + arrayValue[j];
    }
    average = sum / j;
    return average;
}

void reinitFlag (){
    flag = false;
    delay(3000);
    flag = true;
}

void postData(String id, String value){
  HTTPClient http;
  Serial.print("[HTTP] begin...\n");
  // configure traged server and url
  http.begin("http://"+ipHub+"/data"); //HTTP

  Serial.print("[HTTP] POST...\n");
  // start connection and send HTTP header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST("uuid="+id+"&value="+value);
  http.writeToStream(&Serial);
  http.end();
  reinitFlag();
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  
  pinMode(ledPin, OUTPUT);
  SPIFFS.begin();
  if(readConfig()){
    setWifiClient();
    setServer();
  }else{
    setWifiAccesPoint();
    setServer();
  }
}

void loop() {
  server.handleClient();
   if(WiFi.status() == WL_CONNECTED) {
    sensorValue = analogRead(sensorPin);
    //If the train is there, push value in crossingTrain array 
    if(sensorValue < 1030 && sensorValue > 900 && itrArray <= 5){
      crossingTrain[itrArray] = sensorValue;
      itrArray++;
  
      if(itrArray == 5){
        averageBrightness = averageArray(crossingTrain, 5);
  
        if(averageBrightness <1000 && averageBrightness > 900 && flag == true){
          dataSend = "1";
          digitalWrite(ledPin, HIGH);
          postData(uuid, dataSend);
        }
        reinitArray(crossingTrain, 5);
        itrArray = 0;
      }
    } 
    else{
      dataSend = "0";
      digitalWrite(ledPin, LOW);
      postData(uuid, dataSend);
    }
   }
  delay(5);
}
