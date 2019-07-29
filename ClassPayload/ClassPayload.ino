#include <FlightGPS.h>
#include <UbloxGPS.h>
#include <RelayXBee.h>
#include <SoftwareSerial.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPS++.h>

File datalog;
char datalogName[] = "Sensor00.csv";
bool datalogOpen = false;
bool fix = false;
unsigned long int timer = 0;

String ID = "HI";
String data;

#define UBLOX Serial2 //SoftwareSerial UBLOX(2,3); // RX , TX
#define XBEE Serial1 //SoftwareSerial XBEE(4,5);
#define sdLED_pin 6
#define fixLED_pin 7
#define pressure_pin A14
#define temp_pin A15
#define chipSelect_pin 53

RelayXBee xbee(&XBEE, ID); //not sure why you have to put 'A' here, but you do.
UbloxGPS ublox(&UBLOX);

int x, y, z;

void setup() {
  SDsetup();
  sensorSetup();
}

void loop() {
  updateSensors();
  updateXbee();
}

void SDsetup() {
  pinMode(sdLED_pin, OUTPUT);
  pinMode(chipSelect_pin, OUTPUT);
  Serial.begin(9600);
  Serial.println("test");
  if (!SD.begin(chipSelect_pin)) {
    Serial.println("Problem initializing sd card");//Display an error if SD communication fails
    while(true) {    //this part of the loop ensures that the LEDs blink IF the SD isnt working (to catch attention before flight, Im assuming)
      digitalWrite(sdLED_pin,HIGH);
      delay(1000);
      digitalWrite(sdLED_pin,LOW);
      delay(1000);
      };} //Note that this error loop is never broken - check for slow blinking LEDs before flying

  else{  //file creation process
    for (byte i = 0; i < 100; i++) {
      datalogName[6] = '0' + i/10;
      datalogName[7] = '0' + i%10;
      if (!SD.exists(datalogName)) {  //make sure both file names are available before opening them
        openDatalog();
        break;
      }
    }
  }
} 

void sensorSetup()
{
  Serial.println("test2");
  xbee.init('A');
  Serial.println("test3");
  pinMode(fixLED_pin,OUTPUT);
  UBLOX.begin(UBLOX_BAUD);
  ublox.init();
  delay(50); //sometimes airborne will not set if there is no delay
  ublox.setAirborne();
  for(byte i=0; i<3; i++){ 
    if (ublox.setAirborne()) {
      datalog.println("Air mode successfully set.");
      Serial.println("Air mode successfully set.");
      break;
    }}

  String header = "GPS Time,Lat,Lon,Alt (ft),# Sats,Temp (C),Pressure (psi)";
  logData(header);
}

void updateSensors(){
  while(UBLOX.available()){ublox.update();}

  if (millis() - timer > 1000) { //change as the blink sequence will now take up this time
    timer = millis();
    
     data = String(ublox.getHour()-5) + ":" + String(ublox.getMinute()) + ":" + String(ublox.getSecond()) + ", "
                + String(ublox.getLat(), 4) + ", " + String(ublox.getLon(), 4) + ", " + String(ublox.getAlt_feet()) + ", "
                + String(ublox.getSats()) + ", " + String(temperature()) + ", " + String(pressure()) + ", ";

     if(ublox.getFixAge() > 2000){
      data += " No Fix,";
      fix = false;
     }
     else{
      data+= " Fix,";
      fix = true;
     }
     logData(data);}
}

void updateXbee() {
  String command = xbee.receive();        //check xBee for incoming messages that match ID
  if (command == "") return;

  if (command == "DATA")
    xbee.send(data);
}

//functions to change file state (open/closed) and handle the appropriate status LED
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

//functions that handle both opening and closing of files when logging data to ensure it is saved properly
void logData(String dataa) {
  openDatalog();
  datalog.println(dataa);
  Serial.println(dataa);
  closeDatalog();
}

double pressure()
{
  double rawPressure = analogRead(pressure_pin);
  double pressureV = rawPressure*(5.0/1024.0);
  double pressuree = ((pressureV-0.5)*(15.0/4.0));
  return pressuree;
}

double temperature(){
  double rawTemp = analogRead(temp_pin);
  double tempV = rawTemp*(5.0/1024.0);
  double temp = ((tempV-0.5)*100);
  return temp;
}
