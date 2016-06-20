#include <SPI.h>
#include <RFM69.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <settings.h>

#define PIR_PIN     2
#define METER_PIN   3
#define ONEWIRE_PIN 5

#define REPORTING_DELAY 5 * 60 * 1000 // 5 minutes
#define NUMRETRIES 6

// Global data.
int errorCounter;
int packetsSent;

// Auxiliary libraries.
OneWire bus(ONEWIRE_PIN);
DallasTemperature sensors(&bus);
RFM69 radio;

float readTemperature() {
  sensors.begin();
  int tries = 0;
  while (tries < 10) {
    if (sensors.getDeviceCount() > 0) {
      sensors.setResolution(12);
      sensors.requestTemperatures();
      return sensors.getTempCByIndex(0);
    } else {
      tries++;
      sensors.begin();
    }
  }
  return -1000;
}

// Event handlers and associated data.
volatile int meterPulses;
void handleMeterPulse() {
  // TODO: Ensure we only count pulses that are N msec long.
  meterPulses++;
}

volatile int pirPulses;
void handlePirPulse() {
  pirPulses++;
}
// End event handlers

void ioSetup() {
  pinMode(METER_PIN, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(METER_PIN), handleMeterPulse, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), handlePirPulse, FALLING);
}

void radioSetup() {
  radio.initialize(RF69_868MHZ, NODEID, NETWORKID);
  radio.encrypt(ENCRYPTKEY);
}

String gatherData() {
  String stream;
  StaticJsonBuffer<200> buffer;
  JsonObject& root = buffer.createObject();

  float temp = readTemperature();
  if (temp > -1000) {
    root["temperature"] = temp;
  }

  root["movementCounter"] = pirPulses;
  root["energyCounter"] = meterPulses;
  root["errorCounter"] = errorCounter;
  root["packetsSent"] = packetsSent;
  root["rssi"] = radio.RSSI;
  root["uptime"] = millis();
  root.printTo(stream);
  return stream;
}

void report() {
  String data = gatherData();
  if (radio.sendWithRetry(GATEWAYID, data.c_str(), data.length(), NUMRETRIES)) {
    packetsSent++;
  } else {
    errorCounter++;
  }
}

void receive() {
  if (radio.receiveDone()) {
    if (radio.ACKRequested()) radio.sendACK();
  }
}

void setup() {
  Serial.begin(115200);

  radioSetup();
  ioSetup();
}

void loop() {
  receive();
  // TODO: Make this timer-driven?
  report();
  delay(REPORTING_DELAY);
}
