// Simple test of USB Host Mouse/Keyboard
//
// This example is in the public domain

#include "USBHost_t36.h"

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
KeyboardController keyboard1(myusb);
KeyboardController keyboard2(myusb);
MouseController mouse1(myusb);

void setup()
{
	while (!Serial) ; // wait for Arduino Serial Monitor
	Serial.println("USB Host Testing");
	myusb.begin();
	keyboard1.attachPress(OnPress);
	keyboard2.attachPress(OnPress);
}


void loop()
{
	myusb.Task();
  if(mouse1.available()) {
    Serial.print("buttons = ");
    Serial.print(mouse1.getButtons(),DEC);
    Serial.print(",  wheel = ");
    Serial.print(mouse1.getWheel(),DEC);
    Serial.print(",  mouseX = ");
    Serial.print(mouse1.getMouseX(),DEC);
    Serial.print(",  mouseY = ");
    Serial.println(mouse1.getMouseY(),DEC);
    mouse1.mouseDataClear();
  }
  delay(50);
}


void OnPress(int key)
{
	Serial.print("key '");
	Serial.print((char)key);
	Serial.print("'  ");
	Serial.println(key);
	//Serial.print("key ");
	//Serial.print((char)keyboard1.getKey());
	//Serial.print("  ");
	//Serial.print((char)keyboard2.getKey());
	//Serial.println();
}

