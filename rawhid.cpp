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

void RawHIDController::init()
{
	USBHost::contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	USBHIDParser::driver_ready_for_hid_collection(this);	
}

hidclaim_t RawHIDController::claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage)
{
	// only claim RAWHID devices currently: 16c0:0486
#ifdef USBHOST_PRINT_DEBUG
	USBHDBGSerial.printf("Rawhid Claim: %x:%x usage: %x\n", dev->idVendor, dev->idProduct, topusage);
#endif

	if ((dev->idVendor != 0x16c0 || (dev->idProduct) != 0x486)) return CLAIM_NO;
	if (mydevice != NULL && dev != mydevice) return CLAIM_NO;
	if (usage_) return CLAIM_NO;			// Only claim one
	if (fixed_usage_  && (fixed_usage_ != topusage)) return CLAIM_NO; 	// See if we want specific one and if so is it this one
	mydevice = dev;
	collections_claimed++;
	usage_ = topusage;
	driver_ = driver;	// remember the driver. 
	return CLAIM_INTERFACE;  // We wa
}

void RawHIDController::disconnect_collection(Device_t *dev)
{
	if (--collections_claimed == 0) {
		mydevice = NULL;
		usage_ = 0;
	}
}

bool RawHIDController::hid_process_in_data(const Transfer_t *transfer) 
{
#ifdef USBHOST_PRINT_DEBUG
	USBHDBGSerial.printf("RawHIDController::hid_process_in_data: %x\n", usage_);
#endif

	if (receiveCB) {
		return (*receiveCB)(usage_, (const uint8_t *)transfer->buffer, transfer->length);
	}
	return true;
}

bool RawHIDController::hid_process_out_data(const Transfer_t *transfer) 
{
#ifdef USBHOST_PRINT_DEBUG
	USBHDBGSerial.printf("RawHIDController::hid_process_out_data: %x\n", usage_);
#endif
	return true;
}

bool RawHIDController::sendPacket(const uint8_t *buffer) 
{
	if (!driver_) return false;
	return driver_->sendPacket(buffer);
}



void RawHIDController::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax)
{
	// These should not be called as we are claiming the whole interface and not
	// allowing the parse to happen
#ifdef USBHOST_PRINT_DEBUG
	USBHDBGSerial.printf("RawHID::hid_input_begin %x %x %x %x\n", topusage, type, lgmin, lgmax);
#endif
	//hid_input_begin_ = true;
}

void RawHIDController::hid_input_data(uint32_t usage, int32_t value)
{
	// These should not be called as we are claiming the whole interface and not
	// allowing the parse to happen
#ifdef USBHOST_PRINT_DEBUG
	USBHDBGSerial.printf("RawHID: usage=%X, value=%d", usage, value);
	if ((value >= ' ') && (value <='~')) USBHDBGSerial.printf("(%c)", value);
	USBHDBGSerial.println();
#endif
}

void RawHIDController::hid_input_end()
{
	// These should not be called as we are claiming the whole interface and not
	// allowing the parse to happen
#ifdef USBHOST_PRINT_DEBUG
	USBHDBGSerial.println("RawHID::hid_input_end");
#endif
//	if (hid_input_begin_) {
//		hid_input_begin_ = false;
//	}
}


