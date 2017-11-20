/* USB keyboard power Host for Teensy 3.6
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

#define TOPUSAGE_SYS_CONTROL 	0x10080
#define TOPUSAGE_CONSUMER_CONTROL	0x0c0001

hidclaim_t KeyboardHIDExtrasController::claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage)
{
	// Lets try to claim a few specific Keyboard related collection/reports
	//Serial.printf("KBH Claim %x\n", topusage);
	if ((topusage != TOPUSAGE_SYS_CONTROL) 
		&& (topusage != TOPUSAGE_CONSUMER_CONTROL)
		) return CLAIM_NO;
	// only claim from one physical device
	//Serial.println("KeyboardHIDExtrasController claim collection");
	if (mydevice != NULL && dev != mydevice) return CLAIM_NO;
	mydevice = dev;
	collections_claimed_++;
	return CLAIM_REPORT;
}

void KeyboardHIDExtrasController::disconnect_collection(Device_t *dev)
{
	if (--collections_claimed_ == 0) {
		mydevice = NULL;
	}
}

void KeyboardHIDExtrasController::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax)
{
	//Serial.printf("KPC:hid_input_begin TUSE: %x TYPE: %x Range:%x %x\n", topusage, type, lgmin, lgmax);
	topusage_ = topusage;	// remember which report we are processing. 
	hid_input_begin_ = true;
	hid_input_data_ = false;
}

void KeyboardHIDExtrasController::hid_input_data(uint32_t usage, int32_t value)
{
	// Hack ignore 0xff00 high words as these are user values... 
	if ((usage & 0xffff0000) == 0xff000000) return; 
	//Serial.printf("KeyboardHIDExtrasController: topusage= %x usage=%X, value=%d\n", topusage_, usage, value);

	// See if the value is in our keys_down list
	usage &= 0xffff;		// only keep the actual key
	if (usage == 0) return;	// lets not process 0, if only 0 happens, we will handle it on the end to remove existing pressed items.

	// Remember if we have received any logical key up events.  Some keyboard appear to send them
	// others do no...
	hid_input_data_ = true;

	uint8_t key_index;
	for (key_index = 0; key_index < count_keys_down_; key_index++) {
		if (keys_down[key_index] == usage) {
			if (value) return;		// still down

			if (keyReleasedFunction) {
				keyReleasedFunction(topusage_, usage);
			}

			// Remove from list
			count_keys_down_--;
			for (;key_index < count_keys_down_; key_index++) {
				keys_down[key_index] = keys_down[key_index+1];
			}
			return;
		}
	}
	// Was not in list
	if (!value) return;	// still 0
	if (keyPressedFunction) {
		keyPressedFunction(topusage_, usage);
	}
	if (count_keys_down_ < MAX_KEYS_DOWN) {
		keys_down[count_keys_down_++] = usage;
	}
}

void KeyboardHIDExtrasController::hid_input_end()
{
	//Serial.println("KPC:hid_input_end");
	if (hid_input_begin_) {

		// See if we received any data from parser if not, assume all keys released... 
		if (!hid_input_data_ ) {
			if (keyPressedFunction) {
				while (count_keys_down_) {
					count_keys_down_--;
					keyReleasedFunction(topusage_, keys_down[count_keys_down_]);
				}
			}
			count_keys_down_ = 0;
		}

		event_ = true;
		hid_input_begin_ = false;
	}		
}



