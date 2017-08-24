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

void MouseController::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	driver_ready_for_device(this);
}

bool MouseController::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	println("MouseController claim this=", (uint32_t)this, HEX);

	// only claim at interface level
	if (type != 1) return false;
	if (len < 9+9+7) return false;

	uint32_t numendpoint = descriptors[4];
	if (numendpoint < 1) return false;
	if (descriptors[5] != 3) return false; // bInterfaceClass, 3 = HID
	if (descriptors[6] != 1) return false; // bInterfaceSubClass, 1 = Boot Device
	if (descriptors[7] != 2) return false; // bInterfaceProtocol, 2 = Mouse
	if (descriptors[9] != 9) return false;
	if (descriptors[10] != 33) return false; // HID descriptor (ignored, Boot Protocol)
	if (descriptors[18] != 7) return false;
	if (descriptors[19] != 5) return false; // endpoint descriptor
	uint32_t endpoint = descriptors[20];
	println("ep(mouse) = ", endpoint, HEX);
	if ((endpoint & 0xF0) != 0x80) return false; // must be IN direction
	endpoint &= 0x0F;
	if (endpoint == 0) return false;
	if (descriptors[21] != 3) return false; // must be interrupt type
	uint32_t size = descriptors[22] | (descriptors[23] << 8);
	println("descriptors[22] = ",descriptors[22]);
	println("descriptors[23] = ",descriptors[23]);
	println("packet size(mouse) = ", size);
	// packey size seems to be 20 for (wireless type 2) or 6 bytes for wired
	packetSize = size;	
	if ((size != 20) && (size != 6)) return false; 
	if(packetSize == 6) packetSize = 8; // Minimum packet size needed is 8
	uint32_t interval = descriptors[24];
	println("polling interval = ", interval);
	datapipe = new_Pipe(dev, 3, endpoint, 1, packetSize, interval);
	datapipe->callback_function = callback;
	queue_Data_Transfer(datapipe, report, packetSize, this);
	mk_setup(setup, 0x21, 10, 0, 0, 0); // 10=SET_IDLE
	queue_Control_Transfer(dev, &setup, NULL, this);
	return true;
}

void MouseController::control(const Transfer_t *transfer)
{
}

void MouseController::callback(const Transfer_t *transfer)
{
	//println("MouseController Callback (static)");
	if (transfer->driver) {
		((MouseController *)(transfer->driver))->new_data(transfer);
	}
}

void MouseController::disconnect()
{
	// TODO: free resources
}


// Arduino defined this static weak symbol callback, and their
// examples use it as the only way to detect new key presses,
// so unfortunate as static weak callbacks are, it probably
// needs to be supported for compatibility
extern "C" {
void __MouseControllerEmptyCallback() { }
}

//void mouseEvent()  __attribute__ ((weak, alias("__mouseEventEmptyCallback")));

void MouseController::new_data(const Transfer_t *transfer)
{
	println("MouseController Callback (member)");
	print("  Mouse Data: ");
	print_hexbytes(transfer->buffer, 8);

	// The mouse button report byte is 1 for left button, 2 for right button, 4 for wheel
	// button (three button mouse).
	//
	// The mouse x and y report consists of two 12 bit signed values packed into three
	// bytes and are relative values. Shown below are the three x/y bytes of a wireless
	// mouse report packet.
	//
	//			byte 3 is the lower 8 bits of the x coordinate.
	//			byte 5 is the upper 8 bits of the y coordinate.
	//			byte 4 lower 4 bits are the upper 4 bits of the x coordinate.
	//			byte 4 upper 4 bits are the lower 4 bits of the y coordinate.
	//
	// Example of one increment in all four directions:
	//			x						x
	//		 ff 0f 00 = left -1		 00 01 00 = right 1
	//			y						y
	//		 00 f0 ff = up -1		 00 10 00 = down 1
	//
	// The wheel report is a signed byte that is 1 for forward and -1 for backward movement.
	// It is cleared to zero with any x and/or y movement.
	//
	// Wireless Logitech mouse reports have byte 0 set to 0x02 indicating a type 2 report.
	// Not sure what this really means yet but all bytes of the report packet are shifted
	// ahead by one byte and there is a single byte that is always zero after the button 
	// report byte.

	if(packetSize == 20) {
		buttons = report[1];
		mouseX  = ((report[4] & 0x0f) << 8 | (report[3] & 0xff));
		mouseY  = ((report[5] & 0xff) << 4 | (report[4] >> 4) & 0x0f);
		wheel   = report[6];
	} else {
		buttons = report[0];
		mouseX  = ((report[2] & 0x0f) << 8 | (report[1] & 0xff));
		mouseY  = ((report[3] & 0xff) << 4 | (report[2] >> 4) & 0x0f);
		wheel   = report[4];
	}
	mouseEvent = true;
	memcpy(prev_report, report, 20);
	queue_Data_Transfer(datapipe, report, 20, this);
}

void MouseController::mouseDataClear() {
	mouseEvent = false;
	buttons = 0;
	mouseX  = 0;
	mouseY  = 0;
	wheel   = 0;
}
	
