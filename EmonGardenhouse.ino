/*
EmonGardenhouse
An Arduino Mini Pro, AM2302 Humidity/temp sensor and a RFM12B. 
Used as a temperature/humidity sensor. 

Schematic also in source folder. 

©Patrik Hermansson 2015
*/

// DHT11 Humidity/Temp sensor
#include <dht.h>
#define dht_dpin 3 //no ; here. Set equal to channel sensor is on
dht DHT;

#include <JeeLib.h>   
#include <avr/sleep.h>
ISR(WDT_vect) { Sleepy::watchdogEvent(); }  // Attached JeeLib sleep function to Atmega328 watchdog -enables MCU to be put into sleep mode inbetween readings to reduce power consumption 

// For RFM12B
#define MYNODE 22            // Should be unique on network, node ID 30 reserved for base station
#define RF_freq RF12_433MHZ     // frequency - match to same frequency as RFM12B module (change to 868Mhz or 915Mhz if appropriate)
#define group 210 

// EnergyMonitor
#include "EmonLib.h"                   // Include Emon Library
EnergyMonitor emon1;                   // Create an instance

//---------------------------------------------------
// Data structures for transfering data between units
//---------------------------------------------------
typedef struct { 
  byte glcdspace;
  byte hour;
  byte minute;
} PayloadTX;         // neat way of packaging data for RF comms
PayloadTX emontx;

typedef struct { int temperature, humidity, volt; } PayloadGLCD;
PayloadGLCD emonglcd;
//-------------------------------------------------------------------------------------------- 
// Flow control
//-------------------------------------------------------------------------------------------- 
unsigned long last_emontx;                   // Used to count time from last emontx update
unsigned long last_emonbase;                   // Used to count time from last emontx update

// Time
String shour, smin;
int ihour, imin;

boolean answer;

// Battery monitor
int sensorPin = A0;    // select the input pin for the potentiometer
int sensorValue = 0;  // variable to store the value coming from the sensor
float volt;

void setup()   {
  Serial.begin(9600);
  Serial.println("Welcome to EmonGardenhouse!");
  
  // Init RF
  delay(500); 				   //wait for power to settle before firing up the RF
  rf12_initialize(MYNODE, RF_freq,group);
  rf12_control(0xC040);                                                 // set low-battery level to 2.2V i.s.o. 3.1V
  delay(10);
  rf12_sleep(RF12_SLEEP);
  //wait for RF to settle   
  
  // DHT11 on
  //pinMode(3, OUTPUT);     
  //digitalWrite(3, HIGH);
  
  Serial.println("Com test with base");
  emonglcd.temperature = 0; 
  emonglcd.humidity = 0;
  delay(100);
  rf12_sendNow(0, &emonglcd, sizeof emonglcd);                     //send temperature data via RFM12B using new rf12_sendNow wrapper -glynhudson
  rf12_sendWait(2); 
  
  Serial.println("Done");
  delay(2000);
}
void loop() {
    // Send the values to base after the base has sent her message. 
    if (answer){
      delay(500);
      Serial.println("Calling base...");
          
      // Get temp and humidity from sensor
      emonglcd.temperature = (int) (DHT.temperature*100);                          // set emonglcd payload
      emonglcd.humidity = (int) (DHT.humidity);
      emonglcd.volt = (int) (volt*100);
      //emonglcd.humidity = 38;
      
      // Send data
      rf12_sleep(RF12_WAKEUP);
      // if ready to send + exit loop if it gets stuck as it seems too
      int i = 0; while (!rf12_canSend() && i<10) {rf12_recvDone(); i++;}
      rf12_sendStart(0, &emontx, sizeof emontx);
      // set the sync mode to 2 if the fuses are still the Arduino default
      // mode 3 (full powerdown) can only be used with 258 CK startup fuses
      rf12_sendWait(2);
      rf12_sleep(RF12_SLEEP); 
      delay(50);
      
      //rf12_sendNow(0, &emonglcd, sizeof emonglcd);                     //send temperature data via RFM12B using new rf12_sendNow wrapper -glynhudson
      //rf12_sendWait(10); 
      answer = false;
      //last_emonbase = millis();
    }
    
    // Read battery voltage
    sensorValue = analogRead(sensorPin);
    volt = sensorValue * (3.32 / 1023.0);
  
    // Read humidity and temp  
    DHT.read22(dht_dpin);
    
    // Print results on serial port...
    Serial.print("Battery = ");
    Serial.print(volt);
    Serial.print("V H = ");
    Serial.print(DHT.humidity);
    Serial.print("%  T = ");
    Serial.print(DHT.temperature); 
    Serial.println("C  ");

  delay(5000);    
  // Signal from the emonTx received. Here we collect and format the current time.   
  if (rf12_recvDone())
  {
    if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)  // and no rf errors
    {
      int node_id = (rf12_hdr & 0x1F);
      
      // Node 16 is the emonBase. It sends out some date, for example the current time. 
      if (node_id == 16)			//Assuming 16 is the emonBase node ID
      {
        Serial.println("Got message from emonBase");
        //RTC.adjust(DateTime(2012, 1, 1, rf12_data[1], rf12_data[2], rf12_data[3]));
        emontx = *(PayloadTX*) rf12_data; 
        
        // Get time from server
        // Hour
        //int hour = rf12_data[1];
        int hour = emontx.hour;
        //Serial.print ("hour: ");
        //Serial.println (hour);

        if (hour<10) {
          //Serial.println ("Less than 10");
          shour = "0" + String(hour);
        }  
        else {
          shour = String(hour);
        }
        //Serial.println (shour);
        // Minute
        int min = emontx.minute;

        //Serial.print ("M: ");        
        //Serial.println (min);
        if (min<10) {
          smin = "0"+String(min);
          //Serial.print ("SM: ");        
          //Serial.println (smin);          
        }
        else { 
          smin = String(min);
          //Serial.print ("SM: ");        
          //Serial.println (smin);        
        }
        String time = shour + smin;
        int itime = time.toInt();
        Serial.println(itime);
        // Set flag that tells code to answer to this transmission. 
        answer = true;
        last_emonbase = millis();
      } 
      //Serial.print("node_id: ");
      //Serial.println(node_id);

    }
  }
}
//End
