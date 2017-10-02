// Simple test of USB Host Mouse/Keyboard
//
// This example is in the public domain

#include "USBHost_t36.h"
#include <EventResponder.h>

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

EventResponder event_responder;  // 

uint8_t keyboard_leds = 0;


void setup()
{
  while (!Serial) ; // wait for Arduino Serial Monitor
  Serial.println("USB Host Testing");
  myusb.begin();
  keyboard1.attachPress(OnPress);
  keyboard2.attachPress(OnPress);

 // event_responder.setContext(this);  // Set the contxt to us
  event_responder.attach(&query_device_data_eventResponder); 

}

uint8_t query_buffer[256];

uint16_t query_state = 0;
uint16_t query_needed = 0;


void query_device_data_eventResponder(EventResponderRef event_responder) {
  if (query_state) {
    switch (query_state) {
      case 0x1: Serial.print("Keyboard 1 Manufacturer: "); break;
      case 0x2: Serial.print("Keyboard 1 Product: "); break;
      case 0x4: Serial.print("Keyboard 1 serial: "); break;
      case 0x10: Serial.print("Keyboard 2 Manufacturer: "); break;
      case 0x20: Serial.print("Keyboard 2 Product: "); break;
      case 0x40: Serial.print("Keyboard 2 serial: "); break;
      case 0x100: Serial.print("Mouse Manufacturer: "); break;
      case 0x200: Serial.print("Mouse Product: "); break;
      case 0x400: Serial.print("Mouse serial: "); break;
      case 0x1000: Serial.print("Joystick Manufacturer: "); break;
      case 0x2000: Serial.print("Joystick Product: "); break;
      case 0x4000: Serial.print("Joystick serial: "); break;
    }
    Serial.println((char*)query_buffer);
    query_needed &= ~query_state;
  }

  if (query_needed & 0x1) {
    if (keyboard1.manufacturer(query_buffer, sizeof(query_buffer),event_responder)) {
      query_state = 0x1;
      return;
    } else {
      Serial.println("Keyboard 1 no Manufacturer string");
      query_needed &= ~0x1;
    }
  }
  if (query_needed & 0x2) {
    if (keyboard1.product(query_buffer, sizeof(query_buffer),event_responder)) {
      query_state = 0x2;
      return;
    } else {
      Serial.println("Keyboard 1 no Product string");
      query_needed &= ~0x2;
    }
  }
  if (query_needed & 0x4) {
    if (keyboard1.serialNumber(query_buffer, sizeof(query_buffer),event_responder)) {
      query_state = 0x4;
      return;
    } else {
      Serial.println("Keyboard 1 no Serial number string");
      query_needed &= ~0x4;
    }
  }
  if (query_needed & 0x10) {
    if (keyboard2.manufacturer(query_buffer, sizeof(query_buffer),event_responder)) {
      query_state = 0x10;
      return;
    } else {
      Serial.println("Keyboard 2 no Manufacturer string");
      query_needed &= ~0x10;
    }
  }
  if (query_needed & 0x20) {
    if (keyboard2.product(query_buffer, sizeof(query_buffer),event_responder)) {
      query_state = 0x20;
      return;
    } else {
      Serial.println("Keyboard 2 no Product string");
      query_needed &= ~0x20;
    }
  }
  if (query_needed & 0x40) {
    if (keyboard2.serialNumber(query_buffer, sizeof(query_buffer),event_responder)) {
      query_state = 0x40;
      return;
    } else {
      Serial.println("Keyboard 2 no Serial number string");
      query_needed &= ~0x40;
    }
  }

  if (query_needed & 0x100) {
    if (mouse1.manufacturer(query_buffer, sizeof(query_buffer),event_responder)) {
      query_state = 0x100;
      return;
    } else {
      Serial.println("Mouse no Manufacturer string");
      query_needed &= ~0x100;
    }
  }
  if (query_needed & 0x200) {
    if (mouse1.product(query_buffer, sizeof(query_buffer),event_responder)) {
      query_state = 0x200;
      return;
    } else {
      Serial.println("Mouse no Product string");
      query_needed &= ~0x200;
    }
  }
  if (query_needed & 0x400) {
    if (mouse1.serialNumber(query_buffer, sizeof(query_buffer),event_responder)) {
      query_state = 0x400;
      return;
    } else {
      Serial.println("Mouse no Serial number string");
      query_needed &= ~0x400;
    }
  }

  if (query_needed & 0x1000) {
    if (joystick1.manufacturer(query_buffer, sizeof(query_buffer),event_responder)) {
      query_state = 0x1000;
      return;
    } else {
      Serial.println("Joystick no Product string");
      query_needed &= ~0x1000;
    }
  }
  if (query_needed & 0x2000) {
    if (joystick1.product(query_buffer, sizeof(query_buffer),event_responder)) {
      query_state = 0x2000;
      return;
    } else {
      Serial.println("Joystick no Product string");
      query_needed &= ~0x2000;
    }
  }
  if (query_needed & 0x4000) {
    if (joystick1.serialNumber(query_buffer, sizeof(query_buffer),event_responder)) {
      query_state = 0x4000;
      return;
    } else {
      Serial.println("Joystick no Serial number string");
      query_needed &= ~0x4000;
    }
  }
  query_needed = 0; // if we got here... clear it out
  query_state = 0;
}



void loop()
{
  myusb.Task();
  bool any_new_devices = false;
  bool keyboard1_connected_now = keyboard1.connected();
  if (keyboard1_connected != keyboard1_connected_now) {
    keyboard1_connected = keyboard1_connected_now;
    if (keyboard1_connected) {
      Serial.printf("\n\n*** Keyboard 1 %04x:%04x Connected ***\n", keyboard1.idVendor(), keyboard1.idProduct());
      query_needed |= 0x7;
      any_new_devices = true;
    } else {
      Serial.println("\n*** Keyboard 1 disconnected ***");
    }
  }
  bool keyboard2_connected_now = keyboard2.connected();
  if (keyboard2_connected != keyboard2_connected_now) {
    keyboard2_connected = keyboard2_connected_now;
    if (keyboard2_connected) {
      Serial.printf("\n\n*** Keyboard 2 %04x:%04x Connected ***\n", keyboard2.idVendor(), keyboard2.idProduct());
      query_needed |= 0x70;
      any_new_devices = true;
    } else {
      Serial.println("\n*** Keyboard 2 disconnected ***");
    }
  }

  
  bool mouse_connected_now = mouse1.connected();
  if (mouse_connected != mouse_connected_now) {
    mouse_connected = mouse_connected_now;
    if (mouse_connected) {
      Serial.printf("\n\n*** Mouse %04x:%04x Connected ***\n", mouse1.idVendor(), mouse1.idProduct());
      query_needed |= 0x700;
      any_new_devices = true;
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
      query_needed |= 0x7000;
      any_new_devices = true;
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

  if (any_new_devices && (query_state == 0)) {
      event_responder.triggerEvent(); // make sure it gets called;
  }

  if (Serial.available()) {
    keyboard_leds = Serial.read(); 
    if ((keyboard_leds >= '0') && (keyboard_leds <= '9')) {
      keyboard_leds -= '0';
      // Test to see if we can play with LEDS on Keyboard...
      Serial.printf("Set Keyboard Leds to %x\n", keyboard_leds);
      if (keyboard1_connected) keyboard1.setLEDS(keyboard_leds); 
      if (keyboard2_connected) keyboard2.setLEDS(keyboard_leds); 
    }

  }

}


void OnPress(int key)
{
  Serial.print("key '");
  Serial.print((char)key);
  Serial.print("'  ");
  Serial.print(key);
  Serial.print(" MOD: ");
  if (keyboard1_connected) {
    Serial.print(keyboard1.getModifiers(), HEX);
    Serial.print(" OEM: ");
    Serial.println(keyboard1.getOemKey(), HEX);
  } else {
    Serial.print(keyboard2.getModifiers(), HEX);
    Serial.print(" OEM: ");
    Serial.println(keyboard2.getOemKey(), HEX);
  }

  //Serial.print("key ");
  //Serial.print((char)keyboard1.getKey());
  //Serial.print("  ");
  //Serial.print((char)keyboard2.getKey());
  //Serial.println();
}
