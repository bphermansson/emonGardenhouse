/*
  Modded by Patrik Hermansson 2015. Uses another DHT-lib and different way
  to read battery voltage. 
  
  emonTX LowPower Temperature Example 
 
  Example for using emonTx with two AA batteries connected to 3.3V rail. 
  Voltage regulator must not be fitted
  Jumper between PWR and Dig7 on JeePort 4

  This setup allows the DHT22 to be switched on/off by turning Dig7 HIGH/LOW
 
  Part of the openenergymonitor.org project
  Licence: GNU GPL V3
 
  Authors: Glyn Hudson, Trystan Lea
  Builds upon JeeLabs RF12 library and Arduino

  THIS SKETCH REQUIRES:

  Libraries in the standard arduino libraries folder:
	- JeeLib		https://github.com/jcw/jeelib
        - DHT22 Humidity        https://github.com/adafruit/DHT-sensor-library - be sure to rename the sketch folder to remove the '-'


 
*/

#define RF_freq RF12_433MHZ                                                // Frequency of RF12B module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.
const int nodeID = 22;                                                  // emonTx RFM12B node ID - should be unique on network, see recomended node ID range below
const int networkGroup = 210;                                           // emonTx RFM12B wireless network group - needs to be same as emonBase and emonGLCD

/*Recommended node ID allocation
------------------------------------------------------------------------------------------------------------
-ID-	-Node Type- 
0	- Special allocation in JeeLib RFM12 driver - reserved for OOK use
1-4     - Control nodes 
5-10	- Energy monitoring nodes
11-14	--Un-assigned --
15-16	- Base Station & logging nodes
17-30	- Environmental sensing nodes (temperature humidity etc.)
31	- Special allocation in JeeLib RFM12 driver - Node31 can communicate with nodes on any network group
-------------------------------------------------------------------------------------------------------------
*/
                                           
//const int time_between_readings= 60000;                                  //60s in ms - FREQUENCY OF READINGS 
const int time_between_readings= 60000;                                  //60s in ms - FREQUENCY OF READINGS 
#define RF69_COMPAT 0 // set to 1 to use RFM69CW 
#include <JeeLib.h>   // make sure V12 (latest) is used if using RFM69CW
#include <avr/sleep.h>
ISR(WDT_vect) { Sleepy::watchdogEvent(); }                              // Attached JeeLib sleep function to Atmega328 watchdog -enables MCU to be put into sleep mode inbetween readings to reduce power consumption 

// DHT11 Humidity/Temp sensor
#include <dht.h>
#define dht_dpin 3 //no ; here. Set equal to channel sensor is on
dht DHT;

// Connect pin 1 (on the left) of the sensor to +5V
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 4.7-10K resistor from pin 2 (data) to pin 1 (power) of the sensor

// Battery monitor
int sensorPin = A0;    // select the input pin for the potentiometer
int sensorValue = 0;  // variable to store the value coming from the sensor
float volt;

typedef struct {
  	  int temp;
      int humidity;                                  
	    int battery;		                                      
} Payload;
Payload emontx;

int oldtemp, oldhumidity, oldbattery;
boolean firstrun = true;

void setup() {
  Serial.begin(38400);
  Serial.println("emonTX Low-power humidity example"); 
  Serial.println("OpenEnergyMonitor.org");
  Serial.print("Node: "); 
  Serial.print(nodeID); 
  Serial.print(" Freq: "); 
  if (RF_freq == RF12_433MHZ) Serial.print("433Mhz");
  if (RF_freq == RF12_868MHZ) Serial.print("868Mhz");
  if (RF_freq == RF12_915MHZ) Serial.print("915Mhz"); 
  Serial.print(" Network: "); 
  Serial.println(networkGroup);

  pinMode(7,OUTPUT);                                                    // DHT22 power control pin - see jumper setup instructions above
  digitalWrite(7,HIGH);                                                 // turn on DHT22
  delay(2000);                                                          // wait 2s for DH22 to warm up

  rf12_initialize(nodeID, RF_freq, networkGroup);                          // initialize RFM12B
  rf12_control(0xC040);                                                 // set low-battery level to 2.2V i.s.o. 3.1V
  delay(10);
  rf12_sleep(RF12_SLEEP);
}

void loop()
{ 
  // Store previous value
  
  // Turn on DHT-sensor
  digitalWrite(7,HIGH); 
  delay(2000);                                     //wait 2s for sensor                                                                // Send the command to get temperatures
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  
  // Read values and check for sanity
  readdht:
  // Read humidity and temp  
  DHT.read22(dht_dpin);
  delay(100);

  // Read battery voltage
  sensorValue = analogRead(sensorPin);
  delay(10);
  volt = sensorValue * (3.32 / 1023.0);

  if (DHT.temperature < -60 || DHT.temperature > 60 || DHT.humidity < 0 || DHT.humidity > 100) {
    // The values can't be right, read again
    Serial.println("Illegal values from some sensor, read again");
    delay(2000);
    goto readdht;
  }

  // First time entering the loop?
  if (firstrun == true) {
    // Dont compare new value to old value the first time (because the oldvalue is always zero)
    firstrun = false;
  }
  else {
    if (DHT.temperature-oldtemp>6 || oldtemp-DHT.temperature>6) {
      // The values differs to much
      Serial.println("Illegal values from temp sensor, read again");
      delay(2000);
      goto readdht;    
    }
    if (DHT.humidity-oldhumidity>4 || oldhumidity-DHT.humidity>4) {
      // The values differs to much
      Serial.println("Illegal values from humidity sensor, read again");
      delay(2000);
      goto readdht;    
    }
  }
  // Save previous reading
  oldtemp=DHT.temperature;
  oldhumidity=DHT.humidity;
  oldbattery=volt;
  // DHT off
  digitalWrite(7,LOW);
  
  // Raw values
  Serial.print ("DHT.temperature = ");
  Serial.println (DHT.temperature);
  Serial.print ("DHT.humidity = ");
  Serial.println (DHT.humidity);
  Serial.print ("Battery = ");
  Serial.print (volt);
  Serial.print (" V, AD = ");
  Serial.println (sensorValue);
  
  emontx.temp = (int) (DHT.temperature*10);                          // set emonglcd payload
  emontx.humidity = (int) (DHT.humidity);
  emontx.battery = (int) (volt*100);
  
  delay(10);
  
    // Print results on serial port...
    Serial.println("Garbage");
    Serial.print("Battery = ");
    Serial.print(emontx.battery );
    Serial.print("V H = ");
    Serial.print(DHT.humidity);
    Serial.println("%");
    Serial.print("T = ");
    Serial.print(emontx.temp); 
    Serial.println("C  ");

  delay(5);

  
   // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(emontx.temp) || isnan(emontx.humidity)) {
    Serial.println("Failed to read from DHT");}
    else
    {
      
      rf12_sleep(RF12_WAKEUP);
      // if ready to send + exit loop if it gets stuck as it seems too
      int i = 0; while (!rf12_canSend() && i<10) {rf12_recvDone(); i++;}
      rf12_sendStart(0, &emontx, sizeof emontx);
      // set the sync mode to 2 if the fuses are still the Arduino default
      // mode 3 (full powerdown) can only be used with 258 CK startup fuses
      rf12_sendWait(2);
      rf12_sleep(RF12_SLEEP); 
      delay(50);
    }
    
  Sleepy::loseSomeTime(time_between_readings);
}

