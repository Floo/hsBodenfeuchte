

#include <RFduinoBLE.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include <stdint.h>
#include "timer.h"

#define PIN_PWR 4
#define PIN_BATT 5
#define PIN_LED 3
#define PIN_CNT 2

#define SEND_TIMEOUT 2000
#define DEBUG

typedef struct __attribute__((packed)) {
  unsigned long humValue;
  unsigned long maxHumValue;
  float battVoltage;
} sendStruct;

//uint64_t hibernate = MINUTES(2);
uint64_t hibernate = SECONDS(5);
uint64_t tmpHibernate;
unsigned long oldValue = 0;
unsigned long tmpValue = 0;
unsigned long tmp2Value = 0;
unsigned int humPreCounter = 0;
unsigned int humAverageCounter = 0;
unsigned int advertiseLevel = 0;
unsigned int timeoutCounter = 0;
bool humValid = false;
bool finished = false;
bool advertising = false;
bool quit = false;
bool cntIntEnable = false;
#ifdef DEBUG
char sendBuffer[128];
#endif
Timer timer;

sendStruct messwerte;

float readBatteryVoltage() {
  int sensorValue = analogRead(PIN_BATT);
#ifdef DEBUG
  Serial.print("Batteriespannung: ");
  Serial.println(sensorValue * (3.3 / 1023.0));
#endif 
  return sensorValue * (3.3 / 1023.0);
}

void startHumidity() {
  humPreCounter = 0;
  humAverageCounter = 0;
  humValid = false;
  messwerte.humValue = 0;
  oldValue = 0;
  digitalWrite(PIN_PWR, LOW);
  digitalWrite(PIN_LED, HIGH);
  cntIntEnable = true;
#ifdef DEBUG
  Serial.println("Messung gestartet");
#endif
}

void stopHumidity() {
  cntIntEnable = false;
  digitalWrite(PIN_PWR, HIGH);
#ifdef DEBUG
  Serial.println("Messung beendet");
#endif
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
    if (messwerte.humValue == 0) {
      messwerte.humValue = tmp2Value;
    } else {
      messwerte.humValue = (178 * tmp2Value + (256 - 178) * messwerte.humValue )/ 256;
    }
    if (humAverageCounter++ > 10) {
      stopHumidity();
      humValid = true;
      if (messwerte.maxHumValue < messwerte.humValue) {
        messwerte.maxHumValue = messwerte.humValue;
      }
    }
  }
  oldValue = tmpValue;
  return 0;
}

void setup() {
#ifdef DEBUG
  Serial.begin(9600);
#endif
  pinMode(PIN_PWR, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  pinMode(PIN_BATT, INPUT);
  pinMode(PIN_CNT, INPUT);
  RFduino_pinWake(PIN_CNT, HIGH); 
  RFduino_pinWakeCallback(PIN_CNT, HIGH, readHumidity);
  messwerte.maxHumValue = 0;
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
    startHumidity();
    messwerte.battVoltage = readBatteryVoltage();
  }
  
  if (humValid) {
    humValid = false;
    if (advertiseLevel > 0)
      advertiseLevel--;  //eine Stufe runterschalten
    RFduinoBLE.txPowerLevel = -20 + advertiseLevel * 4;
#ifdef DEBUG
    snprintf(sendBuffer, 128, "humValue: %lu, humMaxValue: %lu, battery: %f\n",
              messwerte.humValue, messwerte.maxHumValue, messwerte.battVoltage); 
    Serial.println(sendBuffer);
    Serial.println("waiting for connect");
#endif
    RFduinoBLE.begin();
    RFduinoBLE.send((char*)&messwerte, sizeof(messwerte));
    timer.setTimeout(SEND_TIMEOUT);
    advertising = true;
  }
  
  if (advertising && timer.timeout()) {
    if (advertiseLevel < 7) {
      advertiseLevel++;
      RFduinoBLE.txPowerLevel = -20 + advertiseLevel * 4;
      timer.setTimeout(SEND_TIMEOUT);
    } else {
      advertising = false;
      if (timeoutCounter < 3) {
        timeoutCounter++;
        tmpHibernate = hibernate;
        hibernate = SECONDS(5);
      }
#ifdef DEBUG
      Serial.println("Sending Timeout");
#endif
      RFduinoBLE.end();
      finished = true;
    }
  }
  
  if (advertising && quit) {
    advertising = false;
    quit = false;
    if (timeoutCounter > 0) {
      timeoutCounter = 0;
      hibernate = tmpHibernate;
    }
    RFduinoBLE.end();
#ifdef DEBUG
    Serial.println("Sending successful");
#endif
    finished = true;
  }
}

void RFduinoBLE_onReceive(char *data, int len) {
  if (strncmp(data, "OK", 2)) {
#ifdef DEBUG
    Serial.println("receive OK");
#endif
    quit = true;
  } else if (strncmp(data, "P", 1)) {
    char *enddata = strstr(data, "\n");
    enddata = '\0';
    hibernate = MINUTES(atoi(data + 1));
    timeoutCounter = 0;
#ifdef DEBUG
    Serial.print("receive P: ");
    Serial.println(atoi(data + 1));
#endif
    quit = true;
  }
}


