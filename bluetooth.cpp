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
 *
 * Note: special thanks to the Linux kernel for the CH341's method of operation, particularly how the baud rate is encoded.
 */

#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info

#define print   USBHost::print_
#define println USBHost::println_

/************************************************************/
//  Define mapping VID/PID - to Serial Device type.
/************************************************************/

/************************************************************/
//  Initialization and claiming of devices & interfaces
/************************************************************/

void BluetoothController::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
	driver_ready_for_device(this);
}

bool BluetoothController::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	// only claim at device level 
	println("BluetoothController claim this=", (uint32_t)this, HEX);
	print("vid=", dev->idVendor, HEX);
	print(", pid=", dev->idProduct, HEX);
	print(", bDeviceClass = ", dev->bDeviceClass, HEX);
   	print(", bDeviceSubClass = ", dev->bDeviceSubClass, HEX);
   	println(", bDeviceProtocol = ", dev->bDeviceProtocol, HEX);
	print_hexbytes(descriptors, len);

	// Lets try to support the main USB Bluetooth class...
	// http://www.usb.org/developers/defined_class/#BaseClassE0h
	if (dev->bDeviceClass != 0xe0) return false;	// not base class wireless controller
	if ((dev->bDeviceSubClass != 1) || (dev->bDeviceProtocol != 1)) return false; // Bluetooth Programming Interface

	if (type == 0) {
	}

	return false;
}


void BluetoothController::disconnect()
{
}



void BluetoothController::control(const Transfer_t *transfer)
{
	//println("control callback (bluetooth) ", pending_control, HEX);
}

/************************************************************/
//  Interrupt-based Data Movement
/************************************************************/

void BluetoothController::rx_callback(const Transfer_t *transfer)
{
	if (!transfer->driver) return;
	((BluetoothController *)(transfer->driver))->rx_data(transfer);
}

void BluetoothController::tx_callback(const Transfer_t *transfer)
{
	if (!transfer->driver) return;
	((BluetoothController *)(transfer->driver))->tx_data(transfer);
}


void BluetoothController::rx_data(const Transfer_t *transfer)
{
	//uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
}


void BluetoothController::tx_data(const Transfer_t *transfer)
{
}

