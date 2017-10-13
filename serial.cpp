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

// quick hack - ultimately USBHost print & println need to be renamed
#define print   USBHost::print
#define println USBHost::println

void USBSerial::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	driver_ready_for_device(this);
}

bool USBSerial::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	// only claim at interface level
	println("USBSerial claim this=", (uint32_t)this, HEX);
	print("vid=", dev->idVendor, HEX);
	println(", pid=", dev->idProduct, HEX);
	if (type == 0) {
		if (dev->idVendor == 0x0403 && dev->idProduct == 0x6001) {
			// FTDI FT232
			println("len = ", len);
			if (len < 23) return false;
			if (descriptors[0] != 9) return false; // length 9
			if (descriptors[9] != 7) return false; // length 7
			if (descriptors[10] != 5) return false; // ep desc
			uint32_t rxep = descriptors[11];
			if (descriptors[12] != 2) return false; // bulk type
			if (descriptors[13] != 64) return false; // size 64
			if (descriptors[14] != 0) return false;
			if (descriptors[16] != 7) return false; // length 7
			if (descriptors[17] != 5) return false; // ep desc
			uint32_t txep = descriptors[18];
			if (descriptors[19] != 2) return false; // bulk type
			if (descriptors[20] != 64) return false; // size 64
			if (descriptors[21] != 0) return false;
			if (!check_rxtx_ep(rxep, txep)) return false;
			print("FTDI, rxep=", rxep & 15);
			println(", txep=", txep);
			if (!init_buffers(64, 64)) return false;
			rxpipe = new_Pipe(dev, 2, rxep & 15, 1, 64);
			if (!rxpipe) return false;
			txpipe = new_Pipe(dev, 2, txep, 0, 64);
			if (!txpipe) {
				// TODO: free rxpipe
				return false;
			}
			ignore_first_two_bytes = true;
			rxpipe->callback_function = rx_callback;
			queue_Data_Transfer(rxpipe, rx1, 64, this);
			rxstate = 1;
			if (rxsize > 128) {
				queue_Data_Transfer(rxpipe, rx2, 64, this);
				rxstate = 3;
			}
			txpipe->callback_function = tx_callback;
			return true;
		}
	}
	return false;
}

// check if two legal endpoints, 1 receive & 1 transmit
bool USBSerial::check_rxtx_ep(uint32_t &rxep, uint32_t &txep)
{
	if ((rxep & 0x0F) == 0) return false;
	if ((txep & 0x0F) == 0) return false;
	uint32_t rxdir = rxep & 0xF0;
	uint32_t txdir = txep & 0xF0;
	if (rxdir == 0x80 && txdir == 0x00) {
		return true;
	}
	if (rxdir == 0x00 && txdir == 0x80) {
		std::swap(rxep, txep);
		return true;
	}
	return false;
}

// initialize buffer sizes and pointers
bool USBSerial::init_buffers(uint32_t rsize, uint32_t tsize)
{
	// buffer must be able to hold 2 of each packet, plus have room to
	if (sizeof(bigbuffer) < (rsize + tsize) * 3 + 2) return false;
	rx1 = (uint8_t *)bigbuffer;
	rx2 = rx1 + rsize;
	tx1 = rx2 + rsize;
	tx2 = tx1 + tsize;
	rxbuf = tx2 + tsize;
	// FIXME: this assume 50-50 split - not true when rsize != tsize
	rxsize = (sizeof(bigbuffer) - (rsize + tsize) * 2) / 2;
	txsize = rxsize;
	txbuf = rxbuf + rxsize;
	rxhead = 0;
	rxtail = 0;
	txhead = 0;
	txtail = 0;
	rxstate = 0;
	return true;
}







void USBSerial::rx_callback(const Transfer_t *transfer)
{
	if (!transfer->driver) return;
	((USBSerial *)(transfer->driver))->rx_data(transfer);
}

void USBSerial::tx_callback(const Transfer_t *transfer)
{
	if (!transfer->driver) return;
	((USBSerial *)(transfer->driver))->tx_data(transfer);
}

void USBSerial::rx_data(const Transfer_t *transfer)
{
	uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
	print("rx: ");
	print_hexbytes(transfer->buffer, len);

	// first update rxstate bitmask
	if (transfer->buffer == rx1) {
		rxstate &= 0xFE;
	} else if (transfer->buffer == rx2) {
		rxstate &= 0xFD;
	}
	// get start of data and actual length
	const uint8_t *p = (const uint8_t *)transfer->buffer;
	if (ignore_first_two_bytes) {
		if (len >= 2) {
			p += 2;
			len -= 2;
		} else {
			len = 0;
		}
	}
	// copy data from packet buffer to circular buffer
	if (len > 0) {
		// TODO: copy to buffer
	}
	// re-queue packet buffer(s) if possible
	uint32_t avail;
	uint32_t head = rxhead;
	uint32_t tail = rxtail;
	if (head >= tail) {
		avail = rxsize - 1 + (tail - head);
	} else {
		avail = (tail - head) - 1;
	}
	uint32_t packetsize = rx2 - rx1;
	if (avail >= packetsize) {
		if ((rxstate & 0x01) == 0) {
			queue_Data_Transfer(rxpipe, rx1, packetsize, this);
			rxstate |= 0x01;
		} else if ((rxstate & 0x02) == 0) {
			queue_Data_Transfer(rxpipe, rx2, packetsize, this);
			rxstate |= 0x02;
		}
		if ((rxstate & 0x03) != 0x03 && avail >= packetsize * 2) {
			if ((rxstate & 0x01) == 0) {
				queue_Data_Transfer(rxpipe, rx1, packetsize, this);
				rxstate |= 0x01;
			} else if ((rxstate & 0x02) == 0) {
				queue_Data_Transfer(rxpipe, rx2, packetsize, this);
				rxstate |= 0x02;
			}
		}
	}
}

void USBSerial::tx_data(const Transfer_t *transfer)
{
	println("tx: ");
}







void USBSerial::begin(uint32_t baud, uint32_t format)
{
}

void USBSerial::end(void)
{
}

int USBSerial::available(void)
{
	return 0;
}

int USBSerial::peek(void)
{
	return -1;
}

int USBSerial::read(void)
{
	return -1;
}

size_t USBSerial::write(uint8_t c)
{
	return 1;
}





























void USBSerial::disconnect()
{
}


