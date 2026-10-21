#include <SPI.h>
#include <RF24.h>
#include <TimeLib.h>

// HW-237 / nRF24L01 connections
const byte rfCE = 6;
const byte rfCSN = 7;
RF24 radio(rfCE, rfCSN);
const byte radioAddress[6] = "00001";

const uint16_t INVALID_VALUE = 65535;

// This must be identical to the transmitter's structure.
// Packet size: 10 bytes.
struct __attribute__((packed)) TelemetryPacket {
  uint16_t sampleNumber;
  uint32_t unixTime;
  uint16_t avgDist_mm;
  uint16_t busVoltage_mV;
};

bool setupRadio() {
  if (!radio.begin()) {
    return false;
  }

  radio.openReadingPipe(1, radioAddress);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);
  radio.setPayloadSize(sizeof(TelemetryPacket));
  radio.setAutoAck(true);
  radio.startListening();
  return true;
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

void displayReceivedPacket(const TelemetryPacket &packet) {
  Serial.println("----------------------");
  Serial.print("Packet received. Sample: ");
  Serial.println(packet.sampleNumber);

  Serial.print("Transmitted time: ");
  printTime((time_t)packet.unixTime);

  Serial.print("Average distance: ");
  if (packet.avgDist_mm == INVALID_VALUE) {
    Serial.println("invalid");
  } else {
    Serial.print(packet.avgDist_mm / 10.0, 1);
    Serial.println(" cm");
  }

  Serial.print("Bus voltage: ");
  if (packet.busVoltage_mV == INVALID_VALUE) {
    Serial.println("invalid");
  } else {
    Serial.print(packet.busVoltage_mV);
    Serial.println(" mV");
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Starting receiver...");

  if (!setupRadio()) {
    Serial.println("NRF24 not found!");
    while (true) {
    }
  }

  Serial.println("Receiver ready and listening.");
}

void loop() {
  if (radio.available()) {
    TelemetryPacket receivedPacket;

    // If several packets arrived, keep the newest one.
    while (radio.available()) {
      radio.read(&receivedPacket, sizeof(receivedPacket));
    }

    displayReceivedPacket(receivedPacket);
  }
}
