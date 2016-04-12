#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

#define USE_SERIAL Serial

ESP8266WiFiMulti WiFiMulti;
int sensorPin = A0;    // select the input pin for the potentiometer
int ledPin = 13;      // select the pin for the LED
int sensorValue = 0;  // variable to store the value coming from the sensor
int crossingTrain[5] = {0};
int i = 0;
int itrArray = 0;
bool flag = true;
double moyenneLuminosite = 0.0;
const char* sensorID = "123456789";

void setup() {
  Serial.begin(115200);
  // declare the ledPin as an OUTPUT:
  pinMode(ledPin, OUTPUT);

  for(uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }

    WiFiMulti.addAP("LPDW-IOT", "LPDWIOTROUTER2015");
}

//Clean un tableau d'entiers
void reinitArray(int arrayToClean[], int size){
  for(int p = 0; p <= size; p++)
  {
    arrayToClean[p] = 0; 
  }
}

//Calcul la moyenne 
double moyenneTableau(int tableau[], int tailleTableau){
    int j = 0, somme = 0;
    double moyenne = 0;
 
    for(j = 0; j < tailleTableau; j++)
    {
        somme = somme + tableau[j];
    }
 
    moyenne = somme / j;

    return moyenne;
}

void reinitFlag (){
    flag = false;
    delay(3000);
    flag = true;
}

void loop() {
  // read the value from the sensor:
  sensorValue = analogRead(sensorPin);

  //If the train is there, push value in crossingTrain array 
  if(sensorValue < 1030 && sensorValue > 900 && itrArray <= 5){
    crossingTrain[itrArray] = sensorValue;
    itrArray++;

    if(itrArray == 5){
      moyenneLuminosite = moyenneTableau(crossingTrain, 5);

      if(moyenneLuminosite <1000 && moyenneLuminosite > 900 && flag == true){
        digitalWrite(ledPin, HIGH);

        if((WiFiMulti.run() == WL_CONNECTED)) {
        
                HTTPClient http;
                USE_SERIAL.print("[HTTP] begin...\n");
                // configure traged server and url
                http.begin("http://192.168.1.222/data"); //HTTP
        
                USE_SERIAL.print("[HTTP] POST...\n");
                // start connection and send HTTP header
                http.addHeader("Content-Type", "application/x-www-form-urlencoded");
                //http.POST("title=foo&value=1");
                http.POST(sensorID);
                http.writeToStream(&Serial);
                http.end();
            }
        reinitFlag();
      }
      reinitArray(crossingTrain, 5);
      itrArray = 0;
    }
  } 
  else{
    digitalWrite(ledPin, LOW);
  }
  delay(5);
}
