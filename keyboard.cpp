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
	{M(KEY_TAB), 0x9 }
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



void KeyboardController::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	driver_ready_for_device(this);
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
	if (size != 8) {
		return false; // must be 8 bytes for Keyboard Boot Protocol
	}
	uint32_t interval = descriptors[24];
	println("polling interval = ", interval);
	datapipe = new_Pipe(dev, 3, endpoint, 1, 8, interval);
	datapipe->callback_function = callback;
	queue_Data_Transfer(datapipe, report, 8, this);
	mk_setup(setup, 0x21, 10, 0, 0, 0); // 10=SET_IDLE
	queue_Control_Transfer(dev, &setup, NULL, this);
	return true;
}

void KeyboardController::control(const Transfer_t *transfer)
{
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
	processing_new_data_ = true;
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
	processing_new_data_ = false;

	// See if we have any outstanding leds to update
	if (update_leds_) {
		updateLEDS();
	}
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
	// TODO: special keys
	// TODO: caps lock
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

		// If we made it here without doing something then return 0;
		if (key & SHIFT_MASK) return 0;
	}

	if ((mod & 0x02) || (mod & 0x20)) key |= SHIFT_MASK;
	if (leds_.capsLock) key ^= SHIFT_MASK;		// Caps lock will switch the Shift;
	for (int i=0; i < 96; i++) {
		if (keycodes_ascii[i] == key) {
			if ((mod & 1) || (mod & 0x10)) return (i+32) & 0x1f;	// Control key is down
			return i + 32;
		}
	}

	// Check for any of our mapped extra keys
	for (uint8_t i = 0; i < (sizeof(keycode_extras)/sizeof(keycode_extras[0])); i++) {
		if (keycode_extras[i].code == key) {
			return keycode_extras[i].ascii;
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
	println("KBD: Update LEDS", leds_.byte, HEX);
	if (processing_new_data_) {
		println("  Update defered");
		update_leds_ = true;
		return; 	// defer until later
	}

	// Now lets tell keyboard new state.
	static uint8_t keyboard_keys_report[1] = {0};
	setup_t keys_setup;
	keyboard_keys_report[0] = leds_.byte;
	queue_Data_Transfer(datapipe, report, 8, this);
	mk_setup(keys_setup, 0x21, 9, 0x200, 0, sizeof(keyboard_keys_report)); // hopefully this sets leds
	queue_Control_Transfer(device, &keys_setup, keyboard_keys_report, this);

	update_leds_ = false;
}











