#include <Arduino.h>
#include "Network.h"

Network::Network(int ethernetBusyPin, int ethernetReadyPin, int onlineLedPin)
{
  _ethernetBusyPin = ethernetBusyPin;
  _ethernetReadyPin = ethernetReadyPin;
  _onlineLedPin = onlineLedPin;
  _hasReset = false;

  pinMode(ethernetBusyPin, INPUT);
  pinMode(ethernetReadyPin, INPUT);
  pinMode(onlineLedPin, OUTPUT);

  onlineLed(false);
}

Network::Network(int ethernetBusyPin, int ethernetReadyPin, int ethernetResetPin, int onlineLedPin)
{
  _ethernetBusyPin = ethernetBusyPin;
  _ethernetReadyPin = ethernetReadyPin;
  _ethernetResetPin = ethernetResetPin;
  _onlineLedPin = onlineLedPin;

  //Reset pin has weak pull-up - pull low to reset
  _hasReset = true;

  pinMode(ethernetBusyPin, INPUT);
  pinMode(ethernetReadyPin, INPUT);
  pinMode(ethernetResetPin, OUTPUT); //for now
  digitalWrite(ethernetResetPin, true);
  pinMode(onlineLedPin, OUTPUT);

  onlineLed(false);
}

//Regular check to make sure the network is configured
void Network::checkConfigure()
{
  if (!isNetworkSetup() && isNetworkReady()) {
    setupNetwork();
  }
}

//Startup network check and configure - long delay
void Network::startupCheckConfigure()
{
  Serial.begin(9600);

  uint16_t genericTimeCounter = millis();
  while (!digitalRead(_ethernetReadyPin)) {
    delay(50);
    if (millis() > (genericTimeCounter + 5000)) {
      return;
    }
  }
}

//The online LED
void Network::onlineLed(int online)
{
  digitalWrite(_onlineLedPin, !online);
}

void Network::reset()
{
  if (_hasReset == false) {
    return;
  }
  pinMode(_ethernetResetPin, OUTPUT);
  digitalWrite(_ethernetResetPin, 0);
  delay(200);
  digitalWrite(_ethernetResetPin, 1);
  pinMode(_ethernetResetPin, INPUT);
}

//Is the network ready and online
boolean Network::isNetworkReady() {
  int online = digitalRead(_ethernetReadyPin);
  onlineLed(online);
  return online;
}

//Has the network been setup yet
boolean Network::isNetworkSetup() {
  return _networkSetup;
}

// 1 = main door
// 2 = device control
void Network::setMode(int mode) {
  _mode = mode;
}

//If the network is ready configure it
void Network::setupNetwork() {
    //Wait until its idle
    while (digitalRead(_ethernetBusyPin)) {}

    //Clear the buffer
    while (Serial.available()) {
      Serial.read();
    }

    if (_mode == 1) {
      Serial.println("+bbms.buildbrighton.com:/access-control/main-door:1");
    } else if (_mode == 2) {
      Serial.println("+bbms.buildbrighton.com:/access-control/device:1");
    }

    //The network has been setup
    _networkSetup = true;
}

//Is the network adapter busy
boolean Network::isBusy() {
  return digitalRead(_ethernetBusyPin);
}

//Clear the serial buffer - usefull for getting clean reads
void Network::clearBuffer() {
  while (Serial.available()) {
        Serial.read();
    }
}

//Receive an incomming string and return the number of characters received
int Network::receiveString() {
  int index;

  //Clear the buffer
  memset(&receivedString[0], 0, sizeof(receivedString));
  while (Serial.available()) {
        receivedString[index] = Serial.read();
        index++;
        delay(10);
  }
  receivedString[index] = '\0';

  return index;
}

boolean Network::waitForResponse() {
  uint16_t genericTimeCounter = millis();
  while (isBusy()) {
    if (millis() > (genericTimeCounter + 10000)) {
      return false;
    }
  }
  //delay needed for the response to be ready - need to improve
  delay(500);
  return true;
}

//Send a message home reporting that we have booted up
void Network::sendBootMessage() {
  Serial.println(":main-door|boot");
}

void Network::sendHeartbeat() {
  Serial.println(":main-door|heartbeat");
}