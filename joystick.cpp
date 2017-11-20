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

#define print   USBHost::print_
#define println USBHost::println_

void JoystickController::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
	driver_ready_for_device(this);
	USBHIDParser::driver_ready_for_hid_collection(this);
}

//*****************************************************************************
// Some simple query functions depend on which interface we are using...
//*****************************************************************************

uint16_t JoystickController::idVendor() 
{
	if (device != nullptr) return device->idVendor;
	if (mydevice != nullptr) return mydevice->idVendor;
	return 0;
}

uint16_t JoystickController::idProduct() 
{
	if (device != nullptr) return device->idProduct;
	if (mydevice != nullptr) return mydevice->idProduct;
	return 0;
}

const uint8_t *JoystickController::manufacturer()
{
	if ((device != nullptr) && (device->strbuf != nullptr)) return &device->strbuf->buffer[device->strbuf->iStrings[strbuf_t::STR_ID_MAN]];
	if ((mydevice != nullptr) && (mydevice->strbuf != nullptr)) return &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_MAN]]; 
	return nullptr;
}

const uint8_t *JoystickController::product()
{
	if ((device != nullptr) && (device->strbuf != nullptr)) return &device->strbuf->buffer[device->strbuf->iStrings[strbuf_t::STR_ID_PROD]];
	if ((mydevice != nullptr) && (mydevice->strbuf != nullptr)) return &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_PROD]]; 
	return nullptr;
}

const uint8_t *JoystickController::serialNumber()
{
	if ((device != nullptr) && (device->strbuf != nullptr)) return &device->strbuf->buffer[device->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]];
	if ((mydevice != nullptr) && (mydevice->strbuf != nullptr)) return &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]]; 
	return nullptr;
}



//*****************************************************************************
// Support for Joysticks that USe HID data. 
//*****************************************************************************

hidclaim_t JoystickController::claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage)
{
	// only claim Desktop/Joystick and Desktop/Gamepad
	if (topusage != 0x10004 && topusage != 0x10005) return CLAIM_NO;
	// only claim from one physical device
	if (mydevice != NULL && dev != mydevice) return CLAIM_NO;
	mydevice = dev;
	collections_claimed++;
	anychange = true; // always report values on first read
	return CLAIM_REPORT;
}

void JoystickController::disconnect_collection(Device_t *dev)
{
	if (--collections_claimed == 0) {
		mydevice = NULL;
		axis_mask_ = 0;	
	}
}

void JoystickController::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax)
{
	// TODO: set up translation from logical min/max to consistent 16 bit scale
}

void JoystickController::hid_input_data(uint32_t usage, int32_t value)
{
	//Serial.printf("Joystick: usage=%X, value=%d\n", usage, value);
	uint32_t usage_page = usage >> 16;
	usage &= 0xFFFF;
	if (usage_page == 9 && usage >= 1 && usage <= 32) {
		uint32_t bit = 1 << (usage -1);
		if (value == 0) {
			if (buttons & bit) {
				buttons &= ~bit;
				anychange = true;
			}
		} else {
			if (!(buttons & bit)) {
				buttons |= bit;
				anychange = true;
			}
		}
	} else if (usage_page == 1 && usage >= 0x30 && usage <= 0x39) {
		// TODO: need scaling of value to consistent API, 16 bit signed?
		// TODO: many joysticks repeat slider usage.  Detect & map to axis?
		uint32_t i = usage - 0x30;
		axis_mask_ |= (1 << i);		// Keep record of which axis we have data on.
		if (axis[i] != value) {
			axis[i] = value;
			anychange = true;
		}
	}
	// TODO: hat switch?
}

void JoystickController::hid_input_end()
{
	if (anychange) {
		joystickEvent = true;
	}
}

void JoystickController::joystickDataClear() {
	joystickEvent = false;
	anychange = false;
}

//*****************************************************************************
// Support for Joysticks that are class specific and do not use HID
// Example: XBox One controller. 
//*****************************************************************************
// Note: currently just XBOX one. 
JoystickController::product_vendor_mapping_t JoystickController::pid_vid_mapping[] = {
	{0x045e, 0x02ea} };

static  uint8_t start_input[] = {0x05, 0x20, 0x00, 0x01, 0x00};

bool JoystickController::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	println("JoystickController claim this=", (uint32_t)this, HEX);

	// only claim at device level
	if (type != 0) return false;
	print_hexbytes(descriptors, len);

	uint8_t i = 0;
	for (; i < (sizeof(pid_vid_mapping)/sizeof(pid_vid_mapping[0])); i++) {
		if ((dev->idVendor == pid_vid_mapping[i].idVendor) && (dev->idProduct == pid_vid_mapping[i].idProduct)) {
			break;
		}
	}  
	if (i == (sizeof(pid_vid_mapping)/sizeof(pid_vid_mapping[0]))) return false; 	// Not in our list

	//  0  1  2  3  4  5  6  7  8 *9 10  1  2  3  4  5 *6  7  8  9 20  1  2  3  4  5  6  7  8  9 30  1... 
	// 09 04 00 00 02 FF 47 D0 00 07 05 02 03 40 00 04 07 05 82 03 40 00 04 09 04 01 00 00 FF 47 D0 00 
	// Lets do some verifications to make sure. 

	if (len < 9+7+7) return false;

	uint32_t count_end_points = descriptors[4];
	if (count_end_points < 2) return false;
	if (descriptors[5] != 0xff) return false; // bInterfaceClass, 3 = HID
	uint32_t rxep = 0;
	uint32_t txep = 0;
	rx_size_ = 0;
	tx_size_ = 0;
	uint32_t descriptor_index = 9; 
	while (count_end_points-- && ((rxep == 0) || txep == 0)) {
		if (descriptors[descriptor_index] != 7) return false; // length 7
		if (descriptors[descriptor_index+1] != 5) return false; // ep desc
		if ((descriptors[descriptor_index+3] == 3) 				// Type 3...
			&& (descriptors[descriptor_index+4] <= 64)
			&& (descriptors[descriptor_index+5] == 0)) {
			// have a bulk EP size 
			if (descriptors[descriptor_index+2] & 0x80 ) {
				rxep = descriptors[descriptor_index+2];
				rx_size_ = descriptors[descriptor_index+4];
			} else {
				txep = descriptors[descriptor_index+2]; 
				tx_size_ = descriptors[descriptor_index+4];
			}
		}
		descriptor_index += 7;  // setup to look at next one...
	}
	if ((rxep == 0) || (txep == 0)) return false; // did not find two end points.
	print("JoystickController, rxep=", rxep & 15);
	print("(", rx_size_);
	print("), txep=", txep);
	print("(", tx_size_);
	println(")");
	rxpipe_ = new_Pipe(dev, 2, rxep & 15, 1, rx_size_);
	if (!rxpipe_) return false;
	txpipe_ = new_Pipe(dev, 2, txep, 0, tx_size_);
	if (!txpipe_) {
		//free_Pipe(rxpipe_);
		return false;
	}
	rxpipe_->callback_function = rx_callback;
	queue_Data_Transfer(rxpipe_, rxbuf_, rx_size_, this);

	txpipe_->callback_function = tx_callback;

	queue_Data_Transfer(txpipe_, start_input, sizeof(start_input), this);
	memset(axis, 0, sizeof(axis));	// clear out any data. 
	return true;
}

void JoystickController::control(const Transfer_t *transfer)
{
}


/************************************************************/
//  Interrupt-based Data Movement
/************************************************************/

void JoystickController::rx_callback(const Transfer_t *transfer)
{
	if (!transfer->driver) return;
	((JoystickController *)(transfer->driver))->rx_data(transfer);
}

void JoystickController::tx_callback(const Transfer_t *transfer)
{
	if (!transfer->driver) return;
	((JoystickController *)(transfer->driver))->tx_data(transfer);
}



/************************************************************/
//  Interrupt-based Data Movement
// XBox one input data when type == 0x20
// Information came from several places on the web including: 
// https://github.com/quantus/xbox-one-controller-protocol
/************************************************************/
typedef struct {
	uint8_t type;
	uint8_t const_0;
	uint16_t id;
	// From online references button order: 
	//     sync, dummy, start, back, a, b, x, y
	//     dpad up, down left, right
	//	   lb, rb, left stick, right stick
	// Axis: 
	//     lt, rt, lx, xy, rx, ry
	//     
	uint16_t buttons; 
	int16_t	axis[6];
} xbox1data20_t;

static const uint8_t xbox_axis_order_mapping[] = {4, 5, 0, 1, 2, 3};

void JoystickController::rx_data(const Transfer_t *transfer)
{
//	print("JoystickController::rx_data: ");
//	print_hexbytes((uint8_t*)transfer->buffer, transfer->length);
	axis_mask_ = 0x3f;	
	xbox1data20_t *xb1d = (xbox1data20_t *)transfer->buffer;
	if ((xb1d->type == 0x20) && (transfer->length >= sizeof (xbox1data20_t))) {
		// We have a data transfer.  Lets see what is new...
		if (xb1d->buttons != buttons) {
			buttons = xb1d->buttons;
			anychange = true;
		}
		for (uint8_t i = 0; i < sizeof (xbox_axis_order_mapping); i++) {
			// The first two values were unsigned. 
			int axis_value = (i < 2)? (int)(uint16_t)xb1d->axis[i] : xb1d->axis[i];
			if (axis_value != axis[xbox_axis_order_mapping[i]]) {
				axis[xbox_axis_order_mapping[i]] = axis_value;
				anychange = true;
			}
		}
		joystickEvent = true;
	}


	queue_Data_Transfer(rxpipe_, rxbuf_, rx_size_, this);
}

void JoystickController::tx_data(const Transfer_t *transfer)
{
}

void JoystickController::disconnect()
{
	axis_mask_ = 0;	
	// TODO: free resources
}


