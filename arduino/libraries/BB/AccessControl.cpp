#include <Arduino.h>
#include "AccessControl.h"
#include "pitches.h"
#include <Adafruit_NeoPixel.h>

AccessControl::AccessControl(int relayPin, int buzzerPin, int ledPin, int debugLedPin) : ledStrip(1, ledPin, NEO_GRB + NEO_KHZ800)
{
  _relayPin = relayPin;
  _buzzerPin = buzzerPin;
  _ledPin = ledPin;
  _debugLedPin = debugLedPin;
  pinMode(_relayPin, OUTPUT);
  pinMode(_buzzerPin, OUTPUT);
  pinMode(_debugLedPin, OUTPUT);
  
  //Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, _ledPin, NEO_GRB + NEO_KHZ800);
  ledStrip.begin();
}

//Control the output relay
void AccessControl::activateRelay(int onOff) {
  digitalWrite(_relayPin, onOff);
}

//Is the system active
boolean AccessControl::isSystemActive() {
  return _systemActive;
}

//When was the system activated
unsigned long AccessControl::getSystemActiveAt() {
  return _systemActiveAt;
}

//Activate the system and store the active time
void AccessControl::setSystemActive() {
  _systemActiveAt = millis();
  _systemActive = true;
}

//Deactive - turn off the relay
void AccessControl::deactivateSystem() {
  _systemActive = false;
}

//Remove the active tag from memory
void AccessControl::clearActiveTag() {
  memset(&activeTagString[0], 0, sizeof(activeTagString));
}

//Control the PCB debug light
void AccessControl::debugLED(int onOff) {
    digitalWrite(_debugLedPin, onOff);
}

//Briefly flash the debug light
void AccessControl::flashDebugLight() {
  debugLED(1);
  delay(100);
  debugLED(0);
}

//Feedback Tones
void AccessControl::failureTone() {
    failureLight();
    
    tone(_buzzerPin, NOTE_A5);
    delay(250);
    noTone(_buzzerPin);
    tone(_buzzerPin, NOTE_C4);
    delay(250);
    noTone(_buzzerPin);
    
}
void AccessControl::ackTone() {
    ackLight();
  
    tone(_buzzerPin, NOTE_A4);
    delay(200);
    noTone(_buzzerPin);
}
void AccessControl::successTone() {
    successLight();
    
    tone(_buzzerPin, NOTE_C4);
    delay(250);
    noTone(_buzzerPin);
    tone(_buzzerPin, NOTE_A4);
    delay(250);
    noTone(_buzzerPin);
}
bool AccessControl::isTimeSet() {
	return _time_set;
}
void AccessControl::recordTimeSet() {
	_time_set = true;
}

//Feedback Lights
void AccessControl::successLight() {
    ledStrip.setPixelColor(0, ledStrip.Color(255, 0, 0));
    ledStrip.show();
}
void AccessControl::standbyLight() {
    ledStrip.setPixelColor(0, ledStrip.Color(255, 255, 0));
    ledStrip.show();
}
void AccessControl::failureLight() {
    ledStrip.setPixelColor(0, ledStrip.Color(0, 255, 0));
    ledStrip.show();
}
void AccessControl::ackLight() {
    ledStrip.setPixelColor(0, ledStrip.Color(0, 0, 255));
    ledStrip.show();
}



