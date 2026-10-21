#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <HCSR04.h>
#include <DS3232RTC.h>
#include <TimeLib.h>

// DS3232 RTC
DS3232RTC myRTC;

// HC-SR04 ultrasonic sensor
const byte trigPin = 9;
const byte echoPin = 8;
UltraSonicDistanceSensor distanceSensor(trigPin, echoPin);

// HW-237 / nRF24L01
RF24 radio(6, 7);
const byte radioAddress[6] = "00001";

const byte NUMBER_OF_READINGS = 5;
const uint16_t INVALID_DISTANCE = 65535;
uint16_t sampleNumber = 0;

// Set this to true for ONE upload to set the RTC to the sketch compile time.
// Then change it to false and upload again, so future resets keep correct time.
const bool SET_RTC_FROM_COMPILE_TIME = true;

// The receiver must use this exact same structure.
// Packet size: 8 bytes.
struct __attribute__((packed)) TelemetryPacket {
  uint16_t sampleNumber;
  uint32_t unixTime;
  uint16_t avgDist_mm;
  uint16_t busVoltage_mV;
};

uint16_t cmToMm(float distanceCm) {
  // Reject invalid, out-of-range, and NaN HC-SR04 readings.
  if (!(distanceCm >= 2.0 && distanceCm <= 400.0)) {
    return INVALID_DISTANCE;
  }

  return (uint16_t)(distanceCm * 10.0 + 0.5);
}

float readAverageDistance() {
  float total = 0;
  byte validReadings = 0;

  for (byte i = 0; i < NUMBER_OF_READINGS; i++) {
    float distanceCm = distanceSensor.measureDistanceCm();

    if (distanceCm >= 2.0 && distanceCm <= 400.0) {
      total += distanceCm;
      validReadings++;
    }

    // Wait for ultrasonic echoes to decay before the next trigger.
    delay(60);
  }

  return (validReadings > 0) ? total / validReadings : -1.0;
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
  packet.avgDist_mm = cmToMm(avgDist);
  packet.busVoltage_mV = (uint16_t)busvoltage;

  // True means the receiver acknowledged this packet.
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

// __DATE__ and __TIME__ are filled in by the Arduino compiler at upload time.
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

  myRTC.begin();

  if (SET_RTC_FROM_COMPILE_TIME) {
    time_t newTime = compiledSketchTime();
    myRTC.set(newTime);
    Serial.print("RTC set to compile time: ");
    printTime(newTime);
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
  float busvoltage = 5.000;
  
  bool success = sendTelemetry(t, avgDist, busvoltage);
  

  if (success) {
    Serial.println("Wireless data sent.");
  } else {
    Serial.println("Wireless transmission failed.");
  }

  Serial.print("Time: ");
  printTime(t);

  Serial.print("Average distance: ");
  if (avgDist < 0) {
    Serial.println("No valid HC-SR04 reading");
  } else {
    Serial.print(avgDist, 1);
    Serial.println(" cm");
  }
  Serial.println("Bus Voltage:");
  Serial.print(busvoltage, 1);

  Serial.println("----------------------");
  delay(5000);
}
