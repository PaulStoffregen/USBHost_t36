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

// Printing of specific data structures.  When this is enabled,
// a tremendous amount of debug printing occurs.  It's done all
// from interrupt context, so this should never normally be
// enabled for regular programs that print from the Arduino sketch.

#ifdef USBHOST_PRINT_DEBUG

void USBHost::print_(const Transfer_t *transfer)
{
	if (!((uint32_t)transfer & 0xFFFFFFE0)) return;
	Serial.print("Transfer @ ");
	Serial.println(((uint32_t)transfer & 0xFFFFFFE0), HEX);
	Serial.print("   next:  ");
	Serial.println(transfer->qtd.next, HEX);
	Serial.print("   anext: ");
	Serial.println(transfer->qtd.alt_next, HEX);
	Serial.print("   token: ");
	Serial.println(transfer->qtd.token, HEX);
	Serial.print("   bufs:  ");
	for (int i=0; i < 5; i++) {
		Serial.print(transfer->qtd.buffer[i], HEX);
		if (i < 4) Serial.print(',');
	}
	Serial.println();
}

void USBHost::print_(const Transfer_t *first, const Transfer_t *last)
{
	Serial.print("Transfer Followup List ");
	Serial.print((uint32_t)first, HEX);
	Serial.print(" to ");
	Serial.println((uint32_t)last, HEX);
	Serial.println("    forward:");
	while (first) {
		Serial.print("    ");
		Serial.print((uint32_t)first, HEX);
		print_token(first->qtd.token);
		first = first->next_followup;
	}
	Serial.println("    backward:");
	while (last) {
		Serial.print("    ");
		Serial.print((uint32_t)last, HEX);
		print_token(last->qtd.token);
		last = last->prev_followup;
	}
}

void USBHost::print_token(uint32_t token)
{
	switch ((token >> 8) & 3) {
	case 0:
		Serial.print(" OUT ");
		Serial.println((token >> 16) & 0x7FFF);
		break;
	case 1:
		Serial.print(" IN ");
		Serial.println((token >> 16) & 0x7FFF);
		break;
	case 2:
		Serial.println(" SETUP");
		break;
	default:
		Serial.println(" unknown");
	}
}

void USBHost::print_(const Pipe_t *pipe)
{
	if (!((uint32_t)pipe & 0xFFFFFFE0)) return;
	Serial.print("Pipe ");
	if (pipe->type == 0) Serial.print("control");
	else if (pipe->type == 1) Serial.print("isochronous");
	else if (pipe->type == 2) Serial.print("bulk");
	else if (pipe->type == 3) Serial.print("interrupt");
	Serial.print(pipe->direction ? " IN" : " OUT");
	Serial.print("  @ ");
	Serial.println((uint32_t)pipe, HEX);
	Serial.print("  horiz link:  ");
	Serial.println(pipe->qh.horizontal_link, HEX);
	Serial.print("  capabilities: ");
	Serial.print(pipe->qh.capabilities[0], HEX);
	Serial.print(',');
	Serial.println(pipe->qh.capabilities[1], HEX);
	Serial.println("  overlay:");
	Serial.print("    cur:   ");
	Serial.println(pipe->qh.current, HEX);
	Serial.print("    next:  ");
	Serial.println(pipe->qh.next, HEX);
	Serial.print("    anext: ");
	Serial.println(pipe->qh.alt_next, HEX);
	Serial.print("    token: ");
	Serial.println(pipe->qh.token, HEX);
	Serial.print("    bufs:  ");
	for (int i=0; i < 5; i++) {
		Serial.print(pipe->qh.buffer[i], HEX);
		if (i < 4) Serial.print(',');
	}
	Serial.println();
	const Transfer_t *t = (Transfer_t *)pipe->qh.next;
	while (((uint32_t)t & 0xFFFFFFE0)) {
		print_(t);
		t = (Transfer_t *)t->qtd.next;
	}
	//Serial.print();
}

void USBHost::print_driverlist(const char *name, const USBDriver *driver)
{
	Serial.print("USBDriver (");
	Serial.print(name);
	Serial.print(") list: ");
	if (driver == NULL) {
		Serial.println("(empty");
		return;
	}
	uint32_t count=0;
	for (const USBDriver *p = driver; p; p = p->next) {
		Serial.print((uint32_t)p, HEX);
		if (p->next) Serial.print(" -> ");
		if (++count > 30) {
			Serial.println("abort:list too long");
			return;
		}
	}
	Serial.println();
}

void USBHost::print_qh_list(const Pipe_t *list)
{
	if (!list) {
		Serial.println("(empty)");
		return;
	}
	const Pipe_t *node = list;
	while (1) {
		Serial.print((uint32_t)node, HEX);
		node = (const Pipe_t *)(node->qh.horizontal_link & 0xFFFFFFE0);
		if (!node) break;
		if (node == list) {
			Serial.print(" (loops)");
			break;
		}
		Serial.print(" -> ");
	}
	Serial.println();
}

static void print_class_subclass_protocol(uint8_t c, uint8_t s, uint8_t p)
{
	Serial.print(c);
	if (c == 3) Serial.print("(HID)");
	if (c == 8) Serial.print("(Mass Storage)");
	if (c == 9) Serial.print("(Hub)");
	Serial.print(" / ");
	Serial.print(s);
	if (c == 3 && s == 1) Serial.print("(Boot)");
	if (c == 8 && s == 6) Serial.print("(SCSI)");
	Serial.print(" / ");
	Serial.print(p);
	if (c == 3 && s == 1 && p == 1) Serial.print("(Keyboard)");
	if (c == 3 && s == 1 && p == 2) Serial.print("(Mouse)");
	if (c == 8 && s == 6 && p == 0x50) Serial.print("(Bulk Only)");
	if (c == 8 && s == 6 && p == 0x62) Serial.print("(UAS)");
	if (c == 9 && s == 0 && p == 1) Serial.print("(Single-TT)");
	if (c == 9 && s == 0 && p == 2) Serial.print("(Multi-TT)");
	Serial.println();
}

void USBHost::print_device_descriptor(const uint8_t *p)
{
	Serial.println("Device Descriptor:");
	Serial.print("  ");
	print_hexbytes(p, p[0]);
	if (p[0] != 18) {
		Serial.println("error: device must be 18 bytes");
		return;
	}
	if (p[1] != 1) {
		Serial.println("error: device must type 1");
		return;
	}
	Serial.printf("    VendorID = %04X, ProductID = %04X, Version = %04X",
		p[8] | (p[9] << 8), p[10] | (p[11] << 8), p[12] | (p[13] << 8));
	Serial.println();
	Serial.print("    Class/Subclass/Protocol = ");
	print_class_subclass_protocol(p[4], p[5], p[6]);
	Serial.print("    Number of Configurations = ");
	Serial.println(p[17]);
}

void USBHost::print_config_descriptor(const uint8_t *p, uint32_t maxlen)
{
	// Descriptor Types: (USB 2.0, page 251)
	Serial.println("Configuration Descriptor:");
	Serial.print("  ");
	print_hexbytes(p, p[0]);
	if (p[0] != 9) {
		Serial.println("error: config must be 9 bytes");
		return;
	}
	if (p[1] != 2) {
		Serial.println("error: config must type 2");
		return;
	}
	Serial.print("    NumInterfaces = ");
	Serial.println(p[4]);
	Serial.print("    ConfigurationValue = ");
	Serial.println(p[5]);

	uint32_t len = p[2] | (p[3] << 8);
	if (len > maxlen) len = maxlen;
	len -= p[0];
	p += 9;

	while (len > 0) {
		if (p[0] > len) {
			Serial.print("  ");
			print_hexbytes(p, len);
			Serial.println("  error: length beyond total data size");
			break;
		}
		Serial.print("  ");
		print_hexbytes(p, p[0]);
		if (p[0] == 9 && p[1] == 4) { // Interface Descriptor
			Serial.print("    Interface = ");
			Serial.println(p[2]);
			Serial.print("    Number of endpoints = ");
			Serial.println(p[4]);
			Serial.print("    Class/Subclass/Protocol = ");
			print_class_subclass_protocol(p[5], p[6], p[7]);
		} else if (p[0] >= 7 && p[0] <= 9 && p[1] == 5) { // Endpoint Descriptor
			Serial.print("    Endpoint = ");
			Serial.print(p[2] & 15);
			Serial.println((p[2] & 128) ? " IN" : " OUT");
			Serial.print("    Type = ");
			switch (p[3] & 3) {
				case 0: Serial.println("Control"); break;
				case 1: Serial.println("Isochronous"); break;
				case 2: Serial.println("Bulk"); break;
				case 3: Serial.println("Interrupt"); break;
			}
			Serial.print("    Max Size = ");
			Serial.println(p[4] | (p[5] << 8));
			Serial.print("    Polling Interval = ");
			Serial.println(p[6]);
		} else if (p[0] == 8 && p[1] == 11) { // IAD
			Serial.print("    Interface Association = ");
			Serial.print(p[2]);
			Serial.print(" through ");
			Serial.println(p[2] + p[3] - 1);
			Serial.print("    Class / Subclass / Protocol = ");
			print_class_subclass_protocol(p[4], p[5], p[7]);
		} else if (p[0] >= 9 && p[1] == 0x21) { // HID
			Serial.print("    HID, ");
			Serial.print(p[5]);
			Serial.print(" report descriptor");
			if (p[5] != 1) Serial.print('s');
			Serial.println();
		}
		len -= p[0];
		p += p[0];
	}
}

void USBHost::print_string_descriptor(const char *name, const uint8_t *p)
{
	uint32_t len = p[0];
	if (len < 4) return;
	Serial.print(name);
	len -= 2;
	p += 2;
	while (len >= 2) {
		uint32_t c = p[0] | (p[1] << 8);
		if (c < 0x80) {
			Serial.write(c);
		} else if (c < 0x800) {
			Serial.write((c >> 6) | 0xC0);
			Serial.write((c & 0x3F) | 0x80);
		} else {
			Serial.write((c >> 12) | 0xE0);
			Serial.write(((c >> 6) & 0x3F) | 0x80);
			Serial.write((c & 0x3F) | 0x80);
		}
		len -= 2;
		p += 2;
	}
	Serial.println();
	//print_hexbytes(p, p[0]);
}


void USBHost::print_hexbytes(const void *ptr, uint32_t len)
{
	if (ptr == NULL || len == 0) return;
	const uint8_t *p = (const uint8_t *)ptr;
	do {
		if (*p < 16) Serial.print('0');
		Serial.print(*p++, HEX);
		Serial.print(' ');
	} while (--len);
	Serial.println();
}

#endif
