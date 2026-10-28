#include <SD.h>                         // for SD card module
#include <Wire.h>                       // for I2C
#include <HCSR04.h>                     // for the USS
#include <DS3232RTC.h>                  // for the RTC https://github.com/JChristensen/DS3232RTC
DS3232RTC myRTC;
#include <Adafruit_Sensor.h>            // for BME 
#include <Adafruit_BME280.h>            // for BME
#include <RF24.h>


#define SEALEVELPRESSURE_HPA (1013.25)  // constant for bme

// HC-SR04 ----------------------------------------------------------------------------------------------
#define trigPin 9     
#define echoPin 8      
UltraSonicDistanceSensor distanceSensor(trigPin, echoPin);

// SD ---------------------------------------------------------------------------------------------------
#define pinCS 10

// nRF ---------------------------------------------------------------------------------------------------
#define rfpinCS 7
#define rfpinCE 6
RF24 radio(rfpinCE, rfpinCS);

// BME280 -----------------------------------------------------------------------------------------------
Adafruit_BME280 bme;


// controls ---------------------------------------------------------------------------------------------
#define LED A3
float busvoltage;

const int powerLatchPin = 4;           // HIGH = Cut Power / LOW = Keep Power On

unsigned long alarmInterval = 10; // 10 min default
unsigned long prevTimeElapsed = 0;

// Returns the sleep interval in seconds based on LiPo voltage
unsigned long getDynamicInterval(float voltage) {
  if (voltage >= 8) return 10;        // 15 min
  else if (voltage >= 7.4) return 10;  // 15 min
  else return 10;                  // 15 hours
}

//nrf24
const byte radioAddress[6] = "00001";
const byte NUMBER_OF_READINGS = 5;
const uint16_t INVALID_DISTANCE = 65535;
uint16_t sampleNumber = 0;
const bool SET_RTC_FROM_COMPILE_TIME = true;
struct __attribute__((packed)) TelemetryPacket {
  uint16_t sampleNumber;
  uint32_t unixTime;
  uint16_t avgDist;
  uint16_t busVoltage;
};

uint16_t cmToMm(float distanceCm) {
  // Reject invalid, out-of-range, and NaN HC-SR04 readings.
  if (!(distanceCm >= 1.0)) {
    return INVALID_DISTANCE;
  }
  return (uint16_t)(distanceCm * 10.0 + 0.5);
}

float readAverageDistance() {
  float total = 0;
  int num = 0;
  for (int i = 0; i < 5; i++) {
    float distanceCm = distanceSensor.measureDistanceCm();
    if (distanceCm >= 1.0) {
      total += distanceCm;
      num++;
    }
  }
  return total/num;
}

bool setupRadio() {
  if (!radio.begin()) {
    return false;
  }
  radio.openWritingPipe(radioAddress);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);
  radio.setRetries(5, 15);
  radio.setPayloadSize(sizeof(TelemetryPacket));
  radio.setAutoAck(true);
  radio.stopListening();
  return true;
}

bool sendTelemetry(time_t t, float avgDist, float busvoltage) {
  TelemetryPacket packet;
  packet.sampleNumber = sampleNumber++;
  packet.unixTime = (uint32_t)t;
  packet.avgDist = cmToMm(avgDist);
  packet.busVoltage = (uint16_t)busvoltage;
  return radio.write(&packet, sizeof(packet));
}

void printTime(time_t t) {
  Serial.print(month(t));
  Serial.print('/');
  Serial.print(day(t));
  Serial.print('/');
  Serial.print(year(t));
  Serial.print(' ');
  Serial.print(hour(t));
  Serial.print(':');
  Serial.print(minute(t));
  Serial.print(':');
  Serial.println(second(t));
}

byte monthNumber(const char *monthText) {
  const char monthNames[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *match = strstr(monthNames, monthText);
  return match ? ((match - monthNames) / 3 + 1) : 1;
}

time_t compiledSketchTime() {
  char monthText[4];
  int day, year, hour, minute, second;
  tmElements_t tm;
  sscanf(__DATE__, "%3s %d %d", monthText, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
  tm.Year = CalendarYrToTm(year);
  tm.Month = monthNumber(monthText);
  tm.Day = day;
  tm.Hour = hour;
  tm.Minute = minute;
  tm.Second = second;
  return makeTime(tm);
}

void setup() {
  Serial.begin(9600);
  pinMode(powerLatchPin, OUTPUT);
  digitalWrite(powerLatchPin, LOW); // Pushing this HIGH actively cuts the power module
  pinMode(pinCS, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // SD card initialization --------------------------------------------------------------------------------------------------------------------------
  if (!SD.begin())                               
  {
    digitalWrite(LED, HIGH);            // LED remains on if SD card does not work
    Serial.println("no SD found");
    //while(true){
      //error_blink();                                    
    //}
  }
  else
  {
    Serial.println("SD found");
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

  // BME initialization ------------------------------------------------------------------------------------------------------------------------------
  bme.begin(0x76);

  //nrf24
  if (SET_RTC_FROM_COMPILE_TIME) {
    time_t newTime = compiledSketchTime();
    myRTC.set(newTime);
    Serial.print("");
    Serial.print("RTC UPDATE TO: ");
    printTime(newTime);
    Serial.print("");
  }
  if (!setupRadio()) {
    Serial.println("NRF24 not found!");
    while (true) {
    }
  }
  Serial.println("Transmitter Ready");


}

void loop() { 
  time_t t = myRTC.get();
  float avgDist = readAverageDistance();
  float busvoltage = analogRead(A0)*0.00978;;
  bool success = sendTelemetry(t, avgDist, busvoltage);
  if (success) {
    Serial.println("Wireless data sent.");
  } else {
    Serial.println("Wireless transmission failed.");
  }
  Serial.print("Time: ");
  printTime(t);
  Serial.print("Average distance: ");
  if (avgDist <= 0) {
    Serial.println("Invalid");
  } else {
    Serial.print(avgDist, 1);
    Serial.println("cm");
  }
  Serial.print("Bus Voltage: ");
  Serial.print(busvoltage, 1);
  Serial.println("V");
  Serial.println("----------------------");
  delay(60000);


  delay(10);
    // --- Read voltage and set dynamic alarm ---
  busvoltage = analogRead(A0)*0.00978;
  alarmInterval = getDynamicInterval(busvoltage);
    // Run the data logging function
  logData();  
    //set alarm and power off
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
  //Serial.println("Recording data...");
  digitalWrite(LED, HIGH);
  delay(10);

  // take five distance readings and average them
  float dist[5];
  float total = 0;
  int num = 0;
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

  // print the data to the file that will be saved on the SD card
  File myFile = SD.open("001.csv", FILE_WRITE);    // change to the file name you want to store the data

  if (myFile)                             // tests if the file has opened
  {
    // write the RTC data
    time_t t = myRTC.get();   //transmit
    myFile.print(String(month(t)));
    myFile.print("/");
    myFile.print(String(day(t)));
    myFile.print("/");
    myFile.print(String(year(t)));
    myFile.print(" ");
    myFile.print(String(hour(t)));
    myFile.print(":");
    myFile.print(String(minute(t)));
    myFile.print(":");
    myFile.print(String(second(t)));
    myFile.print(",");

    // write the USS data
    for (int i = 0; i < 5; i++) {
      myFile.print(dist[i]); myFile.print(",");
    }
    myFile.print(avgDist); myFile.print(","); //transmit

    // write the BME data
    myFile.print(temp); myFile.print(",");
    myFile.print(humidity); myFile.print(",");
    myFile.print(pressure); myFile.print(",");

    // write the power data
    myFile.print(busvoltage); myFile.print(",");  //transmit

    myFile.println("");
    myFile.close();           // closes and saves the file to the SD card
    Serial.print("Complete! Elapsed time: "); //Serial.print(timeElapsed - prevTimeElapsed); //Serial.println(" ms");
    prevTimeElapsed = timeElapsed;
  }
  else
  {
    Serial.println("Error opening file");
    digitalWrite(LED, HIGH);                // LED will stay on if the file is not opening properly. 
    while(true){
      error_blink();
    }
  }
  delay(100);
  digitalWrite(LED, LOW);
}

void error_blink(){
  for(int i=0; i<5; i++){
    digitalWrite(LED, 1);
    delay(50);
    digitalWrite(LED, 0);
    delay(50);
  }
  delay(2000);
}
