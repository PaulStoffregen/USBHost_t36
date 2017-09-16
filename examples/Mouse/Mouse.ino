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
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);
USBHIDParser hid4(myusb);
USBHIDParser hid5(myusb);
MouseController mouse1(myusb);
JoystickController joystick1(myusb);
bool keyboard1_connected = false;
bool keyboard2_connected = false;
bool mouse_connected = false;
bool joystick_connected = false;

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
  bool keyboard1_connected_now = keyboard1.connected();
  if (keyboard1_connected != keyboard1_connected_now) {
    keyboard1_connected = keyboard1_connected_now;
    if (keyboard1_connected) {
      Serial.printf("\n\n*** Keyboard 1 %04x:%04x Connected ***\n", keyboard1.idVendor(), keyboard1.idProduct());
    } else {
      Serial.println("\n*** Keyboard 1 disconnected ***");
    }
  }
  bool keyboard2_connected_now = keyboard2.connected();
  if (keyboard2_connected != keyboard2_connected_now) {
    keyboard2_connected = keyboard2_connected_now;
    if (keyboard2_connected) {
      Serial.printf("\n\n*** Keyboard 2 %04x:%04x Connected ***\n", keyboard2.idVendor(), keyboard2.idProduct());
    } else {
      Serial.println("\n*** Keyboard 2 disconnected ***");
    }
  }

  
  bool mouse_connected_now = mouse1.connected();
  if (mouse_connected != mouse_connected_now) {
    mouse_connected = mouse_connected_now;
    if (mouse_connected) {
      Serial.printf("\n\n*** Mouse %04x:%04x Connected ***\n", mouse1.idVendor(), mouse1.idProduct());
    } else {
      Serial.println("\n*** Mouse disconnected ***");
    }
  }

  
  if(mouse1.available()) {
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
 
  bool joystick_connected_now = joystick1.connected();
  if (joystick_connected != joystick_connected_now) {
    joystick_connected = joystick_connected_now;
    if (joystick_connected) {
      Serial.printf("\n\n*** Joystick %04x:%04x Connected ***\n", joystick1.idVendor(), joystick1.idProduct());
    } else {
      Serial.println("\n*** Joystick disconnected ***");
    }
  }
  
  if (joystick1.available()) {
    Serial.print("Joystick: buttons = ");
    Serial.print(joystick1.getButtons(), HEX);
    Serial.print(", X = ");
    Serial.print(joystick1.getAxis(0));
    Serial.print(", Y = ");
    Serial.print(joystick1.getAxis(1));
    Serial.print(", Z = ");
    Serial.print(joystick1.getAxis(2));
    Serial.print(", Rz = ");
    Serial.print(joystick1.getAxis(5));
    Serial.print(", Rx = ");
    Serial.print(joystick1.getAxis(3));
    Serial.print(", Ry = ");
    Serial.print(joystick1.getAxis(4));
    Serial.print(", Hat = ");
    Serial.print(joystick1.getAxis(9));
    Serial.println();
    joystick1.joystickDataClear();
  }
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