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

bool USBHub::claim(Device_t *dev, int type, const uint8_t *descriptors)
{
	// only claim entire device, never at interface level
	if (type != 0) return false;

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
	endpoint = descriptors[11] & 0x0F;
	if (endpoint == 0) return false;
	// get the maximum packet size
	uint32_t maxsize = descriptors[13] | (descriptors[14] << 8);
	if (maxsize == 0) return false;
	if (maxsize > 1) return false; // do hub chips with > 7 ports exist?

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

	changepipe = NULL;
	changebits = 0;
	state = 0;

	// TODO: need a way to do control transfers with our own setup data.
	mk_setup(dev->setup, 0xA0, 6, 0x2900, 0, sizeof(hub_desc));
	new_Control_Transfer(dev, &dev->setup, hub_desc);

	return true;
}

void USBHub::poweron(uint32_t port)
{
	// TODO: need a way to do control transfers with our own setup data.
	mk_setup(device->setup, 0x23, 3, 8, port, 0);
	new_Control_Transfer(device, &device->setup, NULL);
}

bool USBHub::control(const Transfer_t *transfer)
{
	Serial.println("USBHub control callback");
	print_hexbytes(transfer->buffer, transfer->length);

	if (state == 0) {
		// read hub descriptor to learn hub's capabilities
		if (transfer->buffer != hub_desc) return false;
		// Hub Descriptor, USB 2.0, 11.23.2.1 page 417
		if (hub_desc[0] == 9 && hub_desc[1] == 0x29) {
			numports = hub_desc[2];
			characteristics = hub_desc[3];
			powertime = hub_desc[5];
			// TODO: do we need to use the DeviceRemovable
			// bits to mke synthetic device connect events?
			Serial.print("Hub has ");
			Serial.print(numports);
			Serial.println(" ports");
			state = 1;
			poweron(1);
		}
	} else if (state < numports) {
		// turn on power to all ports
		poweron(++state);
	} else if (state == numports) {
		Serial.println("power turned on to all ports");
		// TODO: create interrupt pipe for status change notifications
		changepipe = new_Pipe(device, 3, endpoint, 1, 1);
		state = 255;
	} else if (state == 255) {
		// parse a status response
	}
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


