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
#include "USBHost.h"

USBHub::USBHub()
{
	// TODO: free Device_t, Pipe_t & Transfer_t we will need
	driver_ready_for_device(this);
}

bool USBHub::claim_device(Device_t *dev, const uint8_t *descriptors)
{
	Serial.print("USBHub claim_device this=");
	Serial.println((uint32_t)this, HEX);

	// check for HUB type
	if (dev->bDeviceClass != 9 || dev->bDeviceSubClass != 0) return false;
	// protocol must be 0=FS, 1=HS Single-TT, or 2=HS Multi-TT
	if (dev->bDeviceProtocol > 2) return false;
	// check for endpoint descriptor
	if (descriptors[9] != 7 || descriptors[10] != 5) return false;
	// endpoint must be IN direction
	if ((descriptors[11] & 0xF0) != 0x80) return false;
	// endpoint type must be interrupt
	if (descriptors[12] != 3) return false;
	// get the endpoint number, must not be zero
	uint32_t endpoint = descriptors[11] & 0x0F;
	if (endpoint == 0) return false;
	// get the maximum packet size
	uint32_t maxsize = descriptors[13] | (descriptors[14] << 8);

	Serial.println(descriptors[9]);
	Serial.println(descriptors[10]);
	Serial.println(descriptors[11], HEX);
	Serial.println(maxsize);
	// bDeviceProtocol = 0 is full speed
	// bDeviceProtocol = 1 is high speed single TT
	// bDeviceProtocol = 2 is high speed multiple TT

	Serial.print("bDeviceClass = ");
	Serial.println(dev->bDeviceClass);
	Serial.print("bDeviceSubClass = ");
	Serial.println(dev->bDeviceSubClass);
	Serial.print("bDeviceProtocol = ");
	Serial.println(dev->bDeviceProtocol);

	mk_setup(dev->setup, 0xA0, 6, 0x2900, 0, sizeof(hub_desc));
	new_Transfer(dev->control_pipe, hub_desc, sizeof(hub_desc));
	// TODO: need to arrange for callback to this driver from enumeration.cpp

	return true;
}

bool USBHub::control_callback(const Transfer_t *transfer)
{
	Serial.println("USBHub control callback");
	print_hexbytes(transfer->buffer, transfer->length);

	return true;
}


/*
config descriptor from a Multi-TT hub
09 02 29 00 01 01 00 E0 32
09 04 00 00 01 09 00 01 00
07 05 81 03 01 00 0C
09 04 00 01 01 09 00 02 00
07 05 81 03 01 00 0C
*/


