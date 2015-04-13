#ifndef BBWiFi_h
#define BBWiFi_h

#include <Arduino.h>
#include <Adafruit_CC3000.h>
#include <aJSON.h>

class BBWiFi
{
  public:
    BBWiFi(int csPin, int irqPin, int vbenPin);
    BBWiFi(int csPin, int irqPin, int vbenPin, int onlineLedPin);
    boolean isNetworkReady();
    void checkConfigure();
    void reset();
    void setMode(int mode);
    void sendBootMessage(char *type, char *device);
    void sendHeartbeat(char *type, char *device);
    bool sendData(char *stringToSend, char httpResponse[]);
    bool sendLookup(char *type, char *device, char *keyFob);
    bool sendArchiveLookup(char *type, char *device, char *keyFob, time_t time);
    void decodeResponse(char *stringToSend, char *message);
    
    time_t getTime();
    bool hasTime();

    void clearBuffer();
    boolean isBusy();

    char receivedString[150];
    int receiveString();
    boolean waitForResponse();

    char cmd[50];
    bool hasCmd();
    void clearCmd();
    
  private:
  
    void onlineLed(int online);
    bool isNetworkSetup();
    void setupNetwork();
    bool displayConnectionDetails();
    void fetchResponse(char httpResponse[]);
    bool lookupIp();
    
    int _csPin;
    int _irqPin;
    int _vbenPin;
    int _onlineLedPin;
    int _networkSetup;
    int _hasReset;
    int _mode;
    
    
    bool _wifi_initialized = false;
    bool _wifi_connected = false;
    bool _dhcp_lookup_complete = false;
    
    time_t _time;
    bool _has_time = false;

    bool _has_cmd = false;
    
    char _wifi_ssid[33];
    char _wifi_pass[64];
    int _wifi_mode;
    
    uint32_t _target_ip = 0;
    char _website[50];
    char _url[50];
    
    const unsigned long _dhcpTimeout = 60L * 1000L;
    const unsigned long connectTimeout = 15000;
          
    Adafruit_CC3000 _cc3000;
    Adafruit_CC3000_Client _client;
};

#endif
