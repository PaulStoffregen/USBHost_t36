/* USB Device Info class
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

// This simple class does nothing, but print out information about the device it
// extracts from calls to claim... 

#include <Arduino.h>
#include <USBHost_t36.h>
#include "USBDeviceInfo.h"

extern void dump_hexbytes(const void *ptr, uint32_t len, uint32_t indent);

static void println(const char *title, uint32_t val, uint8_t b = DEC) {
	Serial.print(title);
	Serial.println(val, b);	
}

static void print(const char *title, uint32_t val, uint8_t b = DEC) {
	Serial.print(title);
	Serial.print(val, b);	
}

void USBDeviceInfo::init()
{
	driver_ready_for_device(this);
}

// Again this class is solely to display as much information about a device as we can...
// This all comes from the information passed to it through the claim method.
bool USBDeviceInfo::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len) {
	println("\nUSBDeviceInfo claim this=", (uint32_t)this, HEX);


	if (type == 0) {
		// At device level
		Serial.println("\n****************************************");
		Serial.println("** Device Level **");
		println("  vid=", dev->idVendor, HEX);
		println("  pid=", dev->idProduct, HEX);
		println("  bDeviceClass = ", dev->bDeviceClass);
	   	println("  bDeviceSubClass = ", dev->bDeviceSubClass);
	   	println("  bDeviceProtocol = ", dev->bDeviceProtocol);
		dump_hexbytes(descriptors, len, 0);
		return false;
	}
	// only claim at interface level
	Serial.println("\n****************************************");
	Serial.println("** Interface Level **");
	dump_hexbytes(descriptors, len, 0);
	if (len < 9+9+7) return false;

	// interface descriptor
	uint32_t numendpoint = descriptors[4];
	println(" bInterfaceNumber = ",  descriptors[2]);
	println(" number end points = ",  numendpoint);
	println(" bInterfaceClass =    ", descriptors[5]);
	println(" bInterfaceSubClass = ", descriptors[6]);
	switch (descriptors[5]) {
		case 2: Serial.println("    Communications and CDC"); break;
		case 3:
			if (descriptors[6] == 1) Serial.println("    HID (BOOT)");
			else Serial.println("    HID"); 
			break;
		case 0xa: Serial.println("    CDC-Data"); break;
	}

	println(" bInterfaceProtocol = ", descriptors[7]);
  if (descriptors[5] == 3) {
    switch (descriptors[7]) {
      case 0: Serial.println("    None"); break;
      case 1: Serial.println("    Keyboard"); break;
      case 2: Serial.println("    Mouse"); break;
    }
    
  }  

	//if (numendpoint < 1 || numendpoint > 2) return false;

	// hid interface descriptor
	uint32_t offset = 9;
	uint32_t hidlen = descriptors[offset];
	uint16_t descsize = 0;

	if (hidlen < 9) return false;

	if (descriptors[10] == 33)  { // This is a HID Descriptor type. return false; // descriptor type, 33=HID
		if (descriptors[14] < 1) return false;  // must be at least 1 extra descriptor
		if (hidlen != (uint32_t)(6 + descriptors[14] * 3)) return false; // must be correct size
		if (9 + hidlen > len) return false;
		uint32_t i=0;
		while (1) {
			if (descriptors[15 + i * 3] == 34) { // found HID report descriptor
				descsize = descriptors[16 + i * 3] | (descriptors[17 + i * 3] << 8);
				println("report descriptor size = ", descsize);
				break;
			}
			i++;
			if (i >= descriptors[14]) break;;
		}
		offset += hidlen;
	}
	// endpoint descriptor(s)
	while (numendpoint && (offset < len)) {
		if ((descriptors[offset] == 7) && (descriptors[offset+1] == 5)) {
			// we have an end point:
			println("  endpoint = ", descriptors[offset+2], HEX);
			print(  "    attributes = ", descriptors[offset+3], HEX);
			switch (descriptors[offset+3] & 0x3) {
				case 0: Serial.println(" Control"); break;
				case 1: Serial.println(" Isochronous"); break;
				case 2: Serial.println(" Bulk"); break;
				case 3: Serial.println(" Interrupt"); break;
			}
			uint32_t size = descriptors[offset+4] | (descriptors[offset+5] << 8);
			println("    size = ", size);
			println("    interval = ", descriptors[offset+6]);
			numendpoint--;
		}
		offset += descriptors[offset];
	}
	return false;
}
