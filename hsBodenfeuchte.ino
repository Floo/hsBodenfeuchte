

#include <RFduinoBLE.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include <stdint.h>
#include "timer.h"

#define PIN_PWR 4
#define PIN_BATT 5
#define PIN_LED 3
#define PIN_CNT 2

//uint64_t hibernate = MINUTES(2);
uint64_t hibernate = SECONDS(5);
unsigned long humValue = 0;
unsigned long oldValue = 0;
unsigned long tmpValue = 0;
unsigned long tmp2Value = 0;
unsigned long maxHumValue = 0;
unsigned int humPreCounter = 0;
unsigned int humAverageCounter = 0;
unsigned int advertiseLevel = 0;
bool humValid = false;
bool finished = false;
bool advertising = false;
bool quit = false;
bool cntIntEnable = false;
float battVoltage = 0;
char sendBuffer[128];
Timer timer;

float readBatteryVoltage() {
  int sensorValue = analogRead(PIN_BATT);
  Serial.print("Batteriespannung: ");
  Serial.println(sensorValue * (3.3 / 1023.0));
  return sensorValue * (3.3 / 1023.0);
}

void startHumidity() {
  humPreCounter = 0;
  humAverageCounter = 0;
  humValid = false;
  humValue = 0;
  oldValue = 0;
  digitalWrite(PIN_PWR, LOW);
  cntIntEnable = true;
  Serial.println("Messung gestartet");
}

void stopHumidity() {
  cntIntEnable = false;
  digitalWrite(PIN_PWR, HIGH);
  Serial.println("Messung beendet");
}

int readHumidity(uint32_t ulPin) {
  tmpValue = micros(); 
  
  //Einschwingvorgang
  if (humPreCounter++ < 10 || !cntIntEnable) {
    return 0;
  }
  
  //erste Messung
  if (humAverageCounter++ == 0) {  
    oldValue = tmpValue;
    return 0;
  }
  
  if (tmpValue > oldValue) { //nur wenn kein Überlauf aufgetreten
    tmp2Value = tmpValue - oldValue;
    //averaging
    if (humValue == 0) {
      humValue = tmp2Value;
    } else {
      humValue = (178 * tmp2Value + (256 - 178) * humValue )/ 256;
    }
    if (humAverageCounter++ > 10) {
      stopHumidity();
      humValid = true;
      if (maxHumValue < humValue) {
        maxHumValue = humValue;
      }
    }
  }
  oldValue = tmpValue;
  return 0;
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_PWR, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  pinMode(PIN_BATT, INPUT);
  pinMode(PIN_CNT, INPUT);
  RFduino_pinWake(PIN_CNT, HIGH); 
  RFduino_pinWakeCallback(PIN_CNT, HIGH, readHumidity);
  startHumidity();
}

void loop() {
  //Serial.println("Schleife gestartet");
  //kann vielleicht entfallen, wenn pinWoke nicht verwendet werden soll
  if (RFduino_pinWoke(PIN_CNT)) {
    RFduino_resetPinWake(PIN_CNT);
  }
  
  if (finished) {
    finished = false;
    digitalWrite(PIN_LED, LOW);
    RFduino_ULPDelay(hibernate);
    digitalWrite(PIN_LED, HIGH);
    startHumidity();
    battVoltage = readBatteryVoltage();
  }
  
  if (humValid) {
    humValid = false;
    advertiseLevel = 0;
    RFduinoBLE.txPowerLevel = -20;
    snprintf(sendBuffer, 128, "humValue: %lu, humMaxValue: %lu, battery: %f\n", humValue, maxHumValue, battVoltage); 
    Serial.println(sendBuffer);
    Serial.println("waiting for connect");
    RFduinoBLE.begin();
    RFduinoBLE.send(sendBuffer, strlen(sendBuffer));
    timer.setTimeout(2000);
    advertising = true;
  }
  
  if (advertising && timer.timeout()) {
    if (advertiseLevel < 7) {
      advertiseLevel++;
      RFduinoBLE.txPowerLevel = -20 + advertiseLevel * 4;
    } else {
      advertising = false;
      Serial.println("Sending Timeout");
      RFduinoBLE.end();
      finished = true;
    }
  }
  
  if (advertising && quit) {
    advertising = false;
    quit = false;
    RFduinoBLE.end();
    Serial.println("Sending successful");
    finished = true;
  }
}

void RFduinoBLE_onReceive(char *data, int len) {
  if (memcmp(data, "OK", 2)) {
    quit = true;
  } else if (memcmp(data, "Pause:", 6)) {
    hibernate = MINUTES(atoi(data + 7));
    quit = true;
  }
}


