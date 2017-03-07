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


void MIDIDevice::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	handleNoteOff = NULL;
	handleNoteOn = NULL;
	handleVelocityChange = NULL;
	handleControlChange = NULL;
	handleProgramChange = NULL;
	handleAfterTouch = NULL;
	handlePitchChange = NULL;
	handleSysEx = NULL;
	handleRealTimeSystem = NULL;
	handleTimeCodeQuarterFrame = NULL;
	rx_head = 0;
	rx_tail = 0;
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
	if (rx_ep && rx_size <= MAX_PACKET_SIZE) {
		rxpipe = new_Pipe(dev, 2, rx_ep, 1, rx_size);
		if (rxpipe) {
			rxpipe->callback_function = rx_callback;
			queue_Data_Transfer(rxpipe, rx_buffer, rx_size, this);
			rx_packet_queued = true;
		}
	} else {
		rxpipe = NULL;
	}
	// if an OUT endpoint was found, create its pipe
	if (tx_ep && tx_size <= MAX_PACKET_SIZE) {
		txpipe = new_Pipe(dev, 2, tx_ep, 0, tx_size);
		if (txpipe) {
			txpipe->callback_function = tx_callback;
		}
	} else {
		txpipe = NULL;
	}
	rx_head = 0;
	rx_tail = 0;
	msg_channel = 0;
	msg_type = 0;
	msg_data1 = 0;
	msg_data2 = 0;
	msg_sysex_len = 0;
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
	uint32_t head = rx_head;
	uint32_t tail = rx_tail;
	uint32_t len = rx_size >> 2; // TODO: use actual received length
	for (uint32_t i=0; i < len; i++) {
		uint32_t msg = rx_buffer[i];
		if (msg) {
			if (++head >= RX_QUEUE_SIZE) head = 0;
			rx_queue[head] = msg;
		}
	}
	rx_head = head;
	rx_tail = tail;
	uint32_t avail = (head < tail) ? tail - head - 1 : RX_QUEUE_SIZE - 1 - head + tail;
	println("rx_size = ", rx_size);
	println("avail = ", avail);
	if (avail >= (uint32_t)(rx_size>>2)) {
		// enough space to accept another full packet
		println("queue another receive packet");
		queue_Data_Transfer(rxpipe, rx_buffer, rx_size, this);
		rx_packet_queued = true;
	} else {
		// queue can't accept another packet's data, so leave
		// the data waiting on the device until we can accept it
		println("wait to receive more packets");
		rx_packet_queued = false;
	}
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
	// should rx_queue be cleared?
	// as-is, the user can still read MIDI messages
	// which arrived before the device disconnected.
	rxpipe = NULL;
	txpipe = NULL;
}




bool MIDIDevice::read(uint8_t channel, uint8_t cable)
{
	uint32_t n, head, tail, avail, ch, type1, type2;

	head = rx_head;
	tail = rx_tail;
	if (head == tail) return false;
	if (++tail >= RX_QUEUE_SIZE) tail = 0;
	n = rx_queue[tail];
	rx_tail = tail;
	if (!rx_packet_queued && rxpipe) {
	        avail = (head < tail) ? tail - head - 1 : RX_QUEUE_SIZE - 1 - head + tail;
		if (avail >= (uint32_t)(rx_size>>2)) {
			__disable_irq();
			queue_Data_Transfer(rxpipe, rx_buffer, rx_size, this);
			__enable_irq();
		}
	}

	println("read: ", n, HEX);

	type1 = n & 15;
	type2 = (n >> 12) & 15;
	ch = ((n >> 8) & 15) + 1;
	if (type1 >= 0x08 && type1 <= 0x0E) {
		if (channel && channel != ch) {
			// ignore other channels when user wants single channel read
			return false;
		}
		if (type1 == 0x08 && type2 == 0x08) {
			msg_type = 8;			// 8 = Note off
			if (handleNoteOff)
				(*handleNoteOff)(ch, (n >> 16), (n >> 24));
		} else
		if (type1 == 0x09 && type2 == 0x09) {
			if ((n >> 24) > 0) {
				msg_type = 9;		// 9 = Note on
				if (handleNoteOn)
					(*handleNoteOn)(ch, (n >> 16), (n >> 24));
			} else {
				msg_type = 8;		// 8 = Note off
				if (handleNoteOff)
					(*handleNoteOff)(ch, (n >> 16), (n >> 24));
			}
		} else
		if (type1 == 0x0A && type2 == 0x0A) {
			msg_type = 10;			// 10 = Poly Pressure
			if (handleVelocityChange)
				(*handleVelocityChange)(ch, (n >> 16), (n >> 24));
		} else
			if (type1 == 0x0B && type2 == 0x0B) {
			msg_type = 11;			// 11 = Control Change
			if (handleControlChange)
				(*handleControlChange)(ch, (n >> 16), (n >> 24));
		} else
			if (type1 == 0x0C && type2 == 0x0C) {
			msg_type = 12;			// 12 = Program Change
				if (handleProgramChange) (*handleProgramChange)(ch, (n >> 16));
		} else
			if (type1 == 0x0D && type2 == 0x0D) {
			msg_type = 13;			// 13 = After Touch
				if (handleAfterTouch) (*handleAfterTouch)(ch, (n >> 16));
		} else
			if (type1 == 0x0E && type2 == 0x0E) {
			msg_type = 14;			// 14 = Pitch Bend
			if (handlePitchChange)
				(*handlePitchChange)(ch, ((n >> 16) & 0x7F) | ((n >> 17) & 0x3F80));
		} else {
			return false;
		}
		msg_channel = ch;
		msg_data1 = (n >> 16);
		msg_data2 = (n >> 24);
		return true;
	}
	if (type1 == 0x04) {
		sysex_byte(n >> 8);
		sysex_byte(n >> 16);
		sysex_byte(n >> 24);
		return false;
	}
	if (type1 >= 0x05 && type1 <= 0x07) {
		sysex_byte(n >> 8);
		if (type1 >= 0x06) sysex_byte(n >> 16);
		if (type1 == 0x07) sysex_byte(n >> 24);
		msg_data1 = msg_sysex_len;
		msg_sysex_len = 0;
		msg_type = 15;			// 15 = Sys Ex
		if (handleSysEx)
			(*handleSysEx)(msg_sysex, msg_data1, 1);
		return true;
	}
	// TODO: single byte messages
	// TODO: time code messages?
	return false;
}

void MIDIDevice::sysex_byte(uint8_t b)
{
	// when buffer is full, send another chunk to handler.
	if (msg_sysex_len >= SYSEX_MAX_LEN) {
		if (handleSysEx) {
			(*handleSysEx)(msg_sysex, msg_sysex_len, 0);
			msg_sysex_len = 0;
		}
	}
	if (msg_sysex_len < SYSEX_MAX_LEN) {
		msg_sysex[msg_sysex_len++] = b;
	}
}




