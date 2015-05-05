/*
EmonGardenhouse
An Arduino Mini Pro, AM2302 Humidity/temp sensor and a RFM12B. 
Used as a temperature/humidity sensor. 

Schematic also in source folder. 

©Patrik Hermansson 2015
*/

// DHT11 Humidity/Temp sensor
#include <dht.h>
#define dht_dpin A5 //no ; here. Set equal to channel sensor is on
dht DHT;

#include <JeeLib.h>   
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

typedef struct { int temperature, humidity, power; } PayloadGLCD;
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
double Irms;

void setup()   {
  Serial.begin(9600);
  Serial.println("Welcome to EmonGardenhouse!");
  
  // Init RF
  delay(500); 				   //wait for power to settle before firing up the RF
  rf12_initialize(MYNODE, RF_freq,group);
  delay(100);	 
  //wait for RF to settle before turning on display
   
  // Init energy monitor
  // http://openenergymonitor.org/emon/buildingblocks/ct-and-ac-power-adaptor-installation-and-calibration-theory
  // http://openenergymonitor.org/emon/buildingblocks/calibration
  //emon1.current(0, 10);             // Current: input pin, calibration.
  emon1.current(4, 10);
   
  Serial.println("Start lcd");
  // Start LCD
  display.begin();
  delay(500);
  // you can change the contrast around to adapt the display
  // for the best viewing!
  display.setContrast(50);

  display.display(); // show splashscreen
  delay(2000);
  display.clearDisplay();   // clears the screen and buffer
  
  // Backlight on
  pinMode(9, OUTPUT);     
  digitalWrite(9, HIGH);
  
  // DHT11 on
  pinMode(3, OUTPUT);     
  digitalWrite(3, HIGH);
  
  // draw a single pixel
  display.drawPixel(10, 10, BLACK);
  display.display();
  Serial.println("Dot is there?");
  delay(2000);
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  display.println("Hello, world!Are there a line break here?");
  display.setTextSize(2);
  display.println("Yes");
  display.setTextSize(1);
  display.println("So this was easy!");
  display.display();
  
  Serial.println("Com test with base");
  emonglcd.temperature = 0; 
  emonglcd.humidity = 0;
  delay(100);
  rf12_sendNow(0, &emonglcd, sizeof emonglcd);                     //send temperature data via RFM12B using new rf12_sendNow wrapper -glynhudson
  rf12_sendWait(2); 
  
  Serial.println("Done");
  
  delay(2000);
  display.clearDisplay();
  
  // Larger text as we move on
  display.setTextSize(3);

}
void loop() {
    // Send the values to base after the base has sent her message. 
    if (answer){
      delay(500);
      Serial.println("Calling base...");
      
      // Get power reading from Current Transformer
      Irms = emon1.calcIrms(1480);  // Calculate Irms only
      Serial.print(Irms*230.0);	       // Apparent power
      Serial.print(" ");
      Serial.println(Irms);		       // Irms
      
      // Get temp and humidity from sensor
      emonglcd.temperature = (int) (DHT.temperature*100);                          // set emonglcd payload
      emonglcd.humidity = (int) (DHT.humidity);
      emonglcd.power = (int) (Irms*230.0);
      //emonglcd.humidity = 38;
      rf12_sendNow(0, &emonglcd, sizeof emonglcd);                     //send temperature data via RFM12B using new rf12_sendNow wrapper -glynhudson
      rf12_sendWait(10); 
      answer = false;
      //last_emonbase = millis();
    }
    
    // Read humidity and temp  
    DHT.read11(dht_dpin);
    
    // Print results on serial port...
    Serial.print("H = ");
    Serial.print(DHT.humidity);
    Serial.print("%  T = ");
    Serial.print(DHT.temperature); 
    Serial.println("C  ");
    Serial.print("Irms: ");	       // Apparent power
    Serial.println(Irms*230.0);	       // Apparent power


    // ...and on the 5110-display
    display.setTextColor(BLACK);
    display.setCursor(0,0);
    display.setTextSize(2);
    // Temp
    display.print(DHT.temperature,1);
    display.println("C");
    // Humidity
    display.setTextSize(2);
    display.print(DHT.humidity,0);
    display.println("%");
    display.display();
    // Time goes in on the third row. 
    display.print(shour);
    display.print(":");
    display.println(smin);
    display.display();
    delay(100);
      
    delay(5000);
    display.clearDisplay();

    
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
