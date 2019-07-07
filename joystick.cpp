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

//#define DEBUG_JOYSTICK
#ifdef  DEBUG_JOYSTICK
#define DBGPrintf USBHDBGSerial.printf
#else
#define DBGPrintf(...) 
#endif

// PID/VID to joystick mapping - Only the XBOXOne is used to claim the USB interface directly, 
// The others are used after claim-hid code to know which one we have and to use it for 
// doing other features.  
JoystickController::product_vendor_mapping_t JoystickController::pid_vid_mapping[] = {
	{ 0x045e, 0x02ea, XBOXONE, false },{ 0x045e, 0x02dd, XBOXONE, false },
	{ 0x045e, 0x0719, XBOX360, false},
	{ 0x054C, 0x0268, PS3, true}, 
	{ 0x054C, 0x042F, PS3, true},	// PS3 Navigation controller
	{ 0x054C, 0x03D5, PS3_MOTION, true},	// PS3 Motion controller
	{ 0x054C, 0x05C4, PS4, true}, 	{0x054C, 0x09CC, PS4, true },
	{ 0x046D, 0xC626, SpaceNav, true},  // 3d Connextion Space Navigator, 0x10008
	{ 0x046D, 0xC628, SpaceNav, true}  // 3d Connextion Space Navigator, 0x10008
};



//-----------------------------------------------------------------------------
void JoystickController::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
	driver_ready_for_device(this);
	USBHIDParser::driver_ready_for_hid_collection(this);
	BluetoothController::driver_ready_for_bluetooth(this);
}

//-----------------------------------------------------------------------------
JoystickController::joytype_t JoystickController::mapVIDPIDtoJoystickType(uint16_t idVendor, uint16_t idProduct, bool exclude_hid_devices)
{
	for (uint8_t i = 0; i < (sizeof(pid_vid_mapping)/sizeof(pid_vid_mapping[0])); i++) {
		if ((idVendor == pid_vid_mapping[i].idVendor) && (idProduct == pid_vid_mapping[i].idProduct)) {
			println("Match PID/VID: ", i, DEC);
			if (exclude_hid_devices && pid_vid_mapping[i].hidDevice) return UNKNOWN;
			return pid_vid_mapping[i].joyType;
		}
	}  
	return UNKNOWN; 	// Not in our list
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
	//if ((btdevice != nullptr) && (btdevice->strbuf != nullptr)) return &btdevice->strbuf->buffer[btdevice->strbuf->iStrings[strbuf_t::STR_ID_MAN]]; 
	if ((mydevice != nullptr) && (mydevice->strbuf != nullptr)) return &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_MAN]]; 
	return nullptr;
}

const uint8_t *JoystickController::product()
{
	if ((device != nullptr) && (device->strbuf != nullptr)) return &device->strbuf->buffer[device->strbuf->iStrings[strbuf_t::STR_ID_PROD]];
	if ((mydevice != nullptr) && (mydevice->strbuf != nullptr)) return &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_PROD]]; 
	if (btdevice != nullptr) return remote_name_;
	return nullptr;
}

const uint8_t *JoystickController::serialNumber()
{
	if ((device != nullptr) && (device->strbuf != nullptr)) return &device->strbuf->buffer[device->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]];
	if ((mydevice != nullptr) && (mydevice->strbuf != nullptr)) return &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]]; 
	return nullptr;
}


bool JoystickController::setRumble(uint8_t lValue, uint8_t rValue, uint8_t timeout)
{
	// Need to know which joystick we are on.  Start off with XBox support - maybe need to add some enum value for the known
	// joystick types. 
	rumble_lValue_ = lValue; 
	rumble_rValue_ = rValue;
	rumble_timeout_ = timeout;

	switch (joystickType_) {
		default:
			break;
		case PS3:
			return transmitPS3UserFeedbackMsg();
		case PS3_MOTION:
			return transmitPS3MotionUserFeedbackMsg();
		case PS4:
			return transmitPS4UserFeedbackMsg();
		case XBOXONE:
			// Lets try sending a request to the XBox 1.
			txbuf_[0] = 0x9;
			txbuf_[1] = 0x0;
			txbuf_[2] = 0x0;
			txbuf_[3] = 0x09; // Substructure (what substructure rest of this packet has)
			txbuf_[4] = 0x00; // Mode
			txbuf_[5] = 0x0f; // Rumble mask (what motors are activated) (0000 lT rT L R)
			txbuf_[6] = 0x0; // lT force
			txbuf_[7] = 0x0; // rT force
			txbuf_[8] = lValue; // L force
			txbuf_[9] = rValue; // R force
			txbuf_[10] = 0xff; // Length of pulse
			txbuf_[11] = 0x00; // Period between pulses			
			txbuf_[12] = 0x00; // Repeat			
			if (!queue_Data_Transfer(txpipe_, txbuf_, 13, this)) {
				println("XBoxOne rumble transfer fail");
			}
			return true;	// 
		case XBOX360:
			txbuf_[0] = 0x00;
			txbuf_[1] = 0x01;
			txbuf_[2] = 0x0F;
			txbuf_[3] = 0xC0;
			txbuf_[4] = 0x00;
			txbuf_[5] = lValue;
			txbuf_[6] = rValue;
			txbuf_[7] = 0x00;
			txbuf_[8] = 0x00;
			txbuf_[9] = 0x00;
			txbuf_[10] = 0x00;
			txbuf_[11] = 0x00;
			if (!queue_Data_Transfer(txpipe_, txbuf_, 12, this)) {
				println("XBox360 rumble transfer fail");
			}
			return true;
	} 
	return false;
}


bool JoystickController::setLEDs(uint8_t lr, uint8_t lg, uint8_t lb)
{
	// Need to know which joystick we are on.  Start off with XBox support - maybe need to add some enum value for the known
	// joystick types. 
	if ((leds_[0] != lr) || (leds_[1] != lg) || (leds_[2] != lb)) {
		leds_[0] = lr;
		leds_[1] = lg;
		leds_[2] = lb;

		switch (joystickType_) {
			case PS3:
				return transmitPS3UserFeedbackMsg();
			case PS3_MOTION:
				return transmitPS3MotionUserFeedbackMsg();
			case PS4:
				return transmitPS4UserFeedbackMsg();
			case XBOX360:
				// 0: off, 1: all blink then return to before
				// 2-5(TL, TR, BL, BR) - blink on then stay on
				// 6-9() - On 
			    // ...
				txbuf_[1] = 0x00;
				txbuf_[2] = 0x08;
				txbuf_[3] = 0x40 + lr;
				txbuf_[4] = 0x00;
				txbuf_[5] = 0x00;
				txbuf_[6] = 0x00;
				txbuf_[7] = 0x00;
				txbuf_[8] = 0x00;
				txbuf_[9] = 0x00;
				txbuf_[10] = 0x00;
				txbuf_[11] = 0x00;
				if (!queue_Data_Transfer(txpipe_, txbuf_, 12, this)) {
					println("XBox360 set leds fail");
				}
				return true;
			case XBOXONE:
			default:
				return false;
		} 
	}
	return false;
}

bool JoystickController::transmitPS4UserFeedbackMsg() {
	if (driver_)  {
		uint8_t packet[32];
	    memset(packet, 0, sizeof(packet));

	    packet[0] = 0x05; // Report ID
	    packet[1]= 0xFF;

	    packet[4] = rumble_lValue_; // Small Rumble
	    packet[5] = rumble_rValue_; // Big rumble
	    packet[6] = leds_[0]; // RGB value 
	    packet[7] = leds_[1]; 
	    packet[8] = leds_[2];
	    // 9, 10 flash ON, OFF times in 100ths of second?  2.5 seconds = 255
	    DBGPrintf("Joystick update Rumble/LEDs\n");
		return driver_->sendPacket(packet, 32);
	} else if (btdriver_) {
		uint8_t packet[79];
	    memset(packet, 0, sizeof(packet));
//0xa2, 0x11, 0xc0, 0x20, 0xf0, 0x04, 0x00
	    packet[0] = 0x52; 
	    packet[1] = 0x11;      // Report ID
		packet[2] = 0x80;
		//packet[3] = 0x20;
		packet[4] = 0xFF;

	    packet[7] = rumble_lValue_; // Small Rumble
	    packet[8] = rumble_rValue_; // Big rumble
	    packet[9] = leds_[0]; // RGB value 
	    packet[10] = leds_[1]; 
	    packet[11] = leds_[2];

	    // 12, 13 flash ON, OFF times in 100ths of sedond?  2.5 seconds = 255
	    DBGPrintf("Joystick update Rumble/LEDs\n");
     	btdriver_->sendL2CapCommand(packet, sizeof(packet), 0x40);

     	return true;
	}
	return false;
}

static const uint8_t PS3_USER_FEEDBACK_INIT[] = {
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00 };

bool JoystickController::transmitPS3UserFeedbackMsg() {
	if (driver_) {
	    memcpy(txbuf_, PS3_USER_FEEDBACK_INIT, 48);

	    txbuf_[1] = rumble_lValue_? rumble_timeout_ : 0;
	    txbuf_[2] = rumble_lValue_; // Small Rumble
	    txbuf_[3] = rumble_rValue_? rumble_timeout_ : 0; 
	    txbuf_[4] = rumble_rValue_; // Big rumble
	    txbuf_[9] = leds_[2] << 1; // RGB value 	// using third led now...
	    //DBGPrintf("\nJoystick update Rumble/LEDs %d %d %d %d %d\n",  txbuf_[1], txbuf_[2],  txbuf_[3],  txbuf_[4],  txbuf_[9]);
		return driver_->sendControlPacket(0x21, 9, 0x201, 0, 48, txbuf_); 
	} else if (btdriver_) {
		txbuf_[0] = 0x52;
		txbuf_[1] = 0x1;
	    memcpy(&txbuf_[2], PS3_USER_FEEDBACK_INIT, 48);

	    txbuf_[3] = rumble_lValue_? rumble_timeout_ : 0;
	    txbuf_[4] = rumble_lValue_; // Small Rumble
	    txbuf_[5] = rumble_rValue_? rumble_timeout_ : 0; 
	    txbuf_[6] = rumble_rValue_; // Big rumble
	    txbuf_[11] = leds_[2] << 1; // RGB value 
	    DBGPrintf("\nJoystick update Rumble/LEDs %d %d %d %d %d\n",  txbuf_[3], txbuf_[4],  txbuf_[5],  txbuf_[6],  txbuf_[11]);
     	btdriver_->sendL2CapCommand(txbuf_, 50, BluetoothController::CONTROL_SCID);
     	return true;
	}
	return false;
}

#define MOVE_REPORT_BUFFER_SIZE 7
#define MOVE_HID_BUFFERSIZE 50 // Size of the buffer for the Playstation Motion Controller

bool JoystickController::transmitPS3MotionUserFeedbackMsg() {
	if (driver_) {
        txbuf_[0] = 0x02; // Set report ID, this is needed for Move commands to work
        txbuf_[2] = leds_[0];
        txbuf_[3] = leds_[1];
        txbuf_[4] = leds_[2];
        txbuf_[6] = rumble_lValue_; // Set the rumble value into the write buffer

		//return driver_->sendControlPacket(0x21, 9, 0x201, 0, MOVE_REPORT_BUFFER_SIZE, txbuf_); 
		return driver_->sendPacket(txbuf_, MOVE_REPORT_BUFFER_SIZE);

	} else if (btdriver_) {
        txbuf_[0] = 0xA2; // HID BT DATA_request (0xA0) | Report Type (Output 0x02)
        txbuf_[1] = 0x02; // Report ID
        txbuf_[3] = leds_[0];
        txbuf_[4] = leds_[1];
        txbuf_[5] = leds_[2];
        txbuf_[7] = rumble_lValue_;
     	btdriver_->sendL2CapCommand(txbuf_, MOVE_HID_BUFFERSIZE, BluetoothController::INTERRUPT_SCID);
     	return true;
	}
	return false;
}

//*****************************************************************************
// Support for Joysticks that Use HID data. 
//*****************************************************************************

hidclaim_t JoystickController::claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage)
{
	// only claim Desktop/Joystick and Desktop/Gamepad
	if (topusage != 0x10004 && topusage != 0x10005 && topusage != 0x10008) return CLAIM_NO;
	// only claim from one physical device
	if (mydevice != NULL && dev != mydevice) return CLAIM_NO;

	// Also don't allow us to claim if it is used as a standard usb object (XBox...)
	if (device != nullptr) return CLAIM_NO;

	mydevice = dev;
	collections_claimed++;
	anychange = true; // always report values on first read
	driver_ = driver;	// remember the driver. 
	driver_->setTXBuffers(txbuf_, nullptr, sizeof(txbuf_));
	connected_ = true;		// remember that hardware is actually connected...

	// Lets see if we know what type of joystick this is. That is, is it a PS3 or PS4 or ...
	joystickType_ = mapVIDPIDtoJoystickType(mydevice->idVendor, mydevice->idProduct, false);
	DBGPrintf("JoystickController::claim_collection joystickType_=%d\n", joystickType_);
	switch (joystickType_) {
		case PS3:
		case PS3_MOTION: // not sure yet
			additional_axis_usage_page_ = 0x1;
			additional_axis_usage_start_ = 0x100;
			additional_axis_usage_count_ = 39;
			axis_change_notify_mask_ = (uint64_t)-1;	// Start off assume all bits 
			break;
		case PS4:
			additional_axis_usage_page_ = 0xFF00;
			additional_axis_usage_start_ = 0x21;
			additional_axis_usage_count_ = 54;
			axis_change_notify_mask_ = (uint64_t)0xfffffffffffff3ffl;	// Start off assume all bits - 10 and 11
			break;
		default: 
			additional_axis_usage_page_ = 0x09;
			additional_axis_usage_start_ = 0x21;
			additional_axis_usage_count_ = 5;
			axis_change_notify_mask_ = 0x3ff;	// Start off assume only the 10 bits...
	}
	DBGPrintf("Claim Additional axis: %x %x %d\n", additional_axis_usage_page_, additional_axis_usage_start_, additional_axis_usage_count_);
	return CLAIM_REPORT;
}

void JoystickController::disconnect_collection(Device_t *dev)
{
	if (--collections_claimed == 0) {
		mydevice = NULL;
		driver_ = nullptr;
		axis_mask_ = 0;	
		axis_changed_mask_ = 0;
	}
}

void JoystickController::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax)
{
	// TODO: set up translation from logical min/max to consistent 16 bit scale
}

void JoystickController::hid_input_data(uint32_t usage, int32_t value)
{
	DBGPrintf("joystickType_=%d\n", joystickType_);
	DBGPrintf("Joystick: usage=%X, value=%d\n", usage, value);
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
			axis_changed_mask_ |= (1 << i);
			if (axis_changed_mask_ & axis_change_notify_mask_)
				anychange = true;
		}
	} else if (usage_page == additional_axis_usage_page_) {
		// see if the usage is witin range.
		//DBGPrintf("UP: usage_page=%x usage=%x User: %x %d\n", usage_page, usage, user_buttons_usage_start, user_buttons_count_);
		if ((usage >= additional_axis_usage_start_) && (usage < (additional_axis_usage_start_ + additional_axis_usage_count_))) {
			// We are in the user range. 
			uint16_t usage_index = usage - additional_axis_usage_start_ + STANDARD_AXIS_COUNT;
			if (usage_index < (sizeof(axis)/sizeof(axis[0]))) {
				if (axis[usage_index] != value) {
					axis[usage_index] = value;
					if (usage_index > 63) usage_index = 63;	// don't overflow our mask
					axis_changed_mask_ |= ((uint64_t)1 << usage_index);		// Keep track of which ones changed.
					if (axis_changed_mask_ & axis_change_notify_mask_)
						anychange = true;	// We have changes... 
				}
				axis_mask_ |= ((uint64_t)1 << usage_index);		// Keep record of which axis we have data on.
			}
			//DBGPrintf("UB: index=%x value=%x\n", usage_index, value);
		}

	} else {
		DBGPrintf("UP: usage_page=%x usage=%x add: %x %x %d\n", usage_page, usage, additional_axis_usage_page_, additional_axis_usage_start_, additional_axis_usage_count_);

	}
	// TODO: hat switch?
}

void JoystickController::hid_input_end()
{
	if (anychange) {
		joystickEvent = true;
	}
}

bool JoystickController::hid_process_out_data(const Transfer_t *transfer) 
{
	//DBGPrintf("JoystickController::hid_process_out_data\n");
	return true;
}

void JoystickController::joystickDataClear() {
	joystickEvent = false;
	anychange = false;
	axis_changed_mask_ = 0;
	axis_mask_ = 0;
}

//*****************************************************************************
// Support for Joysticks that are class specific and do not use HID
// Example: XBox One controller. 
//*****************************************************************************

static  uint8_t xboxone_start_input[] = {0x05, 0x20, 0x00, 0x01, 0x00};
static  uint8_t xbox360w_inquire_present[] = {0x08, 0x00, 0x0F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

bool JoystickController::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	println("JoystickController claim this=", (uint32_t)this, HEX);

	// Don't try to claim if it is used as USB device or HID device
	if (mydevice != NULL) return false;
	if (device != nullptr) return false;

	// Try claiming at the interface level.
	if (type != 1) return false;
	print_hexbytes(descriptors, len);

	JoystickController::joytype_t jtype = mapVIDPIDtoJoystickType(dev->idVendor, dev->idProduct, true);
	println("Jtype=", (uint8_t)jtype, DEC);
	if (jtype == UNKNOWN)
		return false; 

	// XBOX One
	//  0  1  2  3  4  5  6  7  8 *9 10  1  2  3  4  5 *6  7  8  9 20  1  2  3  4  5  6  7  8  9 30  1... 
	// 09 04 00 00 02 FF 47 D0 00 07 05 02 03 40 00 04 07 05 82 03 40 00 04 09 04 01 00 00 FF 47 D0 00 
	// Lets do some verifications to make sure. 

	// XBOX 360 wireless... Has 8 interfaces.  4 joysticks (1, 3, 5, 7) and 4 headphones assume 2,4,6, 8... 
	// Shows data for #1 only... 
	// Also they have some unknown data type we need to ignore between interface and end points.
	//  0  1  2  3  4  5  6  7  8 *9 10  1  2  3  4  5 *6  7  8  9 20  1  2  3  4  5  6  7  8 
	// 09 04 00 00 02 FF 5D 81 00 14 22 00 01 13 81 1D 00 17 01 02 08 13 01 0C 00 0C 01 02 08 

  	// 29 30  1  2  3  4  5  6  7  8  9 40 41 42
	// 07 05 81 03 20 00 01 07 05 01 03 20 00 08 

	if (len < 9+7+7) return false;

	// Some common stuff for both XBoxs
	uint32_t count_end_points = descriptors[4];
	if (count_end_points < 2) return false;
	if (descriptors[5] != 0xff) return false; // bInterfaceClass, 3 = HID
	rx_ep_ = 0;
	uint32_t txep = 0;
	uint8_t rx_interval = 0;
	uint8_t tx_interval = 0;
	rx_size_ = 0;
	tx_size_ = 0;
	uint32_t descriptor_index = 9; 
	if (descriptors[descriptor_index+1] == 0x22)  {
		if (descriptors[descriptor_index] != 0x14) return false; // only support specific versions...
		descriptor_index += descriptors[descriptor_index]; // XBox360w ignore this unknown setup...
	}	
	while (count_end_points-- && ((rx_ep_ == 0) || txep == 0)) {
		if (descriptors[descriptor_index] != 7) return false; // length 7
		if (descriptors[descriptor_index+1] != 5) return false; // ep desc
		if ((descriptors[descriptor_index+3] == 3) 				// Type 3...
			&& (descriptors[descriptor_index+4] <= 64)
			&& (descriptors[descriptor_index+5] == 0)) {
			// have a bulk EP size 
			if (descriptors[descriptor_index+2] & 0x80 ) {
				rx_ep_ = descriptors[descriptor_index+2];
				rx_size_ = descriptors[descriptor_index+4];
				rx_interval = descriptors[descriptor_index+6];
			} else {
				txep = descriptors[descriptor_index+2]; 
				tx_size_ = descriptors[descriptor_index+4];
				tx_interval = descriptors[descriptor_index+6];
			}
		}
		descriptor_index += 7;  // setup to look at next one...
	}
	if ((rx_ep_ == 0) || (txep == 0)) return false; // did not find two end points.
	print("JoystickController, rx_ep_=", rx_ep_ & 15);
	print("(", rx_size_);
	print("), txep=", txep);
	print("(", tx_size_);
	println(")");
	rxpipe_ = new_Pipe(dev, 3, rx_ep_ & 15, 1, rx_size_, rx_interval);
	if (!rxpipe_) return false;
	txpipe_ = new_Pipe(dev, 3, txep, 0, tx_size_, tx_interval);
	if (!txpipe_) {
		//free_Pipe(rxpipe_);
		return false;
	}
	rxpipe_->callback_function = rx_callback;
	queue_Data_Transfer(rxpipe_, rxbuf_, rx_size_, this);

	txpipe_->callback_function = tx_callback;

	if (jtype == XBOXONE) {
		queue_Data_Transfer(txpipe_, xboxone_start_input, sizeof(xboxone_start_input), this);
		connected_ = true;		// remember that hardware is actually connected...
	} else if (jtype == XBOX360) {
		queue_Data_Transfer(txpipe_, xbox360w_inquire_present, sizeof(xbox360w_inquire_present), this);
		connected_ = 0;		// remember that hardware is actually connected...
	}
	memset(axis, 0, sizeof(axis));	// clear out any data. 
	joystickType_ = jtype;		// remember we are an XBox One. 
	DBGPrintf("   JoystickController::claim joystickType_ %d\n", joystickType_);
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
// 20 00 C5 0E 00 00 00 00 00 00 F0 06 AD FB 7A 0A DD F7 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
// 20 00 E0 0E 40 00 00 00 00 00 F0 06 AD FB 7A 0A DD F7 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
typedef struct {
	uint8_t type;
	uint8_t const_0;
	uint16_t id;
	// From online references button order: 
	//     sync, dummy, start, back, a, b, x, y
	//     dpad up, down left, right
	//	   lb, rb, left stick, right stick
	// Axis: 
	//     lt, rt, lx, ly, rx, ry
	//     
	uint16_t buttons; 
	int16_t	axis[6];
} xbox1data20_t;

typedef struct {
	uint8_t state;
	uint8_t id_or_type;
	uint16_t controller_status;
	uint16_t unknown;
	// From online references button order: 
	//     sync, dummy, start, back, a, b, x, y
	//     dpad up, down left, right
	//	   lb, rb, left stick, right stick
	// Axis: 
	//     lt, rt, lx, ly, rx, ry
	//
	uint16_t buttons; 
	uint8_t lt;
	uint8_t rt;
	int16_t	axis[4];
} xbox360data_t;

static const uint8_t xbox_axis_order_mapping[] = {3, 4, 0, 1, 2, 5};

void JoystickController::rx_data(const Transfer_t *transfer)
{
	print("JoystickController::rx_data (", joystickType_, DEC);
	print("): ");
	print_hexbytes((uint8_t*)transfer->buffer, transfer->length);

	if (joystickType_ == XBOXONE) {
		// Process XBOX One data
		axis_mask_ = 0x3f;	
		axis_changed_mask_ = 0;	// assume none for now
		xbox1data20_t *xb1d = (xbox1data20_t *)transfer->buffer;
		if ((xb1d->type == 0x20) && (transfer->length >= sizeof (xbox1data20_t))) {
			// We have a data transfer.  Lets see what is new...
			if (xb1d->buttons != buttons) {
				buttons = xb1d->buttons;
				anychange = true;
				joystickEvent = true;
				println("  Button Change: ", buttons, HEX);
			}
			for (uint8_t i = 0; i < sizeof (xbox_axis_order_mapping); i++) {
				// The first two values were unsigned. 
				int axis_value = (i < 2)? (int)(uint16_t)xb1d->axis[i] : xb1d->axis[i];
				if (axis_value != axis[xbox_axis_order_mapping[i]]) {
					axis[xbox_axis_order_mapping[i]] = axis_value;
					axis_changed_mask_ |= (1 << xbox_axis_order_mapping[i]);
					anychange = true;
				}
			}
			joystickEvent = true;
		}

	} else if (joystickType_ == XBOX360) {
		// First byte appears to status - if the byte is 0x8 it is a connect or disconnect of the controller. 
		xbox360data_t  *xb360d = (xbox360data_t *)transfer->buffer;
		if (xb360d->state == 0x08) {
			if (xb360d->id_or_type != connected_) {
				connected_ = xb360d->id_or_type;	// remember it... 
				if (connected_) {
					println("XBox360w - Connected type:", connected_, HEX);
					// rx_ep_ should be 1, 3, 5, 7 for the wireless convert to 2-5 on led
					setLEDs(2+rx_ep_/2);	// Right now hard coded to first joystick...

				} else {
					println("XBox360w - disconnected");
				}
			}
		} else if((xb360d->id_or_type == 0x00) && (xb360d->controller_status & 0x1300)) {
			  // Controller status report - Maybe we should save away and allow the user access?
	            println("XBox360w - controllerStatus: ", xb360d->controller_status, HEX);
        } else if(xb360d->id_or_type == 0x01) { // Lets only process report 1.
			//const uint8_t *pbuffer = (uint8_t*)transfer->buffer;
        	//for (uint8_t i = 0; i < transfer->length; i++) DBGPrintf("%02x ", pbuffer[i]);
        	//DBGPrintf("\n");
	        
	        if (buttons != xb360d->buttons) {
	        	buttons = xb360d->buttons;
	        	anychange = true;
	        }
			axis_mask_ = 0x3f;	
			axis_changed_mask_ = 0;	// assume none for now

			for (uint8_t i = 0; i < 4; i++) {
				if (axis[i] != xb360d->axis[i]) {
					axis[i] = xb360d->axis[i];
					axis_changed_mask_ |= (1 << i);
					anychange = true;
				}
			}
			// the two triggers show up as 4 and 5
			if (axis[4] != xb360d->lt) {
				axis[4] = xb360d->lt;
				axis_changed_mask_ |= (1 << 4);
				anychange = true;
			}

			if (axis[5] != xb360d->rt) {
				axis[5] = xb360d->rt;
				axis_changed_mask_ |= (1 << 5);
				anychange = true;
			}

			if (anychange) joystickEvent = true;
		}
	}

	queue_Data_Transfer(rxpipe_, rxbuf_, rx_size_, this);
}

void JoystickController::tx_data(const Transfer_t *transfer)
{
}

void JoystickController::disconnect()
{
	axis_mask_ = 0;	
	axis_changed_mask_ = 0;
	// TODO: free resources
}

bool JoystickController::claim_bluetooth(BluetoothController *driver, uint32_t bluetooth_class, uint8_t *remoteName) 
{
	if ((((bluetooth_class & 0xff00) == 0x2500) || (((bluetooth_class & 0xff00) == 0x500))) && ((bluetooth_class & 0x3C) == 0x08)) {
		DBGPrintf("JoystickController::claim_bluetooth TRUE\n");
		btdriver_ = driver;
		btdevice = (Device_t*)driver;	// remember this way 
		if (remoteName) mapNameToJoystickType(remoteName);
		return true;
	}

	if (remoteName && mapNameToJoystickType(remoteName)) {
		if ((joystickType_ == PS3) || (joystickType_ == PS3_MOTION)) {
			DBGPrintf("JoystickController::claim_bluetooth TRUE PS3 hack...\n");
			btdriver_ = driver;
			btdevice = (Device_t*)driver;	// remember this way 
			special_process_required = SP_PS3_IDS; 		// PS3 maybe needs different IDS. 
			return true;
		}
	}
	return false;
}


bool JoystickController::process_bluetooth_HID_data(const uint8_t *data, uint16_t length) 
{
	// Example data from PS4 controller
	//01 7e 7f 82 84 08 00 00 00 00
	//   LX LY RX RY BT BT PS LT RT
	DBGPrintf("JoystickController::process_bluetooth_HID_data: data[0]=%x\n", data[0]);
	// May have to look at this one with other controllers...
	if (data[0] == 1) {
		//print("  Joystick Data: ");
		// print_hexbytes(data, length);
		if (length > TOTAL_AXIS_COUNT) length = TOTAL_AXIS_COUNT;	// don't overflow arrays...
		DBGPrintf("  Joystick Data: ");
		for(uint16_t i =0; i < length; i++) DBGPrintf("%02x ", data[i]);
		DBGPrintf("\r\n");	
		if (joystickType_ == PS3) {
			// Quick and dirty hack to match PS3 HID data
			uint32_t cur_buttons = data[2] | ((uint16_t)data[3] << 8) | ((uint32_t)data[4] << 16); 
			if (cur_buttons != buttons) {
				buttons = cur_buttons;
				joystickEvent = true;	// something changed.
			}

			uint64_t mask = 0x1;
			axis_mask_ = 0x27;	// assume bits 0, 1, 2, 5
			for (uint16_t i = 0; i < 3; i++) {
				if (axis[i] != data[i+6]) {
					axis_changed_mask_ |= mask;
					axis[i] = data[i+6];
				}
				mask <<= 1;	// shift down the mask.
			}
			if (axis[5] != data[9]) {
				axis_changed_mask_ |= (1<<5);
				axis[5] = data[9];
			}
			
			if (axis[3] != data[18]) {
				axis_changed_mask_ |= (1<<3);
				axis[3] = data[18];
			}
			
			if (axis[4] != data[19]) {
				axis_changed_mask_ |= (1<<4);
				axis[4] = data[19];
			}
			
			// Then rest of data
			mask = 0x1 << 10;	// setup for other bits
			for (uint16_t i = 10; i < length; i++ ) {
				axis_mask_ |= mask;
				if(data[i] != axis[i]) { 
					axis_changed_mask_ |= mask;
					axis[i] = data[i];
				} 
				mask <<= 1;	// shift down the mask.
			}
		} else if (joystickType_ == PS3_MOTION) {
			// Quick and dirty PS3_Motion data.
			uint32_t cur_buttons = data[1] | ((uint16_t)data[2] << 8) | ((uint32_t)data[3] << 16); 
			if (cur_buttons != buttons) {
				buttons = cur_buttons;
				joystickEvent = true;	// something changed.
			}

			// Hard to know what is best here. for now just copy raw data over... 
			// will do this for now... Format of thought to be data.
			//  data[1-3] Buttons (mentioned 4 as well but appears to be counter
			// axis[0-1] data[5] Trigger, Previous trigger value
			// 2-5 Unknown probably place holders for Axis like data for other PS3
			// 6 - Time stamp
			// 7 - Battery
			// 8-19 - Accel: XL, XH, YL, YH, ZL, ZH, XL2, XH2, YL2, YH2, ZL2, ZH2
			// 20-31 - Gyro: Xl,Xh,Yl,Yh,Zl,Zh,Xl2,Xh2,Yl2,Yh2,Zl2,Zh2
			// 32 - Temp High
			// 33 - Temp Low (4 bits)  Maybe Magneto x High on other?? 
			uint64_t mask = 0x1;
			axis_mask_ = 0;	// assume bits 0, 1, 2, 5
			// Then rest of data
			mask = 0x1 << 10;	// setup for other bits
			for (uint16_t i = 5; i < length; i++ ) {
				axis_mask_ |= mask;
				if(data[i] != axis[i-5]) { 
					axis_changed_mask_ |= mask;
					axis[i-5] = data[i];
				} 
				mask <<= 1;	// shift down the mask.
			}

		} else {
			uint64_t mask = 0x1;
			axis_mask_ = 0;

			for (uint16_t i = 0; i < length; i++ ) {
				axis_mask_ |= mask;
				if(data[i] != axis[i]) { 
					axis_changed_mask_ |= mask;
					axis[i] = data[i];
				} 
				mask <<= 1;	// shift down the mask.
//				DBGPrintf("%02x ", axis[i]);
			}

		}

		if (axis_changed_mask_ & axis_change_notify_mask_)
			joystickEvent = true;
		connected_ = true;
		return true;

	} else if(data[0] == 0x11){
		DBGPrintf("\n  Joystick Data: ");
		uint64_t mask = 0x1;
		axis_mask_ = 0;
		axis_changed_mask_ = 0;
		
		//This moves data to be equivalent to what we see for
		//data[0] = 0x01
		uint8_t tmp_data[length-2];
		
		for (uint16_t i = 0; i < (length-2); i++ ) {
			tmp_data[i] = 0;
			tmp_data[i] = data[i+2];
		}
		
		/*
		 * [1] LX, [2] = LY, [3] = RX, [4] = RY
		 * [5] combo, tri, cir, x, sqr, D-PAD (4bits, 0-3
		 * [6] R3,L3, opt, share, R2, L2, R1, L1
		 * [7] Counter (bit7-2), T-PAD, PS
		 * [8] Left Trigger, [9] Right Trigger
		 * [10-11] Timestamp
		 * [12] Battery (0 to 0xff)
		 * [13-14] acceleration x
		 * [15-16] acceleration y
		 * [17-18] acceleration z
		 * [19-20] gyro x
		 * [21-22] gyro y
		 * [23-24] gyro z
		 * [25-29] unknown
		 * [30] 0x00,phone,mic, usb, battery level (4bits)
		 * rest is trackpad?  to do implement?
		 */
		//PS Bit
		tmp_data[7] = (tmp_data[7] >> 0) & 1;
		//set arrow buttons to axis[0]
		tmp_data[10] = tmp_data[5] & ((1 << 4) - 1);
		//set buttons for last 4bits in the axis[5]
		tmp_data[5] = tmp_data[5] >> 4;
		
	
		// Quick and dirty hack to match PS4 HID data
		uint32_t cur_buttons = tmp_data[7] | (tmp_data[10]) | ((tmp_data[6]*10)) | ((uint16_t)tmp_data[5] << 16) ; 
		if (cur_buttons != buttons) {
			buttons = cur_buttons;
			joystickEvent = true;	// something changed.
		}
		
		mask = 0x1;
		axis_mask_ = 0x27;	// assume bits 0, 1, 2, 5
		for (uint16_t i = 0; i < 3; i++) {
			if (axis[i] != tmp_data[i+1]) {
				axis_changed_mask_ |= mask;
				axis[i] = tmp_data[i+1];
			}
			mask <<= 1;	// shift down the mask.
		}
		if (axis[5] != tmp_data[4]) {
			axis_changed_mask_ |= (1<<5);
			axis[5] = tmp_data[4];
		}
		
		if (axis[3] != tmp_data[8]) {
			axis_changed_mask_ |= (1<<3);
			axis[3] = tmp_data[8];
		}
		
		if (axis[4] != tmp_data[9]) {
			axis_changed_mask_ |= (1<<4);
			axis[4] = tmp_data[9];
		}
		
		//limit for masking
		mask = 0x1;
		for (uint16_t i = 6; i < (64); i++ ) {
			axis_mask_ |= mask;
			if(tmp_data[i] != axis[i]) { 
				axis_changed_mask_ |= mask;
				axis[i] = tmp_data[i];
			}
			mask <<= 1;	// shift down the mask.
			DBGPrintf("%02x ", axis[i]);
		}
		DBGPrintf("\n");
		//DBGPrintf("Axis Mask (axis_mask_, axis_changed_mask_; %d, %d\n", axis_mask_,axis_changed_mask_);
		joystickEvent = true;
		connected_ = true;
	}
	return false;
}

bool JoystickController::mapNameToJoystickType(const uint8_t *remoteName)
{
	// Sort of a hack, but try to map the name given from remote to a type...
	if (strncmp((const char *)remoteName, "Wireless Controller", 19) == 0) {
		DBGPrintf("  JoystickController::mapNameToJoystickType %s - set to PS4\n", remoteName);
		joystickType_ = PS4;
	} else if (strncmp((const char *)remoteName, "PLAYSTATION(R)3", 15) == 0) {
		DBGPrintf("  JoystickController::mapNameToJoystickType %x %s - set to PS3\n", (uint32_t)this, remoteName);
		joystickType_ = PS3;
	} else if (strncmp((const char *)remoteName, "Navigation Controller", 21) == 0) {
		DBGPrintf("  JoystickController::mapNameToJoystickType %x %s - set to PS3\n", (uint32_t)this, remoteName);
		joystickType_ = PS3;
	} else if (strncmp((const char *)remoteName, "Motion Controller", 17) == 0) {
		DBGPrintf("  JoystickController::mapNameToJoystickType %x %s - set to PS3 Motion\n", (uint32_t)this, remoteName);
		joystickType_ = PS3_MOTION;
	} else if (strncmp((const char *)remoteName, "Xbox Wireless", 13) == 0) {
		DBGPrintf("  JoystickController::mapNameToJoystickType %x %s - set to XBOXONE\n", (uint32_t)this, remoteName);
		joystickType_ = XBOXONE;
	} else {
		DBGPrintf("  JoystickController::mapNameToJoystickType %s - Unknown\n", remoteName);
	}
	DBGPrintf("  Joystick Type: %d\n", joystickType_);
	return true;
}


bool JoystickController::remoteNameComplete(const uint8_t *remoteName) 
{
	// Sort of a hack, but try to map the name given from remote to a type...
	if (mapNameToJoystickType(remoteName)) {
		switch (joystickType_) {
			case PS4: special_process_required = SP_NEED_CONNECT; break;
			case PS3: special_process_required = SP_PS3_IDS; break;
			case PS3_MOTION: special_process_required = SP_PS3_IDS; break;
			default: 
				break;
		}
	}
	return true;
}

void JoystickController::connectionComplete() 
{
	DBGPrintf("  JoystickController::connectionComplete %x joystick type %d\n", (uint32_t)this, joystickType_);
	switch (joystickType_) {
	case PS4:
		{
			uint8_t packet[2];
			packet[0] = 0x43; // HID BT Get_report (0x40) | Report Type (Feature 0x03)
			packet[1] = 0x02; // Report ID
			DBGPrintf("Set PS4 report\n");
			delay(1);
			btdriver_->sendL2CapCommand(packet, sizeof(packet), 0x40);			
		}
		break;
	case PS3:
		{
			uint8_t packet[6];
			packet[0] = 0x53; // HID BT Set_report (0x50) | Report Type (Feature 0x03)
			packet[1] = 0xF4; // Report ID
			packet[2] = 0x42; // Special PS3 Controller enable commands
			packet[3] = 0x03;
			packet[4] = 0x00;
			packet[5] = 0x00;

			DBGPrintf("enable six axis\n");
			delay(1);
			btdriver_->sendL2CapCommand(packet, sizeof(packet), BluetoothController::CONTROL_SCID);
		}
		break;
	case PS3_MOTION:	
		setLEDs(0, 0xff, 0);	// Maybe try setting to green? 
	default:
		break;	
	}
}

void JoystickController::release_bluetooth() 
{
	btdevice = nullptr;	// remember this way 
	btdriver_ = nullptr;
	connected_ = false;
	special_process_required = false;

}


bool JoystickController::PS3Pair(uint8_t* bdaddr) {
	if (!driver_) return false;
 	if (joystickType_ == PS3) {
 	    /* Set the internal Bluetooth address */
	    txbuf_[0] = 0x01;
	    txbuf_[1] = 0x00;

	    for(uint8_t i = 0; i < 6; i++)
	            txbuf_[i + 2] = bdaddr[5 - i]; // Copy into buffer, has to be written reversed, so it is MSB first

	    // bmRequest = Host to device (0x00) | Class (0x20) | Interface (0x01) = 0x21, bRequest = Set Report (0x09), Report ID (0xF5), Report Type (Feature 0x03), interface (0x00), datalength, datalength, data
		return driver_->sendControlPacket(0x21, 9, 0x3f5, 0, 8, txbuf_); 
	} else if (joystickType_ == PS3_MOTION) {
		// Slightly different than other PS3 units...
	    txbuf_[0] = 0x05;
	    for(uint8_t i = 0; i < 6; i++)
	            txbuf_[i + 1] = bdaddr[i]; // Order different looks like LSB First?

        txbuf_[7] = 0x10;
        txbuf_[8] = 0x01;
        txbuf_[9] = 0x02;
        txbuf_[10] = 0x12;
	    // bmRequest = Host to device (0x00) | Class (0x20) | Interface (0x01) = 0x21, bRequest = Set Report (0x09), Report ID (0xF5), Report Type (Feature 0x03), interface (0x00), datalength, datalength, data
		return driver_->sendControlPacket(0x21, 9, 0x305, 0, 11, txbuf_); 
	}
	return false;
}
