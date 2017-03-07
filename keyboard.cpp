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
	if (size != 8) return false; // must be 8 bytes for Keyboard Boot Protocol
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
	keyCode = convert_to_unicode(mod, key);
	if (keyReleasedFunction) {
		keyReleasedFunction(keyCode);
	} else {
		keyReleased();
	}
}

uint16_t KeyboardController::convert_to_unicode(uint32_t mod, uint32_t key)
{
	// TODO: special keys
	// TODO: caps lock
	// TODO: dead key sequences
	if ((mod & 0x02) || (mod & 0x20)) key |= SHIFT_MASK;
	for (int i=0; i < 96; i++) {
		if (keycodes_ascii[i] == key) return i + 32;
	}
#ifdef ISO_8859_1_A0
	for (int i=0; i < 96; i++) {
		if (keycodes_iso_8859_1[i] == key) return i + 160;
	}
#endif
	return 0;
}












