// Simple test of USB Host
//
// This example is in the public domain

#include "USBHost_t36.h"

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
KeyboardController keyboard1(myusb);
KeyboardController keyboard2(myusb);
MIDIDevice midi1(myusb);

void setup()
{
	while (!Serial) ; // wait for Arduino Serial Monitor
	Serial.println("USB Host Testing");
	myusb.begin();
	keyboard1.attachPress(OnPress);
	keyboard1.attachRawPress(OnRawPress);
	keyboard1.attachRawRelease(OnRawRelease);
	keyboard2.attachPress(OnPress);
	midi1.setHandleNoteOff(OnNoteOff);
	midi1.setHandleNoteOn(OnNoteOn);
	midi1.setHandleControlChange(OnControlChange);
}


void loop()
{
	myusb.Task();
	midi1.read();
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

void OnRawPress(uint8_t keycode)
{
	Serial.print("raw key press: ");
	Serial.println((int)keycode);
}

void OnRawRelease(uint8_t keycode)
{
	Serial.print("raw key release: ");
	Serial.println((int)keycode);
}

void OnNoteOn(byte channel, byte note, byte velocity)
{
	Serial.print("Note On, ch=");
	Serial.print(channel);
	Serial.print(", note=");
	Serial.print(note);
	Serial.print(", velocity=");
	Serial.print(velocity);
	Serial.println();
}

void OnNoteOff(byte channel, byte note, byte velocity)
{
	Serial.print("Note Off, ch=");
	Serial.print(channel);
	Serial.print(", note=");
	Serial.print(note);
	//Serial.print(", velocity=");
	//Serial.print(velocity);
	Serial.println();
}

void OnControlChange(byte channel, byte control, byte value)
{
	Serial.print("Control Change, ch=");
	Serial.print(channel);
	Serial.print(", control=");
	Serial.print(control);
	Serial.print(", value=");
	Serial.print(value);
	Serial.println();
}

