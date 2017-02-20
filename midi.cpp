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
#include "USBHost.h"


MIDIDevice::MIDIDevice()
{
	// TODO: free Device_t, Pipe_t & Transfer_t we will need
	driver_ready_for_device(this);
}

// Audio Class-Specific Descriptor Types (audio 1.0, page 99)
//   CS_UNDEFINED     0x20
//   CS_DEVICE        0x21
//   CS_CONFIGURATION 0x22
//   CS_STRING        0x23
//   CS_INTERFACE     0x24
//   CS_ENDPOINT      0x25
// MS Class-Specific Interface Descriptor Subtypes (midi 1.0, page 36)
//   MS_DESCRIPTOR_UNDEFINED 0x00
//   MS_HEADER               0x01
//   MIDI_IN_JACK            0x02
//   MIDI_OUT_JACK           0x03
//   ELEMENT                 0x04
// MS Class-Specific Endpoint Descriptor Subtypes (midi 1.0, page 36)
//   DESCRIPTOR_UNDEFINED 0x00
//   MS_GENERAL           0x01
// MS MIDI IN and OUT Jack types (midi 1.0, page 36)
//   JACK_TYPE_UNDEFINED 0x00
//   EMBEDDED            0x01
//   EXTERNAL            0x02
// Endpoint Control Selectors (midi 1.0, page 36)
//   EP_CONTROL_UNDEFINED 0x00
//   ASSOCIATION_CONTROL  0x01

bool MIDIDevice::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{

	// only claim at interface level
	if (type != 1) return false;
	println("MIDIDevice claim this=", (uint32_t)this, HEX);
	println("len = ", len);

	const uint8_t *p = descriptors;
	const uint8_t *end = p + len;

	if (p[0] != 9 || p[1] != 4) return false; // interface descriptor
	//println("  bInterfaceClass=", p[5]);
	//println("  bInterfaceSubClass=", p[6]);
	if (p[5] != 1) return false; // bInterfaceClass: 1 = Audio class
	if (p[6] != 3) return false; // bInterfaceSubClass: 3 = MIDI
	p += 9;
	println("  Interface is MIDI");
	rx_ep = 0;
	tx_ep = 0;

	while (p < end) {
		len = *p;
		if (len < 4) return false; // all audio desc are at least 4 bytes
		if (p + len > end) return false; // reject if beyond end of data
		uint32_t type = p[1];
		//println("type: ", type);
		if (type == 4 || type == 11) break; // interface or IAD, not for us
		if (type == 0x24) {  // 0x24 = Audio CS_INTERFACE, audio 1.0, page 99
			uint32_t subtype = p[2];
			//println("subtype: ", subtype);
			if (subtype == 1) {
				// Interface Header, midi 1.0, page 21
				println("    MIDI Header (ignored)");
			} else if (subtype == 2) {
				// MIDI IN Jack, midi 1.0, page 22
				println("    MIDI IN Jack (ignored)");
			} else if (subtype == 3) {
				// MIDI OUT Jack, midi 1.0, page 22
				println("    MIDI OUT Jack (ignored)");
			} else if (subtype == 4) {
				// Element Descriptor, midi 1.0, page 23-24
				println("    MIDI Element (ignored)");
			} else {
				return false; // unknown
			}
		} else if (type == 5) {
			// endpoint descriptor
			if (p[0] < 7) return false; // at least 7 bytes
			if (p[3] != 2) return false; // must be bulk type
			println("    MIDI Endpoint: ", p[2], HEX);
			switch (p[2] & 0xF0) {
			case 0x80:
				// IN endpoint
				if (rx_ep == 0) {
					rx_ep = p[2] & 0x0F;
					rx_size = p[4] | (p[5] << 8);
					println("      rx_size = ", rx_size);
				}
				break;
			case 0x00:
				// OUT endpoint
				if (tx_ep == 0) {
					tx_ep = p[2];
					tx_size = p[4] | (p[5] << 8);
					println("      tx_size = ", tx_size);
				}
				break;
			default:
				return false;
			}
		} else if (type == 37) {
			// MIDI endpoint info, midi 1.0: 6.2.2, page 26
			println("    MIDI Endpoint Jack Association (ignored)");
		} else {
			return false; // unknown
		}
		p += len;
	}
	// if an IN endpoint was found, create its pipe
	if (rx_ep && rx_size <= BUFFERSIZE) {
		rxpipe = new_Pipe(dev, 2, rx_ep, 1, rx_size);
		if (rxpipe) {
			rxpipe->callback_function = rx_callback;
			queue_Data_Transfer(rxpipe, buffer, rx_size, this);
		}
	} else {
		rxpipe = NULL;
	}
	// if an OUT endpoint was found, create its pipe
	if (tx_ep && tx_size <= BUFFERSIZE) {
		txpipe = new_Pipe(dev, 2, tx_ep, 0, tx_size);
		if (txpipe) {
			txpipe->callback_function = tx_callback;
		}
	} else {
		rxpipe = NULL;
	}
	// claim if either pipe created
	return (rxpipe || txpipe);
}

void MIDIDevice::rx_callback(const Transfer_t *transfer)
{
	if (transfer->driver) {
		((MIDIDevice *)(transfer->driver))->rx_data(transfer);
	}
}

void MIDIDevice::tx_callback(const Transfer_t *transfer)
{
	if (transfer->driver) {
		((MIDIDevice *)(transfer->driver))->tx_data(transfer);
	}
}

void MIDIDevice::rx_data(const Transfer_t *transfer)
{
	println("MIDIDevice Receive");
	print("  MIDI Data: ");
	print_hexbytes(transfer->buffer, rx_size);
	// TODO: parse the new data
	queue_Data_Transfer(rxpipe, buffer, rx_size, this);
}

void MIDIDevice::tx_data(const Transfer_t *transfer)
{
	println("MIDIDevice transmit complete");
	print("  MIDI Data: ");
	print_hexbytes(transfer->buffer, tx_size);
	// TODO: return the buffer to the pool...
}


void MIDIDevice::disconnect()
{
	// TODO: free resources
}



