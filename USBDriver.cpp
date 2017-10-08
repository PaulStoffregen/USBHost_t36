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

bool USBDriver::manufacturer(uint8_t *buf, uint16_t length, EventResponderRef  event_responder) 
{
	// Only allow this if we have a device and it has a string ID 
	// and we don't have another active query. 
	if (device && device->stringIDManufacturer && (device->_query_event_responder == nullptr)) {
		device->_query_event_responder = &event_responder;
		device->enum_state = 42;	// set to the answer of all questions...
		setup_t setup;	// a setup structure
		mk_setup(setup, 0x80, 6, 0x0300 | device->stringIDManufacturer, device->LanguageID, length);

		queue_Control_Transfer(device, &setup, buf, NULL);
		return true;
	}
	return false;
}

bool USBDriver::product(uint8_t *buf, uint16_t length, EventResponderRef  event_responder)
{
	// Only allow this if we have a device and it has a string ID 
	// and we don't have another active query. 
	if (device && device->stringIDProduct && (device->_query_event_responder == nullptr)) {
		device->_query_event_responder = &event_responder;
		device->enum_state = 42;	// set to the answer of all questions...
		setup_t setup;	// a setup structure
		mk_setup(setup, 0x80, 6, 0x0300 | device->stringIDProduct, device->LanguageID, length);

		queue_Control_Transfer(device, &setup, buf, NULL);
		return true;
	}
	return false;
}

bool USBDriver::serialNumber(uint8_t *buf, uint16_t length, EventResponderRef  event_responder)
{
	// Only allow this if we have a device and it has a string ID 
	// and we don't have another active query. 
	if (device && device->stringIDSerial && (device->_query_event_responder == nullptr)) {
		device->_query_event_responder = &event_responder;
		device->enum_state = 42;	// set to the answer of all questions...
		setup_t setup;	// a setup structure
		mk_setup(setup, 0x80, 6, 0x0300 | device->stringIDSerial, device->LanguageID, length);

		queue_Control_Transfer(device, &setup, buf, NULL);
		return true;
	}
	return false;
}

