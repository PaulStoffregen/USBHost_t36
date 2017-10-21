#include <USBHost_t36.h>

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
AntPlus ant1(myusb);

void setup() {
  while (!Serial) ; // wait for Arduino Serial Monitor
  Serial.println("Ant+ USB Test");
  myusb.begin();
  ant1.begin();
  ant1.setWheelCircumference(2.112); // wheel circumference, in meters
  ant1.onStatusChange(handleStatusChange);
  ant1.onDeviceID(handleDeviceID);
  ant1.onHeartRateMonitor(handleHeartRateMonitor);
  ant1.onSpeedCadence(handleSpeedCadence);
}

void loop() {
  myusb.Task();
}

void handleHeartRateMonitor(int beatsPerMinute, int milliseconds, int sequenceNumber) {
  Serial.print("HRM: sequence:");
  Serial.print(sequenceNumber);
  Serial.print(", interval:");
  Serial.print(milliseconds);
  Serial.print("ms, bpm:");
  Serial.println(beatsPerMinute);
}

void handleSpeedCadence(float speed, float distance, float rotationPerMinute) {
  Serial.print("SPDCAD: speed: ");
  Serial.print(speed);
  Serial.print(" km/h, cadence: ");
  Serial.print(rotationPerMinute);
  Serial.print("rpm, total distance: ");
  Serial.print(distance);
  Serial.println("km");
}

void handleStatusChange(int channel, int status) {
  Serial.print("Channel ");
  Serial.print(channel);
  Serial.print(" status: ");
  switch (status) {
    case 0: Serial.println("STATUS UNASSIGNED CHANNEL"); break;
    case 1: Serial.println("STATUS ASSIGNED CHANNEL"); break;
    case 2: Serial.println("STATUS SEARCHING CHANNEL"); break;
    case 3: Serial.println("STATUS TRACKING_CHANNEL"); break;
    default: Serial.println("UNKNOWN STATUS STATE");
  }
}

void handleDeviceID(int channel, int devId, int devType, int transType) {
  Serial.print("Device found on channel ");
  Serial.print(channel);
  Serial.print(": deviceId:");
  Serial.print(devId);
  Serial.print(", deviceType:");
  Serial.print(devType);
  Serial.print(", transType:");
  Serial.println(transType);
}
