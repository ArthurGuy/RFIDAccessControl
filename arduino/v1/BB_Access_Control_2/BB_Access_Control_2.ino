/*
LCD Display
EEPROM

Door entry system
*/
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <aJSON.h>
#include <EDB.h>


// Pin Setup
int ethernetBusy = 2;
int ethernetReady = 3;
int doorLockPin = 13;
int memoryResetPin = 12;
int rfidPowerPin = 8;

//Data variables
uint8_t mode = 1;  //mode 1 = door entry; 2 = equipment node
char foundName[30];
uint8_t systemActive = 0;
long systemActiveAt;
uint8_t reCheck = 0;
char activeTagString[13];


//System Status
int networkSetup = 0;


// Use the Internal Arduino EEPROM as storage
#include <EEPROM.h>
#define TABLE_SIZE 1024 // Arduino 328

//Setup the structure of the stored data
struct AccessControl {
  char tag[13];
  char name[20];
} 
accessControl;

// The read and write handlers for using the EEPROM Library
void writer(unsigned long address, byte data) {
    EEPROM.write(address, data);
}
byte reader(unsigned long address) {
    return EEPROM.read(address);
}
// Create an EDB object with the appropriate write and read handlers
EDB db(&writer, &reader);



// Ethernet serial out
#include <SoftwareSerial.h>



// LED Display
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); 



//RFID Reader
SoftwareSerial RFID(10, 11); // RX, TX



void setup() {
    
    // Initialise a 20x4 LCD
    lcd.begin(20, 4);
   
    // Turn on the backlight
    lcd.setBacklight(true);
    
    // Write to the LCD
    lcd.home();
    lcd.print("BB Access Control");
    lcd.setCursor (0, 1);
    lcd.print("Starting...");
  
    pinMode(doorLockPin, OUTPUT);
    pinMode(ethernetBusy, INPUT);
    pinMode(ethernetReady, INPUT);
    pinMode(memoryResetPin, INPUT_PULLUP);
    pinMode(rfidPowerPin, OUTPUT);
    
    
    Serial.begin(9600);
    
    
    //lcd.setCursor (0, 1);
    //lcd.print(F("Waiting for network"));
    //while (!digitalRead(ethernetReady)) {}
    if (startupCheckNetworkReady()) {
        
        lcd.setCursor (0, 1);
        lcd.print(F("Configuring network"));
        
        setupNetwork();
        
        
        
    } else {
      
        //Network not available - continue for now but keep checking
        lcd.setCursor (0, 1);
        lcd.print(F("Netwok unavailable "));
        delay(2000);
        
    }
    
    

    lcd.setCursor (0, 1);
    //lcd.print(F("Network ready"));
    
    //lcd.clear();
    //lcd.home();
    //lcd.print("BB Access Control");
    
    lcd.setCursor (0, 1);
    lcd.print(F("Setting Up the DB  "));
    
    //create table at with starting address 0
    //db.create(0, TABLE_SIZE, (unsigned int)sizeof(accessControl));
    
    //Open a db starting at location 0
    db.open(0);
    
    lcd.setCursor (0, 2);
    lcd.print(F("Local Records: "));
    lcd.print(countRecords(), DEC);
    
    
    rfidPower(1);
    RFID.begin(9600);
    
    delay(2000);
    
    resetDisplay();
}




void loop() {
  
  //If the network is available but hasn't been setup do it
  if (!isNetworkSetup() && isNetworkReady()) {
      setupNetwork();
  }
  
  //resetDisplay();
  
  //Keep watching the network for problems
  //checkNetworkReady();
  lcd.setCursor (0, 3);
  if (!isNetworkReady()) {
      lcd.print("Offline");
  } else {
      lcd.print("Online ");
  }
  
  
  
  //lcd.print('.');
  delay(500);

  char tagString[13];
  int index = 0;
  boolean reading = false;
  boolean tagFetching = false;
  boolean tagFetched = false;
  
  //Was the lookup request successfull
  uint8_t success;
  
  //Do we need to make a all to the server - used for logging when a local lookup works
  boolean callServer = false;
  
  //Clear the string holding the tag id
  memset(&tagString[0], 0, sizeof(tagString));
  
  
  

  while(RFID.available()){

    int readByte = RFID.read(); //read next available byte

    if(readByte == 2) reading = true; //begining of tag
    if(readByte == 3) reading = false; //end of tag
    
    if(reading && readByte != 2 && readByte != 10 && readByte != 13){
      //store the tag
      tagString[index] = readByte;
      index ++;
      tagFetching = true;
    }
    //Have we reached the full tag length
    if ((reading == false) && (index > 1)) {
        break;
    }
  }
  
  if (!reading && tagFetching) {
      tagFetched = true;
      tagFetching = false;
      tagString[index] = '\0';
  }
  
  if (tagFetched) {
      clearDisplayTitle();
      lcd.setCursor (0, 1);
      lcd.print(tagString);
      
      
      lcd.setCursor (0, 2);
      lcd.print("Searching...");
      //delay(250);
      
      //Start the search
      if ((reCheck == 1) && (mode == 2)) {
          //We only need to confirm the id is still the same and there
          if (strcmp(activeTagString, tagString) == 0) {
              //Everything is OK
              systemActive = true;
              systemActiveAt = millis();
          } else {
              //We have a different user - reset and start again
              startRemoteDBLookup("END");
              systemActive = false;
              memset(&activeTagString[0], 0, sizeof(activeTagString));
              
              //Restart the reader so we can tage a new read
              rfidPower(false);
              delay(100);
              rfidPower(true);
          }
          success = searchLocalDB(tagString);
      } else {
          if (isNetworkReady()) {
              success = searchRemoteDB(tagString);
          } else {
              success = searchLocalDB(tagString);
          }
      }
      
      if (success) {
          lcd.setCursor(0, 2);
          lcd.print(foundName);
      } else {
          lcd.setCursor(0, 2);
          lcd.print("Not found");
      }
      
      if (mode == 1) {
          //Door Entry
      
          //Activate the door lock
          if (success) {
              doorLock(1);
          }
          
          delay(4000);
          
          if (success) {
              doorLock(0);
          }
      } else if (mode == 2) {
          //AC Node
        
          if (success) {
              systemActive = true;
              systemActiveAt = millis();
              
              //Store the currently active tag for later
              copyString(activeTagString, tagString);
          }
        
      }
      
      //Do we need to report this?
      if (callServer) {
          callServer = false;
          
          //Make a lookup call - we don't care about the response - just used for logging
          //startRemoteDBLookup(tagString);
      }
      
      
      //The RFID reader can send multiple coppies of the code, clear the buffer of any remaining codes
      while(RFID.available()) {
          RFID.read();
      }
      
      resetDisplay();
  }
  
  //If we are running in device mode keep an eye on the time and reset the rfid reader when needed
  if ((mode == 2) && (systemActive == 1)) {
    
      //If its been 30 seconds reset the rfid reader and hopefully reauthenticate
      if ((systemActiveAt + 30000) < millis()) {
          //if the active flag has been set for 30 seconds reset it and start looking for the next code to keep it active
          rfidPower(false);
          delay(100);
          rfidPower(true);
          
          //We are only interested in rechecking rather than a full auth process
          reCheck = true;
      }
      
      //If its been 45 seconds and no reauthentication turn off the node
      if ((systemActiveAt + 45000) < millis()) {
          systemActive = 0;
      }
      
  }

  if (digitalRead(memoryResetPin) == 0) {
      db.create(0, TABLE_SIZE, (unsigned int)sizeof(accessControl));
      lcd.setCursor (0, 1);
      lcd.print("Clearing DB      ");
      delay(2000);
      resetDisplay();
  }
}


boolean searchLocalDB(char tagString[13])
{
    //Reset the user name variable
    memset(&foundName[0], 0, sizeof(foundName));
    
    for (int recno = 1; recno <= db.count(); recno++)
    {
      EDB_Status result = db.readRec(recno, EDB_REC accessControl);
      if (result == EDB_OK)
      {
        
        if (strcmp(accessControl.tag, tagString) == 0) {
          
            //Copy the name for use later
            copyString(foundName, accessControl.name);
            
            //lcd.setCursor(0, 2);
            //lcd.print(accessControl.name);

            return true;
        } else {
          //lcd.setCursor (0, 3);
          //lcd.print(accessControl.tag);
          //delay(500);
        }
 
      }
      else 
      {
          printError(result);
      }
    }
    //lcd.setCursor(0, 2);
    //lcd.print("Key fob not found");
    return false;
}

void startRemoteDBLookup(char tagString[13])
{
    //Send the tag to start the search
    Serial.println(tagString);
}

boolean searchRemoteDB(char tagString[13])
{
    int index = 0;
    char receivedString[100];
    uint8_t success = 0;
    long checkTime;
    
    //Reset the user name variable
    memset(&foundName[0], 0, sizeof(foundName));
    
    //Clear the buffer before sending the tag - we want a clean buffer
    while (Serial.available()) {
        Serial.read();
    }
    
    //Transmit the ID and start the lookup
    startRemoteDBLookup(tagString);
    
    //Wait while the response is coming in
    checkTime = millis();
    while (digitalRead(ethernetBusy)) {
        if (millis() > (checkTime + 10000)) {
            return false;
        }
    }
    
    
    delay(500); //delay needed for the response to be ready - need to improve
    
    index = 0;
    memset(&receivedString[0], 0, sizeof(receivedString));
    while (Serial.available()) {
        receivedString[index] = Serial.read();
        index++;
        delay(10);
    }
    receivedString[index] = '\0';
    
    
    if (index == 0)
    {
        lcd.setCursor(0, 2);
        lcd.print("Error - no data");
    }
    //else if ((receivedString[0] == '4') && (receivedString[1] == '0') && (receivedString[2] == '4'))
    //{
        //lcd.setCursor(0, 2);
        //lcd.print("Error");
    //}
    else
    {
    
        aJsonObject* jsonObject = aJson.parse(receivedString);
        aJsonObject* valid = aJson.getObjectItem(jsonObject, "valid");
        
        if (valid->valuestring[0] == '1') {
            aJsonObject* name = aJson.getObjectItem(jsonObject, "name");
            
            //Copy the users name to a global variable
            copyString(foundName, name->valuestring);
            
            success = 1;
            
            
        } else {
          
            //lcd.setCursor(0, 2);
            //lcd.print("Key fob not found");
            
            success = 0;
        }
        
        
        //Check for a local record and update it
        boolean foundLocal = 0;
        for (int recno = 1; recno <= db.count(); recno++) {
            EDB_Status result = db.readRec(recno, EDB_REC accessControl);
            if (result == EDB_OK) {
                if (strcmp(accessControl.tag, tagString) == 0) {
                    //Local record found
                    if (!success) {
                        //The local record needs to be removed
                        db.deleteRec(recno);
                    }
                    foundLocal = 1;
                }
            }
        }
        //Add it to the local DB
        if (!foundLocal && success) {
            copyString(accessControl.tag, tagString);
            copyString(accessControl.name, foundName);
            EDB_Status result = db.appendRec(EDB_REC accessControl);
        }
        
        aJson.deleteItem(jsonObject);
    }
    
    return success;
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

/**
  Wait untill the network is ready before continuing
  Includes a 10 second timeout incase there is a problem
*/
boolean startupCheckNetworkReady() {
    boolean systemReset = false;
    long checkTime = millis();
    while (!digitalRead(ethernetReady)) {
        lcd.setCursor (0, 1);
        lcd.print(F("Waiting for network"));
        delay(50);
        systemReset = true;
        if (millis() > (checkTime + 10000)) {
            return false;
        }
    }
    return true;
}

// Is the network adapter ready
boolean isNetworkReady() {
    return digitalRead(ethernetReady);
}

void checkNetworkReady() {
    boolean systemReset = false;
    long checkTime = millis();
    while (!digitalRead(ethernetReady)) {
        lcd.setCursor (0, 1);
        lcd.print(F("Waiting for network"));
        delay(50);
        systemReset = true;
    }
    if (systemReset) {
        resetDisplay();
    }
}

// Initial setup to be run once
void setupNetwork() {
    //Wait untill its idle
    while (digitalRead(ethernetBusy)) {}
   
    //Clear the buffer
    while (Serial.available()) {
      Serial.read();
    }
    
    Serial.println("+bbms.buildbrighton.com:/access-control/status:1");
    
    //delay(2000);
    
    //while (Serial.available()) {
    //    Serial.read();
    //}
    
    //The network has been setup
    networkSetup = true;
}

boolean isNetworkSetup() {
    return networkSetup;
}


void resetDisplay() {
    clearDisplayTitle();
    lcd.setCursor (0, 1);
    lcd.print("Scan your key fob");
}


void clearDisplayTitle() {
    lcd.clear();
    lcd.home();
    lcd.print("BB Access Control");
}



void doorLock(uint8_t onOff) {
    digitalWrite(doorLockPin, onOff);
}

void rfidPower(uint8_t onOff) {
    digitalWrite(rfidPowerPin, onOff);
}



// utility functions

void recordLimit()
{
  Serial.print("Record Limit: ");
  Serial.println(db.limit());
}

void deleteOneRecord(int recno)
{
  Serial.print("Deleting recno: ");
  Serial.println(recno);
  db.deleteRec(recno);
}

void deleteAll()
{
  Serial.print("Truncating table...");
  db.clear();
  Serial.println("DONE");
}

uint8_t countRecords()
{
  return db.count();
}

void createRecords(int num_recs)
{
  Serial.print("Creating Records...");
  for (int recno = 1; recno <= num_recs; recno++)
  {
    accessControl.tag[0] = 'A';
    accessControl.name[0] = 'J';
    EDB_Status result = db.appendRec(EDB_REC accessControl);
    if (result != EDB_OK) printError(result);
  }
  Serial.println("DONE");
}

void dumpAllDBRecords()
{  
  for (int recno = 1; recno <= db.count(); recno++)
  {
    EDB_Status result = db.readRec(recno, EDB_REC accessControl);
    if (result == EDB_OK)
    {
      Serial.print("Recno: "); 
      Serial.print(recno);
      Serial.print(" Tag: "); 
      Serial.print(accessControl.tag);
      Serial.print(" Name: "); 
      Serial.println(accessControl.name);   
    }
    else printError(result);
  }
}


void printError(EDB_Status err)
{
  Serial.print("ERROR: ");
  switch (err)
  {
    case EDB_OUT_OF_RANGE:
      Serial.println("Recno out of range");
      break;
    case EDB_TABLE_FULL:
      Serial.println("Table full");
      break;
    case EDB_OK:
    default:
      Serial.println("OK");
      break;
  }
}
