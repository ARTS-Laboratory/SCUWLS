#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <HCSR04.h>
#include <DS3232RTC.h>
#include <TimeLib.h>
DS3232RTC myRTC;

// HC-SR04 config
const byte trigPin = 9;
const byte echoPin = 8;
UltraSonicDistanceSensor distanceSensor(trigPin, echoPin);

// nRF24 config
RF24 radio(6, 7);
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
  myRTC.begin();
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
}
