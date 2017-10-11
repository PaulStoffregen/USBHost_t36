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
KeyboardHIDExtrasController hidextras(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);
USBHIDParser hid4(myusb);
USBHIDParser hid5(myusb);
MouseController mouse1(myusb);
JoystickController joystick1(myusb);

void setup()
{
  while (!Serial) ; // wait for Arduino Serial Monitor
  Serial.println("USB Host Testing");
  myusb.begin();
  keyboard1.attachPress(OnPress);
  keyboard2.attachPress(OnPress);
  hidextras.attachPress(OnHIDExtrasPress);
  hidextras.attachRelease(OnHIDExtrasRelease);
}


void loop()
{
  myusb.Task();
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
  switch (key) {
    case KEYD_UP       : Serial.print("UP"); break;
    case KEYD_DOWN    : Serial.print("DN"); break;
    case KEYD_LEFT     : Serial.print("LEFT"); break;
    case KEYD_RIGHT   : Serial.print("RIGHT"); break;
    case KEYD_INSERT   : Serial.print("Ins"); break;
    case KEYD_DELETE   : Serial.print("Del"); break;
    case KEYD_PAGE_UP  : Serial.print("PUP"); break;
    case KEYD_PAGE_DOWN: Serial.print("PDN"); break;
    case KEYD_HOME     : Serial.print("HOME"); break;
    case KEYD_END      : Serial.print("END"); break;
    case KEYD_F1       : Serial.print("F1"); break;
    case KEYD_F2       : Serial.print("F2"); break;
    case KEYD_F3       : Serial.print("F3"); break;
    case KEYD_F4       : Serial.print("F4"); break;
    case KEYD_F5       : Serial.print("F5"); break;
    case KEYD_F6       : Serial.print("F6"); break;
    case KEYD_F7       : Serial.print("F7"); break;
    case KEYD_F8       : Serial.print("F8"); break;
    case KEYD_F9       : Serial.print("F9"); break;
    case KEYD_F10      : Serial.print("F10"); break;
    case KEYD_F11      : Serial.print("F11"); break;
    case KEYD_F12      : Serial.print("F12"); break;
    default: Serial.print((char)key); break;
  }
  Serial.print("'  ");
  Serial.print(key);
  Serial.print(" MOD: ");
  if (keyboard1) {
    Serial.print(keyboard1.getModifiers(), HEX);
    Serial.print(" OEM: ");
    Serial.print(keyboard1.getOemKey(), HEX);
    Serial.print(" LEDS: ");
    Serial.println(keyboard1.LEDS(), HEX);
  } else {
    Serial.print(keyboard2.getModifiers(), HEX);
    Serial.print(" OEM: ");
    Serial.print(keyboard2.getOemKey(), HEX);
    Serial.print(" LEDS: ");
    Serial.println(keyboard2.LEDS(), HEX);
  }

  //Serial.print("key ");
  //Serial.print((char)keyboard1.getKey());
  //Serial.print("  ");
  //Serial.print((char)keyboard2.getKey());
  //Serial.println();
}
void OnHIDExtrasPress(uint32_t top, uint16_t key) 
{
  Serial.print("HID (");
  Serial.print(top, HEX);
  Serial.print(") key press:");
  Serial.println(key, HEX);
}

void OnHIDExtrasRelease(uint32_t top, uint16_t key) 
{
  Serial.print("HID (");
  Serial.print(top, HEX);
  Serial.print(") key release:");
  Serial.println(key, HEX);
}
