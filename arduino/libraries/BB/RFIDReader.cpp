#include <Arduino.h>
#include "RFIDReader.h"

RFIDReader::RFIDReader(HardwareSerial& serial): _RFIDSerial(serial)
{
  //Reset Pin
  //pinMode(_rfidResetPin, OUTPUT);

  //rfidReset(0);
  _RFIDSerial.begin(9600);
}



uint8_t RFIDReader::readTag()
{
    //Variablaes used to track the current character and the overall state
    uint8_t index = 0, reading = 0, tagFetching = 0, tagFetched = 0;
    
    //Was the lookup request successfull
    //uint8_t success;
    
    //Clear the current tag
    memset(&tagString[0], 0, sizeof(tagString));
    
    //Is anything coming from the reader?
    while(_RFIDSerial.available()){
        
        //Data format
        // Start (0x02) | Data (10 bytes) | Chksum (2 bytes) | CR | LF | End (0x03)
        
        char readByte = _RFIDSerial.read(); //read next available byte
        
        if(readByte == 2) reading = 1; //begining of tag
        if(readByte == 3) reading = 0; //end of tag
        
        //If we are reading and get a good character add it to the string
        if(reading && readByte != 2 && readByte != 10 && readByte != 13){
            //store the tag
            tagString[index] = readByte;
            index ++;
            tagFetching = 1;
        }
        //Have we reached the full tag length
        //This prevents this loop from trying to read two codes
        if ((reading == 0) && (index > 1)) {
            break;
        }
    }
    //If we have stoped reading and started in the frist palce
    if (!reading && tagFetching) {
        tagFetched = 1;
        tagFetching = 0;
        tagString[index] = '\0';
        
        //Sometimes a random string is received - filter these out
        if (strcmp("000000000000", tagString) == 0) {
            return 0;
        }
    }

    //The RFID reader can send multiple coppies of the code, clear the buffer of any remaining codes
    while(_RFIDSerial.available()) {
        _RFIDSerial.read();
    }
    
    return tagFetched;
}

void RFIDReader::rfidReset(uint8_t resetDevice) {
    //digitalWrite(_rfidResetPin, !resetDevice);
}

void RFIDReader::togglePower() {
    //rfidReset(1);
    //delay(100);
    //rfidReset(0);
    //delay(200);
}
