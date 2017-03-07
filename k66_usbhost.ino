/* USB EHCI Host for Teensy 3.6
 * Copyright 2017 Paul Stoffregen (paul@pjrc.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

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
	// Test board has a USB data mux (this won't be on final Teensy 3.6)
	pinMode(32, OUTPUT);	// pin 32 = USB switch, high=connect device
	digitalWrite(32, LOW);
	pinMode(30, OUTPUT);	// pin 30 = debug info - use oscilloscope
	digitalWrite(30, LOW);

	while (!Serial) ; // wait for Arduino Serial Monitor
	Serial.println("USB Host Testing");

	myusb.begin();

	delay(25);
	Serial.println("Plug in device...");
	digitalWrite(32, HIGH); // connect device

#if 0
	delay(5000);
	Serial.println();
	Serial.println("Ring Doorbell");
	USBHS_USBCMD |= USBHS_USBCMD_IAA;
	if (rootdev) print(rootdev->control_pipe);
#endif

	midi1.setHandleNoteOff(OnNoteOff);
	midi1.setHandleNoteOn(OnNoteOn);
	midi1.setHandleControlChange(OnControlChange);
}


void loop()
{
	myusb.Task();
	midi1.read();
}


void pulse(int usec)
{
	// connect oscilloscope to see these pulses....
	digitalWriteFast(30, HIGH);
	delayMicroseconds(usec);
	digitalWriteFast(30, LOW);
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
	Serial.print(", velocity=");
	Serial.print(velocity);
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

