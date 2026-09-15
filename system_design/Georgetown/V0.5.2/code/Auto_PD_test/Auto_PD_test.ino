#include <Wire.h>
#include <DS3232RTC.h>
DS3232RTC myRTC;

constexpr time_t alarmInterval{10}; //  wake-up interval in seconds
const int powerLatchPin = 4;           // HIGH = Cut Power / LOW = Keep Power On

void setup() {
  // STEP 1: IMMEDIATELY HOLD THE POWER ON (Drive it LOW)
  
  Serial.begin(9600);
  myRTC.begin();

  // STEP 2: DO YOUR WORK
  Serial.println("Arduino Awake and stable!");
  time_t t = myRTC.get();
  
  // (Put your sensor readings / data logging here)
  delay(1000); 

  // STEP 3: SCHEDULE THE NEXT ALARM
  time_t nextAlarm = t + alarmInterval;
  myRTC.setAlarm(DS3232RTC::ALM1_MATCH_HOURS, second(nextAlarm), minute(nextAlarm), hour(nextAlarm), 0);

  // STEP 4: RESET RTC FOR BATTERY OPERATION
  myRTC.alarm(DS3232RTC::ALARM_1);               // Clear current alarm flag
  delay(100);
  myRTC.alarmInterrupt(DS3232RTC::ALARM_1, true); // Re-enable interrupt pin
  myRTC.writeRTC(0x0E, 0x45);                     // Set BBSQW = 1 so alarm works on battery
  delay(100);
  // STEP 5: CUT POWER (Drive it HIGH)
  Serial.println("Triggering cutoff...");

  delay(50);      // Give hardware a moment to stabilize

  pinMode(powerLatchPin, OUTPUT);
  digitalWrite(powerLatchPin, HIGH); // Pushing this HIGH actively cuts the power module
  

}

void loop() {
  // Empty - handled in setup
}
