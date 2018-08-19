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

#ifdef M
#undef M
#endif
#define M(n) ((n) & KEYCODE_MASK)

keycode_extra_t keycode_extras[] = {
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
keycode_numlock_t keycode_numlock[] = {
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

#define print   USBHost::print_
#define println USBHost::println_

void KeyboardController::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
	driver_ready_for_device(this);
	USBHIDParser::driver_ready_for_hid_collection(this);
}

bool KeyboardController::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	println("KeyboardController claim this=", (uint32_t)this, HEX);

	// only claim at interface level
	if (type != 1) return false;
	if (len < 9+9+7) return false;

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

	mk_setup(setup, 0x21, 11, 0, 0, 0); // 11=SET_PROTOCOL  BOOT
	queue_Control_Transfer(dev, &setup, NULL, this);
	return true;
}

void KeyboardController::control(const Transfer_t *transfer)
{
	println("control callback (keyboard)");
	print_hexbytes(transfer->buffer, transfer->length);
	// To decode hex dump to human readable HID report summary:
	//   http://eleccelerator.com/usbdescreqparser/
	uint32_t mesg = transfer->setup.word1;
	println("  mesg = ", mesg, HEX);
	if (mesg == 0x001021 && transfer->length == 0) { // SET_PROTOCOL
		mk_setup(setup, 0x21, 10, 0, 0, 0); // 10=SET_IDLE
		queue_Control_Transfer(device, &setup, NULL, this);
	}
}

void KeyboardController::callback(const Transfer_t *transfer)
{
	//println("KeyboardController Callback (static)");
	if (transfer->driver) {
		((KeyboardController *)(transfer->driver))->new_data(transfer);
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
		}
	}
	for (int i=2; i < 8; i++) {
		uint32_t key = report[i];
		if (key >= 4 && !contains(key, prev_report)) {
			key_press(report[0], key);
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
	mk_setup(setup, 0x21, 9, 0x200, 0, sizeof(leds_.byte)); // hopefully this sets leds
	queue_Control_Transfer(device, &setup, &leds_.byte, this);
}

//=============================================================================
// Keyboard Extras - Combined from other object
//=============================================================================

#define TOPUSAGE_SYS_CONTROL 	0x10080
#define TOPUSAGE_CONSUMER_CONTROL	0x0c0001

hidclaim_t KeyboardController::claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage)
{
	// Lets try to claim a few specific Keyboard related collection/reports
	//Serial.printf("KBH Claim %x\n", topusage);
	if ((topusage != TOPUSAGE_SYS_CONTROL) 
		&& (topusage != TOPUSAGE_CONSUMER_CONTROL)
		) return CLAIM_NO;
	// only claim from one physical device
	//Serial.println("KeyboardController claim collection");
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
	//Serial.printf("KPC:hid_input_begin TUSE: %x TYPE: %x Range:%x %x\n", topusage, type, lgmin, lgmax);
	topusage_ = topusage;	// remember which report we are processing. 
	hid_input_begin_ = true;
	hid_input_data_ = false;
}

void KeyboardController::hid_input_data(uint32_t usage, int32_t value)
{
	// Hack ignore 0xff00 high words as these are user values... 
	if ((usage & 0xffff0000) == 0xff000000) return; 
	//Serial.printf("KeyboardController: topusage= %x usage=%X, value=%d\n", topusage_, usage, value);

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
	//Serial.println("KPC:hid_input_end");
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
