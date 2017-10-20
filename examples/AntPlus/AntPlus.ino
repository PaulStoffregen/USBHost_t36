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
  ant1.onStatusChange(handleStatusChange);
  ant1.onDeviceID(handleDeviceID);
}

void loop() {
  myusb.Task();
}

void handleStatusChange(int channel, int status) {
  Serial.print("Channel ");
  Serial.print(channel);
  Serial.print(" status: ");
  switch (status) {
    case 0: Serial.println("STATUS UNASSIGNED CHANNEL"); break;
    case 2: Serial.println("STATUS ASSIGNED CHANNEL"); break;
    case 3: Serial.println("STATUS SEARCHING CHANNEL"); break;
    case 4: Serial.println("STATUS TRACKING_CHANNEL"); break;
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
