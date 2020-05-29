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
#include "keylayouts.h"   // from Teensyduino core library

typedef struct {
	KEYCODE_TYPE code;
	uint8_t		 ascii;
} keycode_extra_t;

typedef struct {
	KEYCODE_TYPE code;
	KEYCODE_TYPE codeNumlockOff;
	uint8_t charNumlockOn;		// We will assume when num lock is on we have all characters...
} keycode_numlock_t;

typedef struct {
	uint16_t	idVendor;		// vendor id of keyboard
	uint16_t	idProduct;		// product id - 0 implies all of the ones from vendor; 
} keyboard_force_boot_protocol_t;	// list of products to force into boot protocol

#ifdef M
#undef M
#endif
#define M(n) ((n) & KEYCODE_MASK)

static const keycode_extra_t keycode_extras[] = {
	{M(KEY_ENTER), '\n'},
	{M(KEY_ESC), 0x1b},
	{M(KEY_TAB), 0x9 },
	{M(KEY_UP), KEYD_UP },
	{M(KEY_DOWN), KEYD_DOWN },
	{M(KEY_LEFT), KEYD_LEFT },
	{M(KEY_RIGHT), KEYD_RIGHT },
	{M(KEY_INSERT), KEYD_INSERT },
	{M(KEY_DELETE), KEYD_DELETE }, 
	{M(KEY_PAGE_UP), KEYD_PAGE_UP },
	{M(KEY_PAGE_DOWN), KEYD_PAGE_DOWN }, 
	{M(KEY_HOME), KEYD_HOME },
	{M(KEY_END), KEYD_END },   
	{M(KEY_F1), KEYD_F1 },
	{M(KEY_F2), KEYD_F2 },     
	{M(KEY_F3), KEYD_F3 },     
	{M(KEY_F4), KEYD_F4 },     
	{M(KEY_F5), KEYD_F5 },     
	{M(KEY_F6), KEYD_F6 },     
	{M(KEY_F7), KEYD_F7 },     
	{M(KEY_F8), KEYD_F8 },     
	{M(KEY_F9), KEYD_F9  },     
	{M(KEY_F10), KEYD_F10 },    
	{M(KEY_F11), KEYD_F11 },    
	{M(KEY_F12), KEYD_F12 }    
};

// Some of these mapped to key + shift.
static const keycode_numlock_t keycode_numlock[] = {
	{M(KEYPAD_SLASH), 	'/', '/'},
	{M(KEYPAD_ASTERIX), '*', '*'},
	{M(KEYPAD_MINUS),	'-', '-'},
	{M(KEYPAD_PLUS), 	'+', '+'},
	{M(KEYPAD_ENTER), 	'\n', '\n'},
	{M(KEYPAD_1), 		0x80 | M(KEY_END), '1'},
	{M(KEYPAD_2), 		0x80 | M(KEY_DOWN), '2'},
	{M(KEYPAD_3), 		0x80 | M(KEY_PAGE_DOWN), '3'},
	{M(KEYPAD_4), 		0x80 | M(KEY_LEFT), '4'},
	{M(KEYPAD_5), 		0x00, '5'},
	{M(KEYPAD_6), 		0x80 | M(KEY_RIGHT), '6'},
	{M(KEYPAD_7), 		0x80 | M(KEY_HOME),  '7'},
	{M(KEYPAD_8), 		0x80 | M(KEY_UP),  '8'},
	{M(KEYPAD_9), 		0x80 | M(KEY_PAGE_UP), '9'},
	{M(KEYPAD_0), 		0x80 | M(KEY_INSERT), '0'},
	{M(KEYPAD_PERIOD), 	0x80 | M(KEY_DELETE), '.'}
};

static const keyboard_force_boot_protocol_t keyboard_forceBootMode[] = {
	{0x04D9, 0}
};


#define print   USBHost::print_
#define println USBHost::println_




void KeyboardController::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
	driver_ready_for_device(this);
	USBHIDParser::driver_ready_for_hid_collection(this);
	BluetoothController::driver_ready_for_bluetooth(this);
	force_boot_protocol = false;	// start off assuming not
}

bool KeyboardController::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	println("KeyboardController claim this=", (uint32_t)this, HEX);

	// only claim at interface level
	if (type != 1) return false;
	if (len < 9+9+7) return false;
	print_hexbytes(descriptors, len);

	uint32_t numendpoint = descriptors[4];
	if (numendpoint < 1) return false;
	if (descriptors[5] != 3) return false; // bInterfaceClass, 3 = HID
	if (descriptors[6] != 1) return false; // bInterfaceSubClass, 1 = Boot Device
	if (descriptors[7] != 1) return false; // bInterfaceProtocol, 1 = Keyboard
	if (descriptors[9] != 9) return false;
	if (descriptors[10] != 33) return false; // HID descriptor (ignored, Boot Protocol)
	if (descriptors[18] != 7) return false;
	if (descriptors[19] != 5) return false; // endpoint descriptor
	uint32_t endpoint = descriptors[20];
	println("ep = ", endpoint, HEX);
	if ((endpoint & 0xF0) != 0x80) return false; // must be IN direction
	endpoint &= 0x0F;
	if (endpoint == 0) return false;
	if (descriptors[21] != 3) return false; // must be interrupt type
	uint32_t size = descriptors[22] | (descriptors[23] << 8);
	println("packet size = ", size);
	if ((size < 8) || (size > 64)) {
		return false; // Keyboard Boot Protocol is 8 bytes, but maybe others have longer... 
	}
#ifdef USBHS_KEYBOARD_INTERVAL 
	uint32_t interval = USBHS_KEYBOARD_INTERVAL;
#else
	uint32_t interval = descriptors[24];
#endif
	println("polling interval = ", interval);
	datapipe = new_Pipe(dev, 3, endpoint, 1, 8, interval);
	datapipe->callback_function = callback;
	queue_Data_Transfer(datapipe, report, 8, this);

	// see if this device in list of devices that need to be set in
	// boot protocol mode
	bool in_forceBoot_mode_list = false;
	for (uint8_t i = 0; i < sizeof(keyboard_forceBootMode)/sizeof(keyboard_forceBootMode[0]); i++) {
		if (dev->idVendor == keyboard_forceBootMode[i].idVendor) {
			if ((dev->idProduct == keyboard_forceBootMode[i].idProduct) ||
					(keyboard_forceBootMode[i].idProduct == 0)) {
				in_forceBoot_mode_list = true;
				break;
			}
		}
	}
	if (in_forceBoot_mode_list) {
		println("SET_PROTOCOL Boot");
		mk_setup(setup, 0x21, 11, 0, 0, 0); // 11=SET_PROTOCOL  BOOT
	} else {
		mk_setup(setup, 0x21, 10, 0, 0, 0); // 10=SET_IDLE
	}
	queue_Control_Transfer(dev, &setup, NULL, this);
	control_queued = true;
	return true;
}

void KeyboardController::control(const Transfer_t *transfer)
{
	println("control callback (keyboard)");
	control_queued = false;
	print_hexbytes(transfer->buffer, transfer->length);
	// To decode hex dump to human readable HID report summary:
	//   http://eleccelerator.com/usbdescreqparser/
	uint32_t mesg = transfer->setup.word1;
	println("  mesg = ", mesg, HEX);
	if (mesg == 0x00B21 && transfer->length == 0) { // SET_PROTOCOL
		mk_setup(setup, 0x21, 10, 0, 0, 0); // 10=SET_IDLE
		control_queued = true;
		queue_Control_Transfer(device, &setup, NULL, this);
	} else if (force_boot_protocol) {
		forceBootProtocol();	// lets setup to do the boot protocol
		force_boot_protocol = false;	// turn back off
	}
}

void KeyboardController::callback(const Transfer_t *transfer)
{
	//println("KeyboardController Callback (static)");
	if (transfer->driver) {
		((KeyboardController *)(transfer->driver))->new_data(transfer);
	}
}

void KeyboardController::forceBootProtocol()
{
	if (device && !control_queued) {
		mk_setup(setup, 0x21, 11, 0, 0, 0); // 11=SET_PROTOCOL  BOOT
		control_queued = true;
		queue_Control_Transfer(device, &setup, NULL, this);		
	} else {
		force_boot_protocol = true;	// let system know we want to force this.
	}
}

void KeyboardController::disconnect()
{
	// TODO: free resources
}


// Arduino defined this static weak symbol callback, and their
// examples use it as the only way to detect new key presses,
// so unfortunate as static weak callbacks are, it probably
// needs to be supported for compatibility
extern "C" {
void __keyboardControllerEmptyCallback() { }
}
void keyPressed()  __attribute__ ((weak, alias("__keyboardControllerEmptyCallback")));
void keyReleased() __attribute__ ((weak, alias("__keyboardControllerEmptyCallback")));

static bool contains(uint8_t b, const uint8_t *data)
{
	if (data[2] == b || data[3] == b || data[4] == b) return true;
	if (data[5] == b || data[6] == b || data[7] == b) return true;
	return false;
}

void KeyboardController::new_data(const Transfer_t *transfer)
{
	println("KeyboardController Callback (member)");
	print("  KB Data: ");
	print_hexbytes(transfer->buffer, 8);
	for (int i=2; i < 8; i++) {
		uint32_t key = prev_report[i];
		if (key >= 4 && !contains(key, report)) {
			key_release(prev_report[0], key);
			if (rawKeyReleasedFunction) {
				rawKeyReleasedFunction(key);
			}
		}
	}
	if (rawKeyReleasedFunction) {
		// each modifier key is represented by a bit in the first byte
		for (int i = 0; i < 8; ++i)
		{
			uint8_t keybit = 1 << i;
			if ((prev_report[0] & keybit) && !(report[0] & keybit)) {
				rawKeyReleasedFunction(103 + i);
			}
		}
	}
	for (int i=2; i < 8; i++) {
		uint32_t key = report[i];
		if (key >= 4 && !contains(key, prev_report)) {
			key_press(report[0], key);
			if (rawKeyPressedFunction) {
				rawKeyPressedFunction(key);
			}
		}
	}
	if (rawKeyPressedFunction) {
		for (int i = 0; i < 8; ++i)
		{
			uint8_t keybit = 1 << i;
			if (!(prev_report[0] & keybit) && (report[0] & keybit)) {
				rawKeyPressedFunction(103 + i);
			}
		}
	}
	memcpy(prev_report, report, 8);
	queue_Data_Transfer(datapipe, report, 8, this);
}


void KeyboardController::numLock(bool f) {
	if (leds_.numLock != f) {
		leds_.numLock = f;
		updateLEDS();
	}
}

void KeyboardController::capsLock(bool f) {
	if (leds_.capsLock != f) {
		leds_.capsLock = f;
		updateLEDS();
	}
}

void KeyboardController::scrollLock(bool f) {
	if (leds_.scrollLock != f) {
		leds_.scrollLock = f;
		updateLEDS();
	}
}

void KeyboardController::key_press(uint32_t mod, uint32_t key)
{
	// TODO: queue events, perform callback from Task
	println("  press, key=", key);
	modifiers = mod;
	keyOEM = key;
	keyCode = convert_to_unicode(mod, key);
	println("  unicode = ", keyCode);
	if (keyPressedFunction) {
		keyPressedFunction(keyCode);
	} else {
		keyPressed();
	}
}

void KeyboardController::key_release(uint32_t mod, uint32_t key)
{
	// TODO: queue events, perform callback from Task
	println("  release, key=", key);
	modifiers = mod;
	keyOEM = key;

	// Look for modifier keys
	if (key == M(KEY_NUM_LOCK)) {
		numLock(!leds_.numLock);
		// Lets toggle Numlock
	} else if (key == M(KEY_CAPS_LOCK)) {
		capsLock(!leds_.capsLock);

	} else if (key == M(KEY_SCROLL_LOCK)) {
		scrollLock(!leds_.scrollLock);
	} else {
		keyCode = convert_to_unicode(mod, key);
		if (keyReleasedFunction) {
			keyReleasedFunction(keyCode);
		} else {
			keyReleased();
		}
	}
}

uint16_t KeyboardController::convert_to_unicode(uint32_t mod, uint32_t key)
{
	// WIP: special keys
	// TODO: dead key sequences


	if (key & SHIFT_MASK) {
		// Many of these keys will look like they are other keys with shift mask...
		// Check for any of our mapped extra keys
		for (uint8_t i = 0; i < (sizeof(keycode_numlock)/sizeof(keycode_numlock[0])); i++) {
			if (keycode_numlock[i].code == key) {
				// See if the user is using numlock or not...
				if (leds_.numLock) {
					return keycode_numlock[i].charNumlockOn;
				} else {
					key = keycode_numlock[i].codeNumlockOff;
					if (!(key & 0x80)) return key;	// we have hard coded value
					key &= 0x7f;	// mask off the extra and break out to process as other characters...
					break;
				}
			}
		}
	}

	// Check for any of our mapped extra keys - Done early as some of these keys are 
	// above and some below the SHIFT_MASK value
	for (uint8_t i = 0; i < (sizeof(keycode_extras)/sizeof(keycode_extras[0])); i++) {
		if (keycode_extras[i].code == key) {
			return keycode_extras[i].ascii;
		}
	}

	// If we made it here without doing something then return 0;
	if (key & SHIFT_MASK) return 0;

	if ((mod & 0x02) || (mod & 0x20)) key |= SHIFT_MASK;
	if (leds_.capsLock) key ^= SHIFT_MASK;		// Caps lock will switch the Shift;
	for (int i=0; i < 96; i++) {
		if (keycodes_ascii[i] == key) {
			if ((mod & 1) || (mod & 0x10)) return (i+32) & 0x1f;	// Control key is down
			return i + 32;
		}
	}


#ifdef ISO_8859_1_A0
	for (int i=0; i < 96; i++) {
		if (keycodes_iso_8859_1[i] == key) return i + 160;
	}
#endif
	return 0;
}

void KeyboardController::LEDS(uint8_t leds) {
	println("Keyboard setLEDS ", leds, HEX);
	leds_.byte = leds;
	updateLEDS();
}

void KeyboardController::updateLEDS() {
	// Now lets tell keyboard new state.
	if (device != nullptr) {
		// Only do it this way if we are a standard USB device
		mk_setup(setup, 0x21, 9, 0x200, 0, sizeof(leds_.byte)); // hopefully this sets leds
		queue_Control_Transfer(device, &setup, &leds_.byte, this);
	} else {
		// Bluetooth, need to setup back channel to Bluetooth controller. 
	}
}

//=============================================================================
// Keyboard Extras - Combined from other object
//=============================================================================

#define TOPUSAGE_SYS_CONTROL 	0x10080
#define TOPUSAGE_CONSUMER_CONTROL	0x0c0001

hidclaim_t KeyboardController::claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage)
{
	// Lets try to claim a few specific Keyboard related collection/reports
	//USBHDBGSerial.printf("KBH Claim %x\n", topusage);
	if ((topusage != TOPUSAGE_SYS_CONTROL) 
		&& (topusage != TOPUSAGE_CONSUMER_CONTROL)
		) return CLAIM_NO;
	// only claim from one physical device
	//USBHDBGSerial.println("KeyboardController claim collection");
	// Lets only claim if this is the same device as claimed Keyboard... 
	if (dev != device) return CLAIM_NO;
	if (mydevice != NULL && dev != mydevice) return CLAIM_NO;
	mydevice = dev;
	collections_claimed_++;
	return CLAIM_REPORT;
}

void KeyboardController::disconnect_collection(Device_t *dev)
{
	if (--collections_claimed_ == 0) {
		mydevice = NULL;
	}
}

void KeyboardController::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax)
{
	//USBHDBGSerial.printf("KPC:hid_input_begin TUSE: %x TYPE: %x Range:%x %x\n", topusage, type, lgmin, lgmax);
	topusage_ = topusage;	// remember which report we are processing. 
	hid_input_begin_ = true;
	hid_input_data_ = false;
}

void KeyboardController::hid_input_data(uint32_t usage, int32_t value)
{
	// Hack ignore 0xff00 high words as these are user values... 
	if ((usage & 0xffff0000) == 0xff000000) return; 
	//USBHDBGSerial.printf("KeyboardController: topusage= %x usage=%X, value=%d\n", topusage_, usage, value);

	// See if the value is in our keys_down list
	usage &= 0xffff;		// only keep the actual key
	if (usage == 0) return;	// lets not process 0, if only 0 happens, we will handle it on the end to remove existing pressed items.

	// Remember if we have received any logical key up events.  Some keyboard appear to send them
	// others do no...
	hid_input_data_ = true;

	uint8_t key_index;
	for (key_index = 0; key_index < count_keys_down_; key_index++) {
		if (keys_down[key_index] == usage) {
			if (value) return;		// still down

			if (extrasKeyReleasedFunction) {
				extrasKeyReleasedFunction(topusage_, usage);
			}

			// Remove from list
			count_keys_down_--;
			for (;key_index < count_keys_down_; key_index++) {
				keys_down[key_index] = keys_down[key_index+1];
			}
			return;
		}
	}
	// Was not in list
	if (!value) return;	// still 0
	if (extrasKeyPressedFunction) {
		extrasKeyPressedFunction(topusage_, usage);
	}
	if (count_keys_down_ < MAX_KEYS_DOWN) {
		keys_down[count_keys_down_++] = usage;
	}
}

void KeyboardController::hid_input_end()
{
	//USBHDBGSerial.println("KPC:hid_input_end");
	if (hid_input_begin_) {

		// See if we received any data from parser if not, assume all keys released... 
		if (!hid_input_data_ ) {
			if (extrasKeyReleasedFunction) {
				while (count_keys_down_) {
					count_keys_down_--;
					extrasKeyReleasedFunction(topusage_, keys_down[count_keys_down_]);
				}
			}
			count_keys_down_ = 0;
		}

		hid_input_begin_ = false;
	}		
}

bool KeyboardController::claim_bluetooth(BluetoothController *driver, uint32_t bluetooth_class, uint8_t *remoteName) 
{
	USBHDBGSerial.printf("Keyboard Controller::claim_bluetooth - Class %x\n", bluetooth_class);
	if ((((bluetooth_class & 0xff00) == 0x2500) || (((bluetooth_class & 0xff00) == 0x500))) && (bluetooth_class & 0x40)) {
		if (remoteName && (strncmp((const char *)remoteName, "PLAYSTATION(R)3", 15) == 0)) {
			USBHDBGSerial.printf("KeyboardController::claim_bluetooth Reject PS3 hack\n");
			return false;
		}
		USBHDBGSerial.printf("KeyboardController::claim_bluetooth TRUE\n");
		//btdevice = driver;
		return true;
	}
	return false;
}

bool KeyboardController::remoteNameComplete(const uint8_t *remoteName) 
{
	// Real Hack some PS3 controllers bluetoot class is keyboard... 
	if (strncmp((const char *)remoteName, "PLAYSTATION(R)3", 15) == 0) {
		USBHDBGSerial.printf("  KeyboardController::remoteNameComplete %s - Oops PS3 unclaim\n", remoteName);
		return false;
	}
	return true;
}




bool KeyboardController::process_bluetooth_HID_data(const uint8_t *data, uint16_t length) 
{
	// Example DATA from bluetooth keyboard:
	//                  0  1 2 3 4 5  6 7  8 910 1 2 3 4 5 6 7
	//                           LEN         D
	//BT rx2_data(18): 48 20 e 0 a 0 70 0 a1 1 2 0 0 0 0 0 0 0 
	//BT rx2_data(18): 48 20 e 0 a 0 70 0 a1 1 2 0 4 0 0 0 0 0 
	//BT rx2_data(18): 48 20 e 0 a 0 70 0 a1 1 2 0 0 0 0 0 0 0 
	// So Len=9 passed in data starting at report ID=1... 
	USBHDBGSerial.printf("KeyboardController::process_bluetooth_HID_data\n");
	if (data[0] != 1) return false;
	print("  KB Data: ");
	print_hexbytes(data, length);
	for (int i=2; i < length; i++) {
		uint32_t key = prev_report[i];
		if (key >= 4 && !contains(key, report)) {
			key_release(prev_report[0], key);
		}
	}
	for (int i=2; i < 8; i++) {
		uint32_t key = data[i];
		if (key >= 4 && !contains(key, prev_report)) {
			key_press(data[1], key);
		}
	}
	// Save away the data.. But shift down one byte... Don't need the report number
	memcpy(prev_report, &data[1], 8);
	return true;
}

void KeyboardController::release_bluetooth() 
{
	//btdevice = nullptr;
}

//*****************************************************************************
// Some simple query functions depend on which interface we are using...
//*****************************************************************************

uint16_t KeyboardController::idVendor() 
{
	if (device != nullptr) return device->idVendor;
	if (mydevice != nullptr) return mydevice->idVendor;
	if (btdevice != nullptr) return btdevice->idVendor;
	return 0;
}

uint16_t KeyboardController::idProduct() 
{
	if (device != nullptr) return device->idProduct;
	if (mydevice != nullptr) return mydevice->idProduct;
	if (btdevice != nullptr) return btdevice->idProduct;
	return 0;
}

const uint8_t *KeyboardController::manufacturer()
{
	if ((device != nullptr) && (device->strbuf != nullptr)) return &device->strbuf->buffer[device->strbuf->iStrings[strbuf_t::STR_ID_MAN]];
	if ((btdevice != nullptr) && (btdevice->strbuf != nullptr)) return &btdevice->strbuf->buffer[btdevice->strbuf->iStrings[strbuf_t::STR_ID_MAN]]; 
	if ((mydevice != nullptr) && (mydevice->strbuf != nullptr)) return &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_MAN]]; 
	return nullptr;
}

const uint8_t *KeyboardController::product()
{
	if ((device != nullptr) && (device->strbuf != nullptr)) return &device->strbuf->buffer[device->strbuf->iStrings[strbuf_t::STR_ID_PROD]];
	if ((mydevice != nullptr) && (mydevice->strbuf != nullptr)) return &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_PROD]]; 
	if ((btdevice != nullptr) && (btdevice->strbuf != nullptr)) return &btdevice->strbuf->buffer[btdevice->strbuf->iStrings[strbuf_t::STR_ID_PROD]]; 
	return nullptr;
}

const uint8_t *KeyboardController::serialNumber()
{
	if ((device != nullptr) && (device->strbuf != nullptr)) return &device->strbuf->buffer[device->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]];
	if ((mydevice != nullptr) && (mydevice->strbuf != nullptr)) return &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]]; 
	if ((btdevice != nullptr) && (btdevice->strbuf != nullptr)) return &btdevice->strbuf->buffer[btdevice->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]]; 
	return nullptr;
}

