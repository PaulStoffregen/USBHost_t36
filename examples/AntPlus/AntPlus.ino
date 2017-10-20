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

}

void loop() {
  myusb.Task();
}
