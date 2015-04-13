#ifndef BBDatabase_h
#define BBDatabase_h

#include <Arduino.h>


class Network
{
  public:
    Network(int ethernetBusyPin, int ethernetReadyPin, int onlineLedPin);
    Network(int ethernetBusyPin, int ethernetReadyPin, int ethernetResetPin, int onlineLedPin);
    boolean isNetworkReady();
    void startupCheckConfigure();
    void checkConfigure();
    void reset();
    void setMode(int mode);
    void sendBootMessage();
    void sendHeartbeat();

    void clearBuffer();
    boolean isBusy();

    char receivedString[50];
    int receiveString();
    boolean waitForResponse();
    
  private:
  
    void onlineLed(int online);
    boolean isNetworkSetup();
    void setupNetwork();
    
    int _ethernetBusyPin;
    int _ethernetReadyPin;
    int _ethernetResetPin;
    int _onlineLedPin;
    int _networkSetup;
    int _hasReset;
    int _mode;
    
};

#endif
