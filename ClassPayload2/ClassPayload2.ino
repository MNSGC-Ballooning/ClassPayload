#include <UbloxGPS.h>
//#include <SoftwareSerial.h> // do not need when using a mega as it has dedicated serial ports
#include <RelayXBee.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SD.h>

//SoftwareSerial ubloxSer(2,3); //Do not include if using teensy or mega. This is necessary when using arduino uno as it does not have dedicated hardware serial.
#define ubloxSer Serial1 // hardware serial RX1, TX1 on arduino mega
#define xbeeSer Serial2  // hardware serial RX1, TX1 on arduino mega
#define pressure_pin A12 // Honeywell pressure sensor data pin.
#define temperature_pin 25 // DALLAS temperature sensor data pin
#define sdLED_pin 10 // LED that indicates SD logging (YELLOW)
//#define chipSelect 53 //designated chipselect SD card pin on arduino mega. 
#define chipSelect 8 // designated chipSelect pin when using Mega with sparkfun MicroSD shield
#define xbeeLED_pin 12 // LED that indicates xbee radio communications (YELLOW)
#define fixLED_pin 14 // LED that indicates ublox GPS fix
#define altLED_pin 15 // LED that indicates whether altitude is logging as zero or not (not synonymous with fix)

OneWire tempBus(temperature_pin);
DallasTemperature temperature(&tempBus); 
UbloxGPS ublox = UbloxGPS(&ubloxSer);
String ID = "CP";                    // ****************************************** Change this to personalize your xbee ID. no two groups can have the same ID *****************
RelayXBee xbee = RelayXBee(&xbeeSer,ID);
boolean fix = false; // true if your GPS is connected, false if not
boolean altBool = false; // 

String PANID = "000A"; // This will be the PANID used for all payloads on stack A
//String PANID = "000B"; // This will be the PANID used for all payloads on stack B

File datalog; 
String data;
String command;
char datalogName[] = "ClassPayload00.csv"; // Don't change this please
bool datalogOpen = false;
int datacounter = 0;

void setup() {
 SDsetup();
 ubloxSetup();
 xbeeSetup();
}

void ubloxSetup(){
 //Start serial
  Serial.begin(9600); //Make sure this matches the Baud rate when you open your Serial monitor
  pinMode(xbeeLED_pin,OUTPUT);
  pinMode(sdLED_pin,OUTPUT);
  pinMode(altLED_pin,OUTPUT);
  pinMode(fixLED_pin,OUTPUT);
  temperature.begin();
  //Start ublox
  ubloxSer.begin(UBLOX_BAUD);
  
  while(!Serial){
    ; //Wait for serial port to connect
  }
  
  //Start GPS 
  ublox.init();                                          //*******************************************************************************************
  //Attempt to set to airborne 3 times. If successful, records result and breaks loop. If unsuccessful, saves warning and moves on
  byte i = 0;                                            //*******************************************************************************************
  while (i<50) {                                         //*******************************************************************************************
    i++;                                                 //*******************************************************************************************
    if (ublox.setAirborne()) {                           //*************** IF YOU ARE USING ADAFRUIT GPS THIS IS NOT NECESSARY ***********************
      Serial.println("Air mode successfully set.");     //*******************************************************************************************
      break;                                            //*******************************************************************************************
    }                                                   //*******************************************************************************************
    else if (i ==50)                                    //*******************************************************************************************
      Serial.println("WARNING: Failed to set to air mode (50 attemtps). Altitude data may be unreliable.");
    else                                                //*******************************************************************************************
      Serial.println("Error: Air mode set unsuccessful. Reattempting...");
  }                                                     //*******************************************************************************************
  Serial.println("GPS configured");
}

void xbeeSetup(){
  xbeeSer.begin(XBEE_BAUD);
  xbee.init('A');
  xbee.enterATmode();
  xbee.atCommand("ATID" + PANID);// Sets the PANID based on what stack the payload is on.
  xbee.atCommand("ATDL0"); 
  xbee.atCommand("ATMY1");
  xbee.exitATmode();
  Serial.println("Xbee Initialized");
}

void SDsetup(){
  pinMode(chipSelect,OUTPUT);
  if (!SD.begin(chipSelect)) {
    Serial.println("Problem initializing sd card! \n Make sure SD card is correctly placed and restart.");//Display an error if SD communication fails
    
    while(true) {    //this part of the loop ensures that the LEDs blink IF the SD isnt working (to catch attention before flight, Im assuming)
      digitalWrite(sdLED_pin,HIGH);
      delay(100);
      digitalWrite(sdLED_pin,LOW);
      delay(100);
      }}
  else{  //file creation process. Don't worry about this too much
    for (byte i = 0; i < 100; i++) {
      datalogName[12] = '0' + i/10;
      datalogName[13] = '0' + i%10;
      if (!SD.exists(datalogName)) {  //make sure both file names are available before opening them
        openDatalog();
        break;
      }
    }
  }
  }
  
void openDatalog() {
  if (!datalogOpen) {
    datalog = SD.open(datalogName, FILE_WRITE);
    datalogOpen = true;}
}

void closeDatalog() {
  if (datalogOpen) {
    datalog.close();
    datalogOpen = false;}
}

void logData(String dataa) {
  openDatalog();
  datalog.println(dataa);
  Serial.println(dataa);
  digitalWrite(sdLED_pin,HIGH);
  delay(50);
  digitalWrite(sdLED_pin,LOW);
  closeDatalog();
}

void getUbloxData(){
  ublox.update();

  //log data once every second
  if(millis()%1500 == 0) {
    datacounter++;
    //All data is returned as numbers (int or float as appropriate), so values must be converted to strings before logging
    double alt = ublox.getAlt_feet();
    String data = String(ublox.getMonth()) + "/" + String(ublox.getDay()) + "/" + String(ublox.getYear()) + ","
                  + String(ublox.getHour()-5) + ":" + String(ublox.getMinute()) + ":" + String(ublox.getSecond()) + ","
                  + String(ublox.getLat()) + "," + String(ublox.getLon()) + "," + String(alt) + ","
                  + String(ublox.getSats()) + "," + String(temperatureC()) + "," + String(pressure()) + ",";
    //GPS should update once per second, if data is more than 2 seconds old, fix was likely lost
    if(ublox.getFixAge() > 2000){
      data += "No Fix,";
      fix = false;}
    else{
      data += "Fix,";
      fix = true;
    }
    if(fix){digitalWrite(fixLED_pin, datacounter%2);}
    
    if(alt == 0.0 && altBool){
      altBool = false;
      digitalWrite(altLED_pin,altBool);
      goto here;
    }
    if(alt == 0.0 && !altBool){
      altBool = true;
      digitalWrite(altLED_pin,altBool);
      goto here;
    }
    here:
    logData(data);
}}

void updateXbee() {
  digitalWrite(xbeeLED_pin,LOW); //will make sure the LED is off when a cycle begins so you can look for the LED to turn on to indicate communication
  
  if(xbeeSer.available()>0){
    command = xbee.receive();
    digitalWrite(xbeeLED_pin,HIGH);}        //check xBee for incoming messages that match ID
  if (command.equals("status")){
    xbee.send(data);
    delay(100);
    digitalWrite(xbeeLED_pin,LOW);}
  else if (command.equals("temperature")){
    xbee.send("temperature = " + String(temperatureC()));
    delay(100);
    digitalWrite(xbeeLED_pin,LOW);}
  else if (command.equals("pressure")){
    xbee.send("pressure = " + String(pressure()));
    delay(100);
    digitalWrite(xbeeLED_pin,LOW);
    }
  else if (command.equals("ping"))
  {
    xbee.send("I'm still alive but i'm barely breathin'!"); // personalize this to say whatever
    delay(100);
    digitalWrite(xbeeLED_pin,LOW);
  }
  else{xbee.send("ERROR. Cannot identify command: " + command);} // notice the delay and LED flip is not added. Look for more than a second long xbee blink to indicate message error.
}

double temperatureC(){
  temperature.requestTemperatures();
  double temp = temperature.getTempCByIndex(0);
  return temp; 
}

double pressure(){
  double rawPressure = analogRead(pressure_pin);
  double pressureV = rawPressure*(5.0/1024.0);
  double pressuree = ((pressureV-0.5)*(15.0/4.0));
  return pressuree;
}
void loop() {
  getUbloxData();
  updateXbee();
}
