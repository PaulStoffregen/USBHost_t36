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



hidclaim_t MouseController::claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage)
{
	// only claim Desktop/Mouse
	if (topusage != 0x10002) return CLAIM_NO;
	// only claim from one physical device
	if (mydevice != NULL && dev != mydevice) return CLAIM_NO;
	mydevice = dev;
	collections_claimed++;
	return CLAIM_REPORT;
}

void MouseController::disconnect_collection(Device_t *dev)
{
	if (--collections_claimed == 0) {
		mydevice = NULL;
	}
}

void MouseController::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax)
{
	// TODO: check if absolute coordinates
	hid_input_begin_ = true;
}

void MouseController::hid_input_data(uint32_t usage, int32_t value)
{
	//Serial.printf("Mouse: usage=%X, value=%d\n", usage, value);
	uint32_t usage_page = usage >> 16;
	usage &= 0xFFFF;
	if (usage_page == 9 && usage >= 1 && usage <= 8) {
		if (value == 0) {
			buttons &= ~(1 << (usage -1));
		} else {
			buttons |= (1 << (usage -1));
		}
	} else if (usage_page == 1) {
		switch (usage) {
		  case 0x30:
			mouseX = value;
			break;
		  case 0x31:
			mouseY = value;
			break;
		  case 0x32: // Apple uses this for horizontal scroll
			wheelH = value;
			break;
		  case 0x38:
			wheel = value;
			break;
		}
	} else if (usage_page == 12) {
		if (usage == 0x238) { // Microsoft uses this for horizontal scroll
			wheelH = value;
		}
	}
}

void MouseController::hid_input_end()
{
	if (hid_input_begin_) {
		mouseEvent = true;
		hid_input_begin_ = false;
	}
}

void MouseController::mouseDataClear() {
	mouseEvent = false;
	buttons = 0;
	mouseX  = 0;
	mouseY  = 0;
	wheel   = 0;
	wheelH  = 0;
}


