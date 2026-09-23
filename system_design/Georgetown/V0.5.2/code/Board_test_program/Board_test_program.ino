/* this is to test if all the electronics are working */
#include <SPI.h>                        // for SPI
#include <SD.h>                         // for SD card module
#include <Wire.h>                       // for I2C
#include <HCSR04.h>                     // for the USS
#include <DS3232RTC.h>                  // for the RTC https://github.com/JChristensen/DS3232RTC
DS3232RTC myRTC;

#include <Adafruit_Sensor.h>            // for BME 
#include <Adafruit_BME280.h>            // for BME

#define SEALEVELPRESSURE_HPA (1013.25)  // constant for bme

const int powerLatchPin = 4;           // HIGH = Cut Power / LOW = Keep Power On

unsigned long alarmInterval = 5;

// HC-SR04 ----------------------------------------------------------------------------------------------
const int trigPin = 9;
const int echoPin = 8;
UltraSonicDistanceSensor distanceSensor(trigPin, echoPin);

// SD ---------------------------------------------------------------------------------------------------
const int pinCS = 10;

// nRF ---------------------------------------------------------------------------------------------------
const int rfpinCS = 7;

// BME280 -----------------------------------------------------------------------------------------------
Adafruit_BME280 bme;



// controls ---------------------------------------------------------------------------------------------
const int LED = A3;

unsigned long prevTimeElapsed = 0;

void setup() {
  Serial.begin(9600);
  pinMode(pinCS, OUTPUT);
  pinMode(powerLatchPin, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  digitalWrite(powerLatchPin, LOW); 
 
  // SD card initialization --------------------------------------------------------------------------------------------------------------------------
  Serial.println("Checking SD card...");
  delay(500);
  if (!SD.begin(pinCS))
  {
    digitalWrite(LED, HIGH);            // LED remains on if SD card does not work
    Serial.println("no SD found");
  }
  else
  {
    Serial.println("SD card OK");
  }

  

  // BME initialization ------------------------------------------------------------------------------------------------------------------------------
  Serial.println("Checking BME280...");
  if (!bme.begin(0x76))
  {
    digitalWrite(LED, HIGH);            // LED remains on if SD card does not work
    Serial.println("BME280 error");
  }
  else
  {
    Serial.println("BME280 OK");
  }
  

   // RTC initializaiton ------------------------------------------------------------------------------------------------------------------------------
  // initialize the alarms to known values, clear the alarm flags, clear the alarm interrupt flags
  myRTC.begin();
  myRTC.setAlarm(DS3232RTC::ALM1_MATCH_DATE, 0, 0, 0, 1);
  myRTC.setAlarm(DS3232RTC::ALM2_MATCH_DATE, 0, 0, 0, 1);
  myRTC.alarm(DS3232RTC::ALARM_1);
  myRTC.alarm(DS3232RTC::ALARM_2);
  myRTC.alarmInterrupt(DS3232RTC::ALARM_1, false);
  myRTC.alarmInterrupt(DS3232RTC::ALARM_2, false);
  myRTC.squareWave(DS3232RTC::SQWAVE_NONE);
  
  // get the current time from the RTC and set an alarm according to the time interval
                            
  time_t t = myRTC.get();                            
  time_t a = t + alarmInterval - t % alarmInterval;
  if (a <= t) a += alarmInterval;
  // set the alarm
  myRTC.setAlarm(DS3232RTC::ALM1_MATCH_HOURS, second(a), minute(a), hour(a), 0);
  myRTC.alarm(DS3232RTC::ALARM_1);    // clear the alarm flag
  myRTC.alarmInterrupt(DS3232RTC::ALARM_1, true);

}

void loop() {
  digitalWrite(LED, LOW);     // turn off LED before sleeping
  delay(10);
  for(int i=0;i<=5;i++){
    logData();
  }
  power_off();
}



void power_off() {
  delay(100);                               
                          
  // Set next alarm
  time_t a = myRTC.get() + alarmInterval;
  myRTC.setAlarm(DS3232RTC::ALM1_MATCH_HOURS, second(a), minute(a), hour(a), 0);
  myRTC.alarm(DS3232RTC::ALARM_1);               // Clear current alarm flag
  myRTC.alarmInterrupt(DS3232RTC::ALARM_1, true); // Re-enable interrupt pin
  myRTC.writeRTC(0x0E, 0x45);                     // Set BBSQW = 1 so alarm works on coincell battery

  delay(50);      // Give hardware a moment to stabilize

  pinMode(powerLatchPin, OUTPUT);
  digitalWrite(powerLatchPin, HIGH); // Pushing this HIGH actively cuts the power module
}


void logData() {
  // this is the data collection function
  unsigned long timeElapsed = millis();
  ////Serial.println("Recording data...");
  digitalWrite(LED, HIGH);
  delay(10);

  // take five distance readings and average them
  float dist[5];
  float total = 0;
  float num = 0;
  for (int i = 0; i < 5; i++) {
    dist[i] = distanceSensor.measureDistanceCm();
    delay(500);
    if (dist[i] > 0) {
      total = total + dist[i];
      num++;
    }
  }
  float avgDist = total / num;

  // record the ambient temperature, humidity, pressure
  float temp = bme.readTemperature();
  float humidity = bme.readHumidity();
  float pressure = bme.readPressure() / 100.0F;
  delay(50);



  // print the data to the file that will be saved on the SD car


  // write the RTC data
  time_t t = myRTC.get();
  Serial.print(String(month(t)));
  Serial.print("/");
  Serial.print(String(day(t)));
  Serial.print("/");
  Serial.print(String(year(t)));
  Serial.print(" ");
  Serial.print(String(hour(t)));
  Serial.print(":");
  Serial.print(String(minute(t)));
  Serial.print(":");
  Serial.print(String(second(t)));
  Serial.print(",");

  Serial.print(avgDist); Serial.print(",");

  // write the BME data
  Serial.print(temp); Serial.print(",");
  Serial.print(humidity); Serial.print(",");
  Serial.print(pressure); Serial.print(",");

  Serial.println("");
  ////Serial.print("Complete! Elapsed time: "); ////Serial.print(timeElapsed - prevTimeElapsed); ////Serial.println(" ms");
  prevTimeElapsed = timeElapsed;
  delay(100);
  digitalWrite(LED, LOW);
}
