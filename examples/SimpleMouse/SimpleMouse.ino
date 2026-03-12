// Simple test of USB Host Mouse
//
// This example is in the public domain

#include "USBHost_t36.h"

USBHost myusb;

USBHIDParser hid1(myusb);
MouseController mouse1(myusb);

void setup()
{
  while (!Serial) ; // wait for Arduino Serial Monitor
  Serial.println("\n\nUSB Host Testing");
  myusb.begin();
}

void loop()
{
  myusb.Task();

  if (mouse1.available()) {
    Serial.print("Mouse: buttons = ");
    Serial.print(mouse1.getButtons());
    Serial.print(",  mouseX = ");
    Serial.print(mouse1.getMouseX());
    Serial.print(",  mouseY = ");
    Serial.print(mouse1.getMouseY());
    Serial.print(",  wheel = ");
    Serial.print(mouse1.getWheel());
    Serial.print(",  wheelH = ");
    Serial.print(mouse1.getWheelH());
    Serial.println();
    mouse1.mouseDataClear();
  }
}