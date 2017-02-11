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

	if (dev->bDeviceClass != 9 || dev->bDeviceSubClass != 0) return false;

	// bDeviceProtocol = 0 is full speed
	// bDeviceProtocol = 1 is high speed single TT
	// bDeviceProtocol = 2 is high speed multiple TT

	Serial.print("bDeviceClass = ");
	Serial.println(dev->bDeviceClass);
	Serial.print("bDeviceSubClass = ");
	Serial.println(dev->bDeviceSubClass);
	Serial.print("bDeviceProtocol = ");
	Serial.println(dev->bDeviceProtocol);

	return true;
}

