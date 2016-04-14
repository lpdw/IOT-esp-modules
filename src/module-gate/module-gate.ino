#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include "FS.h"
#include <Servo.h>
#include "pitches.h"

const char* ssid;
const char* password;
String uuid = String(ESP.getChipId(), HEX);
String __ap_default_ssid = "tchou-tchou-module-"+uuid;
const char* ap_default_ssid = (const char *)__ap_default_ssid.c_str();
const char* ap_default_psk = ap_default_ssid;

/* if module is output module (0) or input module (1) */
String type = "1";
/* Description of the module */
String label = "Module barrière";
String ipHub;

/* Button flash for apmode */
const int buttonPin = 0;
int buttonState = 0; 

/* Beeper */
// beeperPin
int beeperPin = 15;

int ledState = LOW; 
unsigned long previousMillis = 0;
const long interval = 500;

ESP8266WebServer server(80);

// create servo object to control a servo
Servo gateservo;

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
  server.on("/configwifi", HTTP_POST, handleConfig);
  server.on("/gate/open", HTTP_POST, openGate);
  server.on("/gate/close", HTTP_POST, closeGate);
  server.on("/status", HTTP_GET, statusGate);
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
      digitalWrite(LED_BUILTIN, HIGH);
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
  String actions[3][4] = {
    {uuid,"POST","/gate/open","Ouvrir la barrière (0 deg)"},
    {uuid,"POST","/gate/close","Fermer la barrière (180 deg)"},
    {uuid,"GET","/status","Status de la barrière"},
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

void initGate(){
  gateservo.attach(5);
  gateservo.write(0);
}

void openGate(){
  int pos;
  server.send(202, "text/plain", "OK : Gate opening...");
  for(pos = 100; pos >= 0; pos -= 1)     // goes from 90 degrees to 0 degrees 
  {                                
    gateservo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(15);                       // waits 15ms for the servo to reach the position 
  }
}

void closeGate(){
  int pos;
  server.send(200, "text/plain", "OK : Gate closing...");
  for(pos = 0; pos <= 100; pos += 1) // goes from 0 degrees to 90 degrees 
  {                                  // in steps of 1 degree 
    gateservo.write(pos);              // tell servo to go to position in variable 'pos' 
    unsigned long currentMillis = millis();
    if(currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;   
      if (ledState == LOW) {
        tone(15, NOTE_C6, 500);
        ledState = HIGH;
      } else {
        ledState = LOW;
      }
      digitalWrite(LED_BUILTIN, ledState);
    }
    delay(15);                        // waits 15ms for the servo to reach the position 
  }
  digitalWrite(LED_BUILTIN, LOW);
  //delay before opening gate again
  ringGate(2000);
  openGate();
}

void ringGate(int interval){
  int time;
  for (time = 0; time <= 3; time += 1) {
    tone(15, 1, 500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    tone(15, NOTE_C6, 500);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
  }
  digitalWrite(LED_BUILTIN, LOW);
}

void statusGate(){
    int pos = gateservo.read();
    String gate_status = "";
    if ( pos >= 95 && pos <= 105 ) {
      gate_status = "closed";
    } else if ( pos >= 0 && pos <= 5 ){
      gate_status = "opened";
    } else {
      gate_status = "wrong position : "+String(pos);
    }
    server.send(200, "text/plain", gate_status);
}

void setup() {
  delay(1000);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(buttonPin, INPUT);
  Serial.begin(115200);
  
  SPIFFS.begin();
  if(readConfig()){
    setWifiClient();
    setServer();
  }else{
    setWifiAccesPoint();
    setServer();
  }
  initGate();
}

void loop() {
  buttonState = digitalRead(buttonPin);
  // If flash buttion pressed, goto AP mode
  if (buttonState == LOW) {
    digitalWrite(LED_BUILTIN, HIGH);
    setWifiAccesPoint();
  }
  server.handleClient();
}
