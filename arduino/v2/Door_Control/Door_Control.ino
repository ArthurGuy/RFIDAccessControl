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
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <Time.h>
#include <aJSON.h>


#define DEVICE "new-door"
#define TYPE "door"


//External EEPROM
#define EXTERNAL_EEPROM_ADDR 0x50  //eeprom address

//Local Database
#include <EDB.h>
#include <EEPROM.h>

//Network
#include "BBWiFi.h"

#include "AccessControl.h"


// Pin Setup
const int debugLedPin = 9;
const int wifiCsPin = 10;
const int wifiIrqPin = 3;
const int wifiVbenPin = 4;
const int relayPin = 15;
const int miscSwitch = A11;
const int rfidPowerPin = 23;
const int rfidSerialPin = 7;
const int buzzerPin = 5;
const int onlinePin = 6;
const int LEDPin = 2;


//Time tracking
uint8_t genericTimeCounter;
unsigned long heartbeatTimer;
unsigned long isOnlineTimer = 0;
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
  time_t time;
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
//LiquidCrystal_I2C lcd(0x20,  2, 1, 0, 4, 5, 6, 7, 3, NEGATIVE);




//Network connection
BBWiFi WiFi(wifiCsPin, wifiIrqPin, wifiVbenPin, onlinePin);


//General functions for the access control system
AccessControl accessControl(relayPin, buzzerPin, LEDPin, debugLedPin);



//The received tag id
char tagString[13];
int rfidResetPin = 23;



void setup() {
  
    Serial.begin(9600);
    
    delay(2000);
  
    Serial.println("Starting");
  
  
    // ==== Pin Setup ====
    
    Serial.println("Setting up pins");
    pinMode(miscSwitch, INPUT_PULLUP);
    
    
    
    
    // ==== Time Setup ====
    Serial.println("Setting the time");
    setSyncProvider(getTeensy3Time);
    delay(100);
    if (timeStatus()!= timeSet) {
      Serial.println("Unable to sync with the RTC");
    } else {
      Serial.println("RTC has set the system time");
    }
    digitalClockDisplay();
    
    
    
    
    // ==== RFID =====
    Serial3.begin(9600);
    pinMode(rfidResetPin, OUTPUT);
  
  
    
    
    // ==== Screen Setup ====
    Wire.begin();
    Serial.println("Setting up display");
    lcd.begin(8,2);
    lcd.home();
    lcd.print("Booting");
    lcd.setCursor (0, 1);
    lcd.print("Please wait");
    
    
    // ==== Network Setup ====

    Serial.println("Setting up network");
    //WiFi.reset();
    //delay(500);
    WiFi.setMode(1);

    
    

    
    // ==== Database Setup ====
    
    Serial.println("Setting up database");
    //database create method specifies the start address
    // we need two db's so if they span a wide enough gap they shouldnt interfeer
    
    
    resetMemberDB();
    resetLogDB();
    
    //Open dbs starting at location 0
    openDBs();
    
    Serial.println("Local Records");
    Serial.println(countLocalRecords());
    
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
    
    
    
    heartbeatTimer = millis();
    
    forceScreenRefresh = true;
    

}




void loop() {
  
  //Network setup process - ensures its setup if the first attempt fails
  WiFi.checkConfigure();
  
  //setTime(WiFi.getTime());
  //Teensy3Clock.set(WiFi.getTime());
  
  //If the time hasnt been set and the wifi class has it set the time
  if (!accessControl.isTimeSet() && WiFi.hasTime()) {
    Serial.println("Setting the time");
    //Update the rtc and processor clock
    setTime(WiFi.getTime());
    Teensy3Clock.set(WiFi.getTime());
    digitalClockDisplay();
    accessControl.recordTimeSet();
  }
  
  //lcd.clear();
  //lcdTime();
 
  
  
  //Are we online?
  if ((isOnlineTimer + 5000) < millis()) {
      isOnlineTimer = millis();
      accessControl.systemOnline = WiFi.isNetworkReady();
  }
  

  
  //If we have just come online call home
  if (accessControl.systemOnline && !reportedHome) {
      WiFi.sendBootMessage(TYPE, DEVICE);
      reportedHome = true;
      forceScreenRefresh = true;
  }
  
  //Send a heartbeat message every 300 seconds
  if (accessControl.systemOnline) {
      if ((heartbeatTimer + 300000) < millis()) {
          heartbeatTimer = millis();
          WiFi.sendHeartbeat(TYPE, DEVICE);
      }
  }
  
  
  //Dont run to fast - can probably be improved
  delay(200);

  
  //Was the lookup request successfull
  uint8_t success;
  
  //Has someone presented a tag?
  if (rfidReadTag()) {

      //Beep to let the user know the tag was read
      accessControl.ackTone();
      
      lcd.clear();
      
      //Lookup the tag
      lcd.print("Checking Local");
      success = searchLocalDB(tagString);
      if (success) {
          //Save the current tag and time in the logfile for upload
          copyString(logRecord.tag, tagString);
          logRecord.time = now();
          EDB_Status result = logDb.appendRec(EDB_REC logRecord);
              
      } else if (accessControl.systemOnline) {
          lcd.clear();
          lcd.print("Checking Remote");
          success = searchRemoteDB(tagString);
      }
      
      //Tag found
      if (success) {
        
          accessControl.setSystemActive();

          //accessControl.successTone();
          
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
      
      //If its been 4 seconds turn off the node
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
      
      
      
      if (WiFi.hasCmd()) {
        Serial.print("Command received: "); Serial.println(WiFi.cmd);
        
        //action command
        
        if (strcmp(WiFi.cmd, "clear-memory") == 0) {
          Serial.println("Clearing local memory");
          resetMemberDB();
        }
        
        WiFi.clearCmd();
      }
      
      
  }
  
  //Reset the DB if the button is pressed
  //if (analogRead(miscSwitch) == 0) {
  //    resetDBs();
  //}

  
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

void resetLogDB()
{
    //Create a table to store the access log data
    logDb.create(TABLE_SIZE/2, TABLE_SIZE, (unsigned int)sizeof(logRecord));
    delay(500);
}

void resetMemberDB()
{
    //create table at with starting address 0 - this wipes the db
    db.create(0, TABLE_SIZE/2, (unsigned int)sizeof(accessControlData));
    delay(500);
}

void openDBs()
{
    db.open(0);
    logDb.open(TABLE_SIZE/2);
}


boolean searchLocalDB(char tagString[13])
{
    //The local check is very quick, add a delay so the tones arent to close together!
    //delay(500);

    for (unsigned int recno = 1; recno <= db.count(); recno++)
    {
      EDB_Status result = db.readRec(recno, EDB_REC accessControlData);
      if (result == EDB_OK)
      {
        if (strcmp(accessControlData.tag, tagString) == 0)
        {
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
    WiFi.clearBuffer();
    
    if (!WiFi.sendLookup(TYPE, DEVICE, tagString)) { //not making it past this point
        return false;
      
    }
    else
    {
      Serial.println(WiFi.receivedString);
        aJsonObject* jsonObject = aJson.parse(WiFi.receivedString);
        aJsonObject* valid = aJson.getObjectItem(jsonObject, "valid");
        aJsonObject* cmd = aJson.getObjectItem(jsonObject, "cmd");
        
        //record the command for actioning later
        cmd->valuestring;
        
        
        if (valid->valuestring[0] == '1') {
            //get the loggedin members name
            aJsonObject* name = aJson.getObjectItem(jsonObject, "member");
            Serial.println(name->valuestring);
            
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
                        Serial.println("Found existing local record");
                    }
                }
            }
        }
        
        
        //Add it to the local DB
        if (!foundLocal && success) {
          Serial.println("Adding record to the local db");
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
            
            WiFi.sendArchiveLookup(TYPE, DEVICE, logRecord.tag, logRecord.time);
            
            logDb.deleteRec(recno);
            
            delay(200);
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

/*
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
*/

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


  
void lcdTime() {
  lcd.home();
  lcd.print(hour());
  lcd.print(":");
  lcdPrintDigits(minute());
  lcd.print(":");
  lcdPrintDigits(second());
}

void digitalClockDisplay() {
  // digital clock display of the time
  Serial.print(hour());
  serialPrintDigits(minute());
  serialPrintDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}
void serialPrintDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
void lcdPrintDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  if(digits < 10)
    lcd.print('0');
  lcd.print(digits);
}

time_t getTeensy3Time() {
  return Teensy3Clock.get();
}





// RFID
uint8_t rfidReadTag()
{
    //Variablaes used to track the current character and the overall state
    uint8_t index = 0, reading = 0, tagFetching = 0, tagFetched = 0;
    
    //Was the lookup request successfull
    //uint8_t success;
    
    //Clear the current tag
    memset(&tagString[0], 0, sizeof(tagString));
    
    //Is anything coming from the reader?
    while(Serial3.available()){
        
        //Data format
        // Start (0x02) | Data (10 bytes) | Chksum (2 bytes) | CR | LF | End (0x03)
        
        char readByte = Serial3.read(); //read next available byte
        
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
    while(Serial3.available()) {
        Serial3.read();
    }
    
    return tagFetched;
}

void rfidReset(uint8_t resetDevice) {
    digitalWrite(rfidResetPin, !resetDevice);
}

void rfidTogglePower() {
    rfidReset(1);
    delay(100);
    rfidReset(0);
    delay(200);
}

