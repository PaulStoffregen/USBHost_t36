#include <USBHost_t36.h>

USBHost myusb;
USBSerial userial(myusb);

elapsedMillis msec;
bool led_is_on;

void setup() {
  myusb.begin();
  msec = 0;
  led_is_on = false;
}

void loop() {
  myusb.Task();
  if (userial && msec > 800) {
    if (led_is_on) {
      userial.setDTR(1);
      led_is_on = false;
    } else {
      userial.setDTR(0);
      led_is_on = true;
    }
    msec = 0;
  }
}
