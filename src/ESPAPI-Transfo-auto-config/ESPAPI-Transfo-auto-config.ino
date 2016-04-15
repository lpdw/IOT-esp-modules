#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include "FS.h"

SoftwareSerial ArduinoSerial(14,12);

const char* ssid;
const char* password;
String uuid = String(ESP.getChipId(), HEX);
String __ap_default_ssid = "tchou-tchou-module-"+uuid;
const char* ap_default_ssid = (const char *)__ap_default_ssid.c_str();
const char* ap_default_psk = ap_default_ssid;

String inverse = "false";
bool first_read = true;
/* if module is output module (0) or input module (1) */
String type = "2";
/* Description of the module */
String label = "Transformateur Module";
String ipHub;


/* Button flash for apmode */
const int buttonPin = 0;
int buttonState = 0; 

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
  WiFi.disconnect();
  delay(100);
  WiFi.softAP(ap_default_ssid, ap_default_psk);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
}


/* Configuring http server and listen for POST request on /configwifi */
void setServer(){
  server.on("/power",HTTP_GET, handlePower);
  server.on("/power/strength", handlePowerStrength);
  server.on("/power/inverse", handlePowerInverse);
  server.on("/power/stop", HTTP_POST ,handlePowerShutTheFuckUp);
  server.on("/configwifi", HTTP_POST , handleConfig);

  server.onNotFound(handleNotFound);
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
      registerActions();
    }
  }
}


/*Connect module to hub with the data stored before*/
void setWifiClient(){
  Serial.println("Set client");
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    buttonState = digitalRead(buttonPin);
    // If flash buttion pressed, goto AP mode
    if (buttonState == LOW) {
      setWifiAccesPoint();
      break;
    }
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, LOW);
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

void registerActions(){
  String actions[6][4] = {
    {uuid,"GET","/power","power information"},
    {uuid,"GET","/power/strength","power strength information"},
    {uuid,"POST","/power/strength","set power strength"},
    {uuid,"GET","/power/inverse","power direction"},
    {uuid,"POST","/power/inverse","set power direction"},
    {uuid,"POST","/power/stop","Immediatly stop the train"}
  };

  int nbrActions = sizeof(actions)/sizeof(actions[0]);

  HTTPClient http;
  // configure traged server and url
  for(int i = 0; i < nbrActions; i++){
    Serial.print("[HTTP] begin...\n");
    http.begin("http://"+ipHub+":3000/api/action"); //HTTP
    Serial.print("[HTTP] POST...\n");
    // start connection and send HTTP header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.POST("uuid="+actions[i][0]+"&endpoint="+actions[i][2]+"&type_http="+actions[i][1]+"&description="+actions[i][3]);
    http.writeToStream(&Serial);
    http.end();
  }
}

// /power
void handlePower() {
    ArduinoSerial.print("/s");

    int response = 0;
    if((ArduinoSerial.available() > 0) && (response == 0)) {
      String value = "";
      value = ArduinoSerial.read();
      Serial.println("Valeur: "+value);
      server.send(200, "text/plain", "{ 'Strength':"+value+", 'inverse':"+inverse+"}");
      response=1;
    }
}

// /power/strength
void handlePowerStrength() {

  if (server.method() == HTTP_GET)
  {

    ArduinoSerial.print("/s");

    int response = 0;
    if((ArduinoSerial.available() > 0) && (response == 0)) {
      String value = "";
      value = ArduinoSerial.read();
        Serial.println("Valeur: "+value);


        server.send(200, "text/plain", value);
        response=1;

    }

  } else if((server.method() == HTTP_POST) || (server.method() == 6)){
    String value_set;
    for (uint8_t i = 0; i < server.args(); i++) {
      if(server.argName(i) == "strength"){
        value_set = server.arg(i);
      }
    }

    if(value_set.toInt() >= 0  && value_set.toInt() <= 100)
    {
      Serial.println("/s/"+value_set);
      ArduinoSerial.print("/s/"+value_set);

      server.send(202, "text/plain", value_set);
    } else {
      Serial.print("fail - state");
      Serial.println();
      server.send(400, "text/plain", "Error, check your requests.");
    }


  }

}

// /power/inverse
void handlePowerInverse() {
  if (server.method() == HTTP_GET)
  {
    server.send(200, "text/plain", inverse);
  } else if((server.method() == HTTP_POST) || (server.method() == 6)){
    (inverse == "false")?inverse="true":inverse="false";
    ArduinoSerial.write("/p/i");
    Serial.print("/p/i");
    
    server.send(202, "text/plain", inverse);
  }
}

// /power/shutthefuckup
void handlePowerShutTheFuckUp() {
  ArduinoSerial.write("/p/s");
  Serial.println("/p/s");
  server.send(202, "text/plain", "Stop");
  delay(500);
  ArduinoSerial.write("/s/0");
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void){

  delay(1000);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(buttonPin, INPUT);
  Serial.begin(115200);
  ArduinoSerial.begin(115200);

  SPIFFS.begin();
  if(readConfig()){
    setWifiClient();
    setServer();
  }else{
    setWifiAccesPoint();
    setServer();
  }
}

void loop(void){
  server.handleClient();
  if(first_read)
  {
    ArduinoSerial.print("/s");
    if(ArduinoSerial.available() > 0) {
      String value;
      value = ArduinoSerial.read();
      Serial.println("loop: "+value);
    }
    first_read = false;
  }


  buttonState = digitalRead(buttonPin);
  // If flash buttion pressed, goto AP mode
  if (buttonState == LOW) {
    digitalWrite(LED_BUILTIN, HIGH);
    setWifiAccesPoint();
  }

  
  buttonState = digitalRead(buttonPin);
  // If flash buttion pressed, goto AP mode
  if (buttonState == LOW) {
    digitalWrite(LED_BUILTIN, HIGH);
    setWifiAccesPoint();
  }

//  String msg;
//  if (ArduinoSerial.available() > 0) {
//       msg = ArduinoSerial.read();
//      Serial.println("loop "+msg);
//   }
}
