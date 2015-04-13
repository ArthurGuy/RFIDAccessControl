#ifndef AccessControl_h
#define AccessControl_h

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>


class AccessControl
{
  public:
    AccessControl(int relayPin, int buzzerPin, int ledPin, int debugLedPin);
    void activateRelay(int onOff);
    boolean isSystemActive();
    unsigned long getSystemActiveAt();
    void setSystemActive();
    void deactivateSystem();

    void debugLED(int onOff);
    void flashDebugLight();

    //Clear the stored tag
    void clearActiveTag();

    void failureTone();
    void ackTone();
    void successTone();

    void successLight();
    void standbyLight();
    void failureLight();
    void ackLight();

    //The name of the currently active user
    char activeUserName[21];

    //The tag of the currently active user
    char activeTagString[13];

    //Is the system online
    boolean systemOnline;
    
    bool isTimeSet();
    void recordTimeSet();

  private:

    int _systemActive;
    int _accessFailed;
    long _systemActiveAt;
    int _relayPin;
    int _buzzerPin;
    int _ledPin;
    int _debugLedPin;
    Adafruit_NeoPixel ledStrip;
    bool _time_set = false;

};

#endif
