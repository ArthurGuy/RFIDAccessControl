#ifndef RFIDReader_h
#define RFIDReader_h

#include <Arduino.h>


class RFIDReader
{
  public:
    RFIDReader(HardwareSerial& serial);
    void dot();
    void dash();
    uint8_t readTag();
    void togglePower();
    
    //The received characters form the rfid reader
    char tagString[13];
    
  private:
    int _rfidSerialPin;
    int _rfidResetPin;
    HardwareSerial& _RFIDSerial;
    void rfidReset(uint8_t onOff);
};

#endif
