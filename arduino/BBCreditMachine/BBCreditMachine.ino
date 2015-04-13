#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <sha1.h>
#include <EEPROM.h>

//Chinese adapter
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

//Pins
int counterPin = 2;
int coinPin = 3;
int coinInteruptPin = 0;
int switchPin = 8;

//uint8_t coinPulseCounter = 0;
//uint8_t readingCoin = 0;
//long genericTimeCounter;
//long genericTimeCounter2;

uint16_t coinCount = 0;
uint8_t coinRead = 0;
long lastCoinRead;

void setup()
{
    
    pinMode(coinPin, INPUT);
    pinMode(counterPin, INPUT);
    pinMode(13, INPUT);
    pinMode(switchPin, INPUT_PULLUP);
  
    lcd.begin(8,2);               // initialize the lcd 
    lcd.setBacklight(true);

    lcd.home();                   // go home
    lcd.print("BB Credit");  
    lcd.setCursor(0, 1);        // go to the next line
    lcd.print ("Enter some money");
    delay (1000);
  
    lcd.clear();
    lcd.home();
    
    Sha1.init();
    
    attachInterrupt(coinInteruptPin, coinInserted, RISING);
}

void coinInserted()    
//The function that is called every time it recieves a pulse
{
  coinCount = coinCount + 5;
  coinRead = 1;
  lastCoinRead = millis();
}

void loop()
{
  
  if (coinRead) {
    
    lcd.clear();
    lcd.home();
    lcd.print(coinCount);
    lcd.print("p");
    coinRead = 0;
    
  }
  
  if (!digitalRead(switchPin)) {
    
    char buffer[4];
    itoa(coinCount,buffer,10);
    buffer[3] = '\0';
    
    lcd.setCursor(0, 1);
    lcd.println(buffer);
    generateHash(buffer);
    
    coinCount = 0;
    
    while(!digitalRead(switchPin)) {}
  }
  /*
  if (digitalRead(coinPin)) {
    
    //lcd.home();
    //lcd.clear();
    
    readingCoin = true;
    coinPulseCounter = 0;
    
    while (readingCoin) {
    
  
        while (digitalRead(coinPin)) {
          //waiting - 155ms positive pulse
        }
        coinPulseCounter++;
        
        genericTimeCounter = millis();
        
        while (!digitalRead(coinPin)) {
          //waiting - 155ms negative pulse
          if ((millis() - genericTimeCounter) > 155) {
            //end of coin
            readingCoin = false;
            break;
          }
        }
        //genericTimeCounter2 = millis() - genericTimeCounter;
        //lcd.print(genericTimeCounter2);
        
    
    }
    
    //genericTimeCounter2 = genericTimeCounter - millis();
    
    lcd.print(pulseToPence(coinPulseCounter));
    lcd.print("p ");
    
    
    //delay(1000);
    //lcd.clear();
  }
  */
  /*
  lcd.home();
  //lcd.clear();
  if (digitalRead(coinPin)) {
    lcd.print("Coin Detect");
  } else {
    lcd.print("No Coin");
  }
  
  lcd.setCursor(0, 1);
  if (digitalRead(counterPin)) {
    lcd.print("Counter");
  } else {
    lcd.print("No Counter");
  }
  */
}


void generateHash(char *stringToSend) {
  
  uint8_t		*in, out, i;
      	char		b64[29];
      	static const char PROGMEM b64chars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
      //char signingKey2[]    = "PUT_YOUR_CONSUMER_SECRET_HERE";
      char signingKey[]    = "73JDNCG40AMD";
      
	//Fetch the next id and turn it into a fixed length string
	uint16_t messageId = getNextMessageID();
	char messageIdBuffer[6];
	sprintf(messageIdBuffer, "%d", messageId);


	//_serial->print("Signing Key|");
	//_serial->print(signingKey);
	//_serial->println("|");
	
	uint8_t *hash;
	Sha1.initHmac((uint8_t *)signingKey, 12); // key, and length of key in bytes
	Sha1.print(stringToSend);
	Sha1.print(messageIdBuffer);

      // base64-encode SHA-1 hash output.  This is NOT a general-purpose base64
	// encoder!  It's stripped down for the fixed-length hash -- always 20
	// bytes input, always 27 chars output + '='.
	for(in = Sha1.resultHmac(), out=0; ; in += 3) { // octets to sextets
	    b64[out++] =   in[0] >> 2;
	    b64[out++] = ((in[0] & 0x03) << 4) | (in[1] >> 4);
	    if(out >= 26) break;
	    b64[out++] = ((in[1] & 0x0f) << 2) | (in[2] >> 6);
	    b64[out++] =   in[2] & 0x3f;
	}
	b64[out] = (in[1] & 0x0f) << 2;
	// Remap sextets to base64 ASCII chars
	for(i=0; i<=out; i++) b64[i] = pgm_read_byte(&b64chars[b64[i]]);
	b64[i++] = '=';
	b64[i++] = 0;
    
    
    lcd.println(b64);


}


uint16_t getNextMessageID()
{
  uint16_t id;
  //If the id hasnt been set before, set it
  if (EEPROM.read(200) != 1) {
    EEPROM.write(200, 1);
    EEPROM.write(201, 0);
    EEPROM.write(202, 0);
  }

  //Fetch the ID and increment it
  id = (EEPROM.read(202) * 256) + EEPROM.read(201);
  id++;

  //Save the updated value
  EEPROM.write(201, lowByte(id));
  EEPROM.write(202, highByte(id));
  
  return id;
}


uint8_t pulseToPence(uint8_t pulse) {
  if (pulse == 2) {
    return 5;
  } else if (pulse == 3) {
    return 10;
  } else if (pulse == 4) {
    return 20;
  } else if (pulse == 5) {
    return 50;
  } else if (pulse == 6) {
    return 100;
  } else if (pulse == 7) {
    return 200;
  }
  return 0;
}
