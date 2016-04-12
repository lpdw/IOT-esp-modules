#include <Arduino.h>

/**
 * @file OTA-mDNS-SPIFFS.ino
 * 
 * @author Pascal Gollor (http://www.pgollor.de/cms/)
 * @date 2015-09-18
 * 
 * changelog:
 * 2015-10-22: 
 * - Use new ArduinoOTA library.
 * - loadConfig function can handle different line endings
 * - remove mDNS studd. ArduinoOTA handle it.
 * 
 */

// includes
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <ArduinoOTA.h>


/**
 * @brief mDNS and OTA Constants
 * @{
 */
#define HOSTNAME "ESP8266-OTA-" ///< Hostename. The setup function adds the Chip ID at the end.
/// @}

/**
 * @brief Default WiFi connection information.
 * @{
 */
String chipid = String(ESP.getChipId(), HEX);
String __ap_default_ssid = "esp8266-"+chipid;
const char* ap_default_ssid = (const char *)__ap_default_ssid.c_str();
const char* ap_default_psk = ap_default_ssid;
/// @}

/**
 * Telnet
 */
const uint16_t aport = 23;
WiFiServer TelnetServer(aport);
WiFiClient Telnet;

/// Uncomment the next line for verbose output over UART.
//#define SERIAL_VERBOSE

/**
 * Telnet debug ln function
 */
void printDebugln(const char* c = ""){
//Prints to telnet if connected
  Serial.println(c);
  
  if (!Telnet) {  // otherwise it works only once
    Telnet = TelnetServer.available();
  }
  if (Telnet && Telnet.connected()){
    Telnet.println(c);
  }
}

/**
 * Telnet debug ln function
 */
void printDebug(const char* c = ""){
//Prints to telnet if connected
  Serial.print(c);

  if (!Telnet) {  // otherwise it works only once
    Telnet = TelnetServer.available();
  }
  if (Telnet.connected()){
    Telnet.print(c);
  }
}


/**
 * @brief Read WiFi connection information from file system.
 * @param ssid String pointer for storing SSID.
 * @param pass String pointer for storing PSK.
 * @return True or False.
 * 
 * The config file have to containt the WiFi SSID in the first line
 * and the WiFi PSK in the second line.
 * Line seperator can be \r\n (CR LF) \r or \n.
 */
bool loadConfig(String *ssid, String *pass)
{
  // open file for reading.
  File configFile = SPIFFS.open("/cl_conf.txt", "r");
  if (!configFile)
  {
    printDebugln("Failed to open cl_conf.txt.");

    return false;
  }

  // Read content from config file.
  String content = configFile.readString();
  configFile.close();
  
  content.trim();

  // Check if ther is a second line available.
  int8_t pos = content.indexOf("\r\n");
  uint8_t le = 2;
  // check for linux and mac line ending.
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
    printDebugln("Infvalid content.");
    printDebugln(content.c_str());

    return false;
  }

  // Store SSID and PSK into string vars.
  *ssid = content.substring(0, pos);
  *pass = content.substring(pos + le);

  ssid->trim();
  pass->trim();

#ifdef SERIAL_VERBOSE
  printDebugln("----- file content -----");
  printDebugln(content);
  printDebugln("----- file content -----");
  printDebugln("ssid: " + *ssid);
  printDebugln("psk:  " + *pass);
#endif

  return true;
} // loadConfig


/**
 * @brief Save WiFi SSID and PSK to configuration file.
 * @param ssid SSID as string pointer.
 * @param pass PSK as string pointer,
 * @return True or False.
 */
bool saveConfig(String *ssid, String *pass)
{
  // Open config file for writing.
  File configFile = SPIFFS.open("/cl_conf.txt", "w");
  if (!configFile)
  {
    printDebugln("Failed to open cl_conf.txt for writing");

    return false;
  }

  // Save SSID and PSK.
  configFile.println(*ssid);
  configFile.println(*pass);

  configFile.close();
  
  return true;
} // saveConfig


/**
 * @brief Arduino setup function.
 */
void setup()
{
  String station_ssid = "LPDW-IOT";
  String station_psk = "LPDWIOTROUTER2015";

  Serial.begin(115200);
  TelnetServer.begin();
  TelnetServer.setNoDelay(true);
  
  delay(100);

  printDebugln("\r\n");
  printDebug("Chip ID: 0x");
  String __chipid_debug = String(ESP.getChipId(), HEX);
  char chipid_debug[] = ""; 
  __chipid_debug.toCharArray(chipid_debug, __chipid_debug.length()+1);
  printDebugln(chipid_debug);

  // Set Hostname.
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);

  // Print hostname.
  printDebug("Hostname: ");
  printDebugln(hostname.c_str());
  //printDebugln(WiFi.hostname());


  // Initialize file system.
  if (!SPIFFS.begin())
  {
    printDebugln("Failed to mount file system");
    return;
  }

  // Load wifi connection information.
  if (! loadConfig(&station_ssid, &station_psk))
  {
    station_ssid = "";
    station_psk = "";

    printDebugln("No WiFi connection information available.");
  }

  // Check WiFi connection
  // ... check mode
  if (WiFi.getMode() != WIFI_STA)
  {
    WiFi.mode(WIFI_STA);
    delay(10);
  }

  // ... Compare file config with sdk config.
  if (WiFi.SSID() != station_ssid || WiFi.psk() != station_psk)
  {
    printDebugln("WiFi config changed.");

    // ... Try to connect to WiFi station.
    WiFi.begin(station_ssid.c_str(), station_psk.c_str());

    // ... Pritn new SSID
    printDebug("new SSID: ");
    printDebugln(WiFi.SSID().c_str());

    // ... Uncomment this for debugging output.
    //WiFi.printDiag(Serial);
  }
  else
  {
    // ... Begin with sdk config.
    WiFi.begin();
  }

  printDebugln("Wait for WiFi connection.");

  // ... Give ESP 10 seconds to connect to station.
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000)
  {
    Serial.write('.');
    //printDebug(WiFi.status());
    delay(500);
  }
  printDebugln();

  // Check connection
  if(WiFi.status() == WL_CONNECTED)
  {
    // ... print IP Address
    printDebug("IP address: ");
    String localip = WiFi.localIP().toString();
    printDebugln(localip.c_str());
  }
  else
  {
    printDebugln("Can not connect to WiFi station. Go into AP mode.");
    
    // Go into software AP mode.
    WiFi.mode(WIFI_AP);

    delay(10);

    WiFi.softAP(ap_default_ssid, ap_default_psk);//ap_default_ssid = ap_default_psk

    printDebug("IP address: ");
    String apip = WiFi.softAPIP().toString();
    printDebugln(apip.c_str());
  }

  // Start OTA server.
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.setPassword("0123456789");
  ArduinoOTA.begin();
}


/**
 * @brief Arduino loop function.
 */
void loop()
{
  // Handle OTA server.
  ArduinoOTA.handle();
  yield();
  printDebugln("Test telnet");
  delay(2000);
}

