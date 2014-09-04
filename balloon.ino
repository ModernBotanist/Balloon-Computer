#include <SD.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <Adafruit_GPS.h>
#include <EEPROM.h>
#include "Hell.h"

/** SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 10  */
 
//pin outs
byte gpsTX = 3; //serial pins for gps
byte gpsRX = 2;
byte radTX = 5; //serial pins for radio module
byte radRX = 4;
byte radSleep = 7; //"PD" low to sleep
byte radioPin = 9; //actual tone encodeing (Must be PWM)
byte PTT = 8; //push-to-talk pin, low to TX
byte ErrorLED = 6;

//other Vars or definitions
#define call "KC9UNJ"; //radio callsign
#define fileType ".TXT"; //filetype to append
char GPSFile[8]; //this holds the file name
char Buffer[30]; //for reading responses off radio
byte radTime = 1; //minutes between radio transmissions
byte i; //an index for a few things
unsigned long timer; //our millis counter

char *handshake = "AT+DMOCONNECT";
char *setting = "AT+DMOSETGROUP=0,145.5500,145.5500,0,8,0,1";
  //0 = narrow FM
  //145.5500 = TX freq
  //145.5500 = RX freq
  //0 = RXCTCSS (none)
  //8 = Squelch (highest)
  //0 = TXCTCSS (none)
  //Flag??

//initialization
SoftwareSerial mySerial(gpsTX, gpsRX); //for GPS
SoftwareSerial radio(radTX, radRX); //radio serial
File myFile;
Adafruit_GPS GPS(&mySerial);


/* set to true to only log to SD when GPS has a fix, for debugging, keep it false */
#define LOG_FIXONLY false  


void setup() {  
  
  Serial.begin(115200); //for debug
  
/*********PINOUTS***********************************/
  
  pinMode(radioPin, OUTPUT);
  pinMode(radSleep, OUTPUT);
  pinMode(PTT, OUTPUT);
  pinMode(ErrorLED, OUTPUT); //"an error has occured" LED
  digitalWrite(radSleep, HIGH); //low is radio off
  digitalWrite(PTT, HIGH); //set radio to rx
  
/***********SD Card Setup***************************************/
  
  const byte addr = 0; //EEPROM location for file number on start
  byte counter; //EEPROM stored number
  
  //increment file counter
  counter = EEPROM.read(addr);
  Serial.print(F("counter = "));
  Serial.println(counter);
  counter += 1; //increment
  EEPROM.write(addr, counter); //rewrite for next startup
  Serial.print(F("Counter now = "));
  Serial.println(counter);
  
  //Construct filename for this run
  String newFile = String(counter) + fileType;
  newFile.toCharArray(GPSFile,8);
  Serial.print(F("GPSFile name = "));
  Serial.println(GPSFile);
  
  if (!SD.begin(10)) {
    Serial.println(F("initialization failed!"));
    digitalWrite(ErrorLED, HIGH); //alert LED if SD card fails
  }
  Serial.println(F("initialization done."));
  
  /*****************GPS Set-up*************************************/
  
  GPS.begin(9600);
  delay(2000);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA); //minimal + lock and altitude
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); //gps dump at 1hz
  GPS.sendCommand(PGCMD_NOANTENNA);
  
  /***************Radio Module Set-up********************************/
  
  radio.begin(9600);
  delay(100);
  radio.println(handshake); //send handshake
  
  Serial.print(F("trying to connect, Response = "));
  radio.readBytesUntil(4,Buffer,15); //listen until "end of comm" (4) or 15 char
  for (i = 0; i < 13; i = i + 1) {
    Serial.print(Buffer[i]); //print out char one at a time
  }
  Serial.println(); //return
  Serial.print(F("Success??"));
  if (Buffer[12] == '0') { //is position 12 a zero, indicating success?
    Serial.println(F("YES!"));
  } else {
    Serial.println(F("No."));
    digitalWrite(ErrorLED, HIGH); //LED alarm
  }
  
  delay(1000); //otherwise radio module responds with handshake again
  
  Serial.println(F("initializing settings"));
  radio.println(setting); //send AT command for settings
  Serial.print(F("response = "));

  radio.readBytesUntil(4,Buffer,15);
  for (i = 0; i < 14; i = i + 1) {
    Serial.print(Buffer[i]);
  }  
  Serial.println();
  Serial.println(); //return
  Serial.print(F("Success??"));
  if (Buffer[13] == '0') {
    Serial.println(F("YES!"));
  } else {
    Serial.println(F("No."));
    digitalWrite(ErrorLED, HIGH);
  }
  
 /**********************Set up the timer and other******************/
 
  timer = millis(); //set the timer
  //mySerial.listen(); //otherwise you will only pay attention to the radio 

  }

void loop() {
 
  char c = GPS.read(); //read from gps

  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
     if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
    
    // Sentence parsed! 
    Serial.println(F("OK"));
    if (LOG_FIXONLY && !GPS.fix) { //do we have a fix? do you care?
        Serial.print(F("No Fix"));
        return;
    }

    // Rad. lets log it!
    Serial.println(F("Log"));
   //for some reason, you must open, write, and close for each sentence.
    Serial.println(GPS.lastNMEA());
    myFile = SD.open(GPSFile, FILE_WRITE); //open sd file so you're ready
    myFile.print(GPS.lastNMEA());  //write the string to the SD file
    myFile.close();
    
    //every now and then, radio lat, long, and altitude
 
 // if millis() or timer wraps around, we'll just reset it
  if (timer > millis())  timer = millis();

  //every defined number of minutes or so, radio out the current stats
  if (millis() - timer > (radTime*60000)) { //this code only happens every now and then
    timer = millis(); // reset the timer
   
    //get data
    float lat = GPS.latitude;
    float lon = GPS.longitude;
    int alt = GPS.altitude;
    //debug Printout
    Serial.print(lat);
    Serial.print(lon);
    Serial.println(alt);
    //Convert floats and int to string
    String sumInfo; //I don't know why, but must be done one at a time
    sumInfo += lat;
    sumInfo += F("N");
    sumInfo += lon;
    sumInfo += F("W");
    sumInfo += alt;
    sumInfo += F("m  ");
    sumInfo += call;
    
    char Transmission[70]; //char array for sending to radio

    //convert for sending to radio
    sumInfo.toCharArray(Transmission, 70);
    Serial.println(Transmission);
    
    //the big moment
    digitalWrite(PTT, LOW); //push-to-talk pin low
    for (byte i = 0; i<4; i++){ //send 4 times in a row
      encode(Transmission);
    }
    digitalWrite(PTT, HIGH); //turn off radio when you're done
  }
  }
}

void
encodechar(int ch)
{
    int i, x, y, fch ;
    word fbits ;
 
    /* It looks sloppy to continue searching even after you've
     * found the letter you are looking for, but it makes the
     * timing more deterministic, which will make tuning the
     * exact timing a bit simpler.
     */
    for (i=0; i<NGLYPHS; i++) {
        fch = pgm_read_byte(&glyphtab[i].ch) ;
        if (fch == ch) {
            for (x=0; x<7; x++) {
                fbits = pgm_read_word(&(glyphtab[i].col[x])) ;
                for (y=0; y<14; y++) {
                    if (fbits & (1<<y))
                        tone(radioPin, 262) ;
                    else
                        noTone(radioPin) ;
                         
                    delayMicroseconds(4045L) ;
                }
            }
        }
    }
}
 
void
encode(char *ch)
{
    while (*ch != '\0')
        encodechar(*ch++) ;
}
