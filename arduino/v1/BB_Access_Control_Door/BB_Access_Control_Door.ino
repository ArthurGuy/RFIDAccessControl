/*
LCD Display
EEPROM

Door entry system - refactored version
*/
#include <avr/wdt.h>
#include <aJSON.h>
#include "pitches.h"
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>


//External EEPROM
#define EXTERNAL_EEPROM_ADDR 0x50  //eeprom address

// RFID Reader
#include <SoftwareSerial.h>
#include "RFIDReader.h"

//Local Database
#include <EDB.h>
#include <EEPROM.h>

//Network
#include "Network.h"

#include "AccessControl.h"


// Pin Setup
const int debugLedPin = 3;
const int ethernetBusyPin = 5;
const int ethernetReadyPin = 4;
const int ethernetResetPin = 9;
const int doorLockPin = 7;
const int memoryResetPin = A3;
const int rfidPowerPin = 10;
const int rfidSerialPin = 8;
const int buzzerPin = A0;
const int onlinePin = 6;
const int LEDPin = A1;


//Time tracking
uint8_t genericTimeCounter;
long heartbeatTimer;
//long screenLastRefreshed = 0;
boolean forceScreenRefresh;
boolean reportedHome = false;


// Use the external EEPROM as storage
#define TABLE_SIZE 32768 // Arduino 24LC256

//Setup the structure of the stored data
struct AccessControlData {
  char tag[13];
  char name[21];
} 
accessControlData;

struct LogRecord {
  char tag[13];
} 
logRecord;

// The read and write handlers for using the EEPROM Library
void writer(unsigned long address, byte data) {
    writeEEPROM(address, data);     //external
}
byte reader(unsigned long address) {
    return readEEPROM(address);     //external
}
// Create an EDB object with the appropriate write and read handlers
EDB db(&writer, &reader);
EDB logDb(&writer, &reader);



//LCD Screen
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);


//Setup the rfid reader object
RFIDReader rfidReader(rfidSerialPin, rfidPowerPin);


//Network connection
Network Network(ethernetBusyPin, ethernetReadyPin, ethernetResetPin, onlinePin);
//Network Network(ethernetBusyPin, ethernetReadyPin, onlinePin);


//General functions for the access control system
AccessControl accessControl(doorLockPin, buzzerPin, LEDPin, debugLedPin);




void setup() {
  
  
    // ==== Pin Setup ====
    
    pinMode(memoryResetPin, INPUT_PULLUP);
    
    
    // ==== Screen Setup ====

    lcd.begin(8,2);
    //lcd.home();
    lcd.print("Booting");
    lcd.setCursor (0, 1);
    lcd.print("Please wait");
    
    
    // ==== Network Setup ====

    Network.reset();
    delay(500);
    Network.setMode(1);
    Network.startupCheckConfigure();
    
    //pinMode(ethernetResetPin, OUTPUT); //for now
    //digitalWrite(ethernetResetPin, true);

    
    // ==== Database Setup ====
    
    //database create method specifies the start address
    // we need two db's so if they span a wide enough gap they shouldnt interfeer
    
    //resetDBs();
    
    //Open dbs starting at location 0
    openDBs();
    
    //Serial.println("Local Records");
    //Serial.println(countLocalRecords());
    
    /*
    lcd.home();
    lcd.print(db.limit());
    lcd.print(":");
    lcd.print(db.count());
    lcd.setCursor (0, 1);
    
    lcd.print(logDb.limit());
    lcd.print(":");
    lcd.print(logDb.count());
    delay(2000);
    */
    
    
    
    watchdogOn();
    
    heartbeatTimer = millis();
    
    forceScreenRefresh = true;
}




void loop() {
  
  //Reset the watchdog timer
  wdt_reset();
  
  //Network setup process - ensures its setup if the first attempt fails
  Network.checkConfigure();
  
  
  
  //Are we online?
  accessControl.systemOnline = Network.isNetworkReady();
  
  //If we have just come online call home
  if (accessControl.systemOnline && !reportedHome) {
      Network.sendBootMessage();
      reportedHome = true;
      forceScreenRefresh = true;
  }
  
  //Send a heartbeat message every 300 seconds
  if (accessControl.systemOnline) {
      if ((heartbeatTimer + 300000) < millis()) {
          heartbeatTimer = millis();
          Network.sendHeartbeat();
      }
  }
  
  
  //Dont run to fast - can probably be improved
  delay(100);

  
  //Was the lookup request successfull
  uint8_t success;
  
  //Has someone presented a tag?
  if (rfidReader.readTag()) {

      //Beep to let the user know the tag was read
      accessControl.ackTone();
      
      lcd.clear();
      //Lookup the tag
      if (accessControl.systemOnline) {
          lcd.print("Checking Remote");
          success = searchRemoteDB(rfidReader.tagString);
      } else {
          lcd.print("Checking Local");
          success = searchLocalDB(rfidReader.tagString);
          if (success) {
              //Store in the db for transmission later
              
              copyString(logRecord.tag, rfidReader.tagString);
              EDB_Status result = logDb.appendRec(EDB_REC logRecord);
              
          }
      }
      
      //Tag found
      if (success) {
        
          accessControl.setSystemActive();

          accessControl.successTone();
          
      } else {
          //Tag not found
          accessControl.failureTone();
          
          accessControl.deactivateSystem();
          
          lcd.clear();
          lcd.print("No Access");
          
          delay(500);
          
          forceScreenRefresh = true;
          //set the timer so the screen will refresh in 3 seconds
          //screenLastRefreshed = millis() - 57000;
      }

      
  }
  
  //If a user has passed through update the display and keep the door open
  if (accessControl.isSystemActive()) {
    
      //Open the door
      accessControl.activateRelay(1);
      
      
      //Display the user on the screen
        lcd.clear();
        lcd.print("Welcome back");
        lcd.setCursor (0, 1);
        lcd.print(accessControl.activeUserName);
      
      //If its been 45 seconds and no reauthentication turn off the node
      if ((accessControl.getSystemActiveAt() + 4000) < millis()) {
          accessControl.deactivateSystem();
          forceScreenRefresh = true;
      }
  } else {
      
      //Keep the door locked
      accessControl.activateRelay(0);
      
      
      accessControl.standbyLight();
      
      //Refresh the display message every 60 seconds
      //if (((screenLastRefreshed + 60000) < millis()) || forceScreenRefresh) {
      if (forceScreenRefresh) {
          //screenLastRefreshed = millis();
          forceScreenRefresh = false;
        
          displayReadyMessage();
      }
      
      
      
      
      //Do we have anything to upload
      if (accessControl.systemOnline && (countLocalLogRecords() > 0)) {
          //lcd.print(countLocalLogRecords());
          uploadLogFiles();
      }
      
  }

  //Reset the DB if the button is pressed
  if (digitalRead(memoryResetPin) == 0) {
      resetDBs();
  }

  
  //Flash the debug light so we know its still alive
  accessControl.flashDebugLight();
}

void displayReadyMessage()
{
  lcd.begin(8,2);
  lcd.clear();
  lcd.print("Welcome to");
  lcd.setCursor (0, 1);
  lcd.print("Build Brighton");
  
  lcd.setCursor (14, 1);
  if (accessControl.systemOnline) {
    //lcd.print(".");
  } else {
    lcd.print(".");
  }
}

void resetDBs()
{
    //create table at with starting address 0 - this wipes the db
    db.create(0, TABLE_SIZE/2, (unsigned int)sizeof(accessControlData));

    //Create a table to store the access log data
    logDb.create(TABLE_SIZE/2, TABLE_SIZE, (unsigned int)sizeof(logRecord));
    delay(2000);
}

void openDBs()
{
    db.open(0);
    logDb.open(TABLE_SIZE/2);
}


boolean searchLocalDB(char tagString[13])
{
    //The local check is very quick, add a delay so the tones arent to close together!
    delay(500);
    
    //Reset the watchdog timer
    //wdt_reset();
    
    //Reset the user name variable
    //memset(&foundName[0], 0, sizeof(foundName));
    
    for (int recno = 1; recno <= db.count(); recno++)
    {
      EDB_Status result = db.readRec(recno, EDB_REC accessControlData);
      if (result == EDB_OK)
      {
        
        if (strcmp(accessControlData.tag, tagString) == 0) {
          
            //Copy the name for use later
            copyString(accessControl.activeUserName, accessControlData.name);

            return true;
        }
 
      }
    }

    return false;
}



uint8_t searchRemoteDB(char tagString[13])
{
    uint8_t index = 0;
    uint8_t success = 0;
    
    //Reset the user name variable and error variable
    memset(&accessControl.activeUserName[0], 0, sizeof(accessControl.activeUserName));
    
    //Clear the buffer before sending the tag - we want a clean buffer
    Network.clearBuffer();
    
    //Transmit the ID and start the lookup
    Serial.println(tagString);
    
    //Wait while the response is coming in
    if (!Network.waitForResponse()) {
      return false;
    }
    
    //lcd.setCursor(0, 1);
    //lcd.print("Response Available");
    
    index = Network.receiveString();
    
    //lcd.setCursor(0, 1);
    //lcd.print(Network.receivedString);
    
    if (index == 0)
    {
        //No data
        //We can probably assume the network has failed
        // a reset will either fix it or force it into offline mode
        Network.reset();
        
        //Perform a local lookup instead
        return searchLocalDB(tagString);
    }
    else
    {
        aJsonObject* jsonObject = aJson.parse(Network.receivedString);
        aJsonObject* valid = aJson.getObjectItem(jsonObject, "valid");
        
        if (valid->valuestring[0] == '1') {
            aJsonObject* name = aJson.getObjectItem(jsonObject, "msg");
            
            //Copy the users name to a global variable
            copyString(accessControl.activeUserName, name->valuestring);
            //lcd.setCursor (0, 1);
            //lcd.print(name->valuestring);
            
            success = 1;
            
        } else {
            //Keyfob not found
            success = 0;
        }
        
        //Check for a local record and update it
        uint8_t foundLocal = 0;
        if (countLocalRecords() > 0)
        {
            for (int recno = 1; recno <= countLocalRecords(); recno++)
            {
                //Serial.print("Recno: "); 
                //Serial.println(recno);
                EDB_Status result = db.readRec(recno, EDB_REC accessControlData);
                if (result == EDB_OK)
                {
                    //Serial.print("Key: "); 
                    //Serial.println(accessControlData.tag);
                    if (strcmp(accessControlData.tag, tagString) == 0)
                    {
                        //Local record found
                        if (!success) {
                            //The local record needs to be removed
                            db.deleteRec(recno);
                        }
                        foundLocal = 1;
                        //Serial.println("Found Local");
                    }
                }
            }
        }
        
        
        //Add it to the local DB
        if (!foundLocal && success) {
            copyString(accessControlData.tag, tagString);
            copyString(accessControlData.name, accessControl.activeUserName);
            EDB_Status result = db.appendRec(EDB_REC accessControlData);
        }
        
        aJson.deleteItem(jsonObject);
    }
    
    return success;
}



void uploadLogFiles() {
  
    lcd.clear();
    lcd.print("Please Wait");
  
    for (int recno = 1; recno <= countLocalLogRecords(); recno++)
    {
        //Serial.print("Recno: "); 
        //Serial.println(recno);
        EDB_Status result = logDb.readRec(recno, EDB_REC logRecord);
        if (result == EDB_OK)
        {
            lcd.setCursor (0, 1);
            lcd.print(logRecord.tag);
            
            //Send the ID so it gets added to the log files
            Serial.print(logRecord.tag);
            Serial.println("|delayed");
            if (!Network.waitForResponse()) {
                return;
            }
            
            //if (Network.receiveString() == 0) {
                //Probably not received by the server
                //may not be worth checking this
            //    return;
            //}
            
            logDb.deleteRec(recno);
            
            delay(500);
        }
    }
    
    forceScreenRefresh = true;
}


void copyString(char *p1, char *p2)
{
    while(*p2 !='\0')
    {
       *p1 = *p2;
       p1++;
       p2++;
     }
    *p1= '\0';
}





// utility functions

uint8_t countLocalRecords()
{
    return db.count();
}

uint8_t countLocalLogRecords()
{
    return logDb.count();
}


void watchdogOn() {
  
  // Clear the reset flag, the WDRF bit (bit 3) of MCUSR.
  MCUSR = MCUSR & B11110111;
    
  // Set the WDCE bit (bit 4) and the WDE bit (bit 3) 
  // of WDTCSR. The WDCE bit must be set in order to 
  // change WDE or the watchdog prescalers. Setting the 
  // WDCE bit will allow updtaes to the prescalers and 
  // WDE for 4 clock cycles then it will be reset by 
  // hardware.
  WDTCSR = WDTCSR | B00011000; 
  
  // Set the watchdog timeout prescaler value to 1024 K 
  // which will yeild a time-out interval of about 8.0 s.
  WDTCSR = B00100001;
  
  // Enable the watchdog timer interupt.
  WDTCSR = WDTCSR | B01000000;
  MCUSR = MCUSR & B11110111;

}


//External EEPROM
void writeEEPROM(unsigned int eeaddress, byte data ) 
{
  Wire.beginTransmission(EXTERNAL_EEPROM_ADDR);
  Wire.write((int)(eeaddress >> 8));   // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.write(data);
  Wire.endTransmission();
 
  delay(5);
}
 
byte readEEPROM(unsigned int eeaddress ) 
{
  byte rdata = 0xFF;
 
  Wire.beginTransmission(EXTERNAL_EEPROM_ADDR);
  Wire.write((int)(eeaddress >> 8));   // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.endTransmission();
 
  Wire.requestFrom(EXTERNAL_EEPROM_ADDR,1);
 
  if (Wire.available()) rdata = Wire.read();
 
  return rdata;
}
