/*
LCD Display
NO EEPROM
JSON Response
No Output control
*/
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <aJSON.h>

//Ethernet serial out
#include <SoftwareSerial.h>

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); 

SoftwareSerial RFID(10, 11); // RX, TX



int ethernetBusy = 2;
int ethernetReady = 3;





void setup(){
    
    
    // Initialise a 20x4 LCD
    lcd.begin(20, 4);
   
    // Turn on the backlight
    lcd.setBacklight(true);
    
    // Write to the LCD
    lcd.home();
    lcd.print("BB Access Control");
    lcd.setCursor (0, 1);
    lcd.print("Starting...");
  
    
    pinMode(ethernetBusy, INPUT);
    pinMode(ethernetReady, INPUT);
    
    pinMode(13, OUTPUT);
    
    
    Serial.begin(9600);
    
    
    //lcd.setCursor (0, 1);
    //lcd.print(F("Waiting for network"));
    //while (!digitalRead(ethernetReady)) {}
    checkNetworkReady();
    lcd.setCursor (0, 1);
    lcd.print(F("Configuring network"));
  
    //Wait untill its idle
    while (digitalRead(ethernetBusy)) {}
    
   
    //Clear the buffer
    while (Serial.available()) {
      Serial.read();
    }
    
    Serial.println("+bbms.buildbrighton.com:/access-control/status:1");
    
    delay(3000);
    
    while (Serial.available()) {
      Serial.read();
    }
    
    

    lcd.setCursor (0, 1);
    lcd.print(F("Network ready"));
    
    //lcd.clear();
    //lcd.home();
    //lcd.print("BB Access Control");
    /*
    */
    
    
    
    RFID.begin(9600);
    
    resetDisplay();
}

void loop(){
  
  checkNetworkReady();

  char tagString[13];
  char receivedString[100];
  int index = 0;
  boolean reading = false;
  boolean tagFetching = false;
  boolean tagFetched = false;
  
  memset(&tagString[0], 0, sizeof(tagString));
  
  lightLED(13);
  

  //RFID.listen();
  //while (RFID.available()) {
    //Serial.write(RFID.read());
  //}

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
    
    //Clear the buffer before sending the tag - we want a clean buffer
    while (Serial.available()) {
        Serial.read();
    }
    

    Serial.println(tagString);
    lcd.setCursor (0, 2);
    lcd.print("Sending...");
    //delay(250);
    
    //Wait while the response is coming in
    while (digitalRead(ethernetBusy)) {}
    //delay(50);
    
    /*
    //this doesnt work as it moves on when only a few characters are available
    uint8_t wait = 0;
    while (Serial.available() == 0) {
        delay(50);
        wait++;
        if (wait > 50) {
           break; 
        }
    }
    
    */
    
    
    delay(500); //delay needed for the response to be ready - improve
    
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
    else if ((receivedString[0] == '4') && (receivedString[1] == '0') && (receivedString[2] == '4'))
    {
        
        lcd.setCursor(0, 2);
        lcd.print("Error");
        
    }
    else
    {
    
        aJsonObject* jsonObject = aJson.parse(receivedString);
        aJsonObject* valid = aJson.getObjectItem(jsonObject, "valid");
        
        if (valid->valuestring[0] == '1') {
            aJsonObject* name = aJson.getObjectItem(jsonObject, "name");
            aJsonObject* status = aJson.getObjectItem(jsonObject, "status");
            
            lcd.setCursor(0, 2);
            lcd.print(name->valuestring);
            lcd.setCursor(0, 3);
            lcd.print(status->valuestring);
        } else {
          
            lcd.setCursor(0, 2);
            lcd.print("Key fob not found");
          
        }
        
        aJson.deleteItem(jsonObject);
        
    }
    
    delay(4000);
    
    resetDisplay();
  }

  
  //resetReader(); //eset the RFID reader
}


void checkNetworkReady() {
    boolean systemReset = false;
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



void lightLED(int pin){
///////////////////////////////////
//Turn on LED on pin "pin" for 250ms
///////////////////////////////////

  delay(250);
  digitalWrite(pin, HIGH);
  delay(250);
  digitalWrite(pin, LOW);
}

