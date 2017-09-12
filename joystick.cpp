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

#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info



bool JoystickController::claim_collection(Device_t *dev, uint32_t topusage)
{
	// only claim Desktop/Joystick and Desktop/Gamepad
	if (topusage != 0x10004 && topusage != 0x10005) return false;
	// only claim from one physical device
	if (mydevice != NULL && dev != mydevice) return false;
	mydevice = dev;
	collections_claimed++;
	anychange = true; // always report values on first read
	return true;
}

void JoystickController::disconnect_collection(Device_t *dev)
{
	if (--collections_claimed == 0) {
		mydevice = NULL;
	}
}

void JoystickController::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax)
{
	// TODO: set up translation from logical min/max to consistent 16 bit scale
}

void JoystickController::hid_input_data(uint32_t usage, int32_t value)
{
	//Serial.printf("Joystick: usage=%X, value=%d\n", usage, value);
	uint32_t usage_page = usage >> 16;
	usage &= 0xFFFF;
	if (usage_page == 9 && usage >= 1 && usage <= 32) {
		uint32_t bit = 1 << (usage -1);
		if (value == 0) {
			if (buttons & bit) {
				buttons &= ~bit;
				anychange = true;
			}
		} else {
			if (!(buttons & bit)) {
				buttons |= bit;
				anychange = true;
			}
		}
	} else if (usage_page == 1 && usage >= 0x30 && usage <= 0x39) {
		// TODO: need scaling of value to consistent API, 16 bit signed?
		// TODO: many joysticks repeat slider usage.  Detect & map to axes?
		uint32_t i = usage - 0x30;
		if (axis[i] != value) {
			axis[i] = value;
			anychange = true;
		}
	}
	// TODO: hat switch?
}

void JoystickController::hid_input_end()
{
	if (anychange) {
		joystickEvent = true;
	}
}

void JoystickController::joystickDataClear() {
	joystickEvent = false;
	anychange = false;
}


