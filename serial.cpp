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

/************************************************************/
//  Initialization and claiming of devices & interfaces
/************************************************************/

void USBSerial::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
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
			sertype = FTDI;
			rxpipe->callback_function = rx_callback;
			queue_Data_Transfer(rxpipe, rx1, 64, this);
			rxstate = 1;
			if (rxsize > 128) {
				queue_Data_Transfer(rxpipe, rx2, 64, this);
				rxstate = 3;
			}
			txstate = 0;
			txpipe->callback_function = tx_callback;
			baudrate = 115200;
			pending_control = 0x0F;
			mk_setup(setup, 0x40, 0, 0, 0, 0); // reset port
			queue_Control_Transfer(dev, &setup, NULL, this);
			control_queued = true;
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

void USBSerial::disconnect()
{
}


/************************************************************/
//  Control Transfer For Configuration
/************************************************************/


void USBSerial::control(const Transfer_t *transfer)
{
	println("control callback (serial)");
	control_queued = false;

	// set data format
	if (pending_control & 1) {
		pending_control &= ~1;
		mk_setup(setup, 0x40, 4, 8, 0, 0); // data format 8N1
		queue_Control_Transfer(device, &setup, NULL, this);
		control_queued = true;
		return;
	}
	// set baud rate
	if (pending_control & 2) {
		pending_control &= ~2;
		uint32_t baudval = 3000000 / baudrate;
		mk_setup(setup, 0x40, 3, baudval, 0, 0);
		queue_Control_Transfer(device, &setup, NULL, this);
		control_queued = true;
		return;
	}
	// configure flow control
	if (pending_control & 4) {
		pending_control &= ~4;
		mk_setup(setup, 0x40, 2, 0, 0, 0);
		queue_Control_Transfer(device, &setup, NULL, this);
		control_queued = true;
		return;
	}
	// set DTR
	if (pending_control & 8) {
		pending_control &= ~8;
		mk_setup(setup, 0x40, 1, 0x0101, 0, 0);
		queue_Control_Transfer(device, &setup, NULL, this);
		control_queued = true;
		return;
	}
}


/************************************************************/
//  Interrupt-based Data Movement
/************************************************************/

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

	// first update rxstate bitmask, since buffer is no longer queued
	if (transfer->buffer == rx1) {
		rxstate &= 0xFE;
	} else if (transfer->buffer == rx2) {
		rxstate &= 0xFD;
	}
	// get start of data and actual length
	const uint8_t *p = (const uint8_t *)transfer->buffer;
	if (sertype == FTDI) {
		if (len >= 2) {
			p += 2;
			len -= 2;
		} else {
			len = 0;
		}
	}
	//if (len > 0) {
		//print("rx: ");
		//print_hexbytes(p, len);
	//}
	// Copy data from packet buffer to circular buffer.
	// Assume the buffer will always have space, since we
	// check before queuing the buffers
	uint32_t head = rxhead;
	uint32_t tail = rxtail;
	if (++head >= rxsize) head = 0;
	uint32_t avail;
	if (len > 0) {
		//print("head=", head);
		//print(", tail=", tail);
		avail = rxsize - head;
		//print(", avail=", avail);
		//println(", rxsize=", rxsize);
		if (avail > len) avail = len;
		memcpy(rxbuf + head, p, avail);
		if (len <= avail) {
			head += avail - 1;
			if (head >= rxsize) head = 0;
		} else {
			head = len - avail - 1;
			memcpy(rxbuf, p + avail, head + 1);
		}
		rxhead = head;
	}
	// TODO: can be this more efficient?  We know from above which
	// buffer is no longer queued, so possible skip most of this work?
	rx_queue_packets(head, tail);
}

// re-queue packet buffer(s) if possible
void USBSerial::rx_queue_packets(uint32_t head, uint32_t tail)
{
	uint32_t avail;
	if (head >= tail) {
		avail = rxsize - 1 - head + tail;
	} else {
		avail = tail - head - 1;
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
	uint32_t mask;
	uint8_t *p = (uint8_t *)transfer->buffer;
	if (p == tx1) {
		println("tx1:");
		mask = 1;
		//txstate &= 0xFE;
	} else if (p == tx2) {
		println("tx2:");
		mask = 2;
		//txstate &= 0xFD;
	} else {
		return; // should never happen
	}
	// check how much more data remains in the transmit buffer
	uint32_t head = txhead;
	uint32_t tail = txtail;
	uint32_t count;
	if (head >= tail) {
		count = head - tail;
	} else {
		count = txsize + head - tail;
	}
	uint32_t packetsize = tx2 - tx1;
	if (count < packetsize) {
		// not enough data in buffer to fill a full packet
		txstate &= ~mask;
		return;
	}
	// immediately transmit another full packet, if we have enough data
	println("TX:moar data!!!!");
	if (++tail >= txsize) tail = 0;
	uint32_t n = txsize - tail;
	if (n > packetsize) n = packetsize;
	memcpy(p, txbuf + tail, n);
	if (n >= packetsize) {
		tail += n - 1;
		if (tail >= txsize) tail = 0;
	} else {
		uint32_t len = packetsize - n;
		memcpy(p + n, txbuf, len);
		tail = len - 1;
	}
	txtail = tail;
	queue_Data_Transfer(txpipe, p, packetsize, this);
}


void USBSerial::timer_event(USBDriverTimer *whichTimer)
{
	println("txtimer");
	uint32_t count;
	uint32_t head = txhead;
	uint32_t tail = txtail;
	if (head == tail) {
		return; // nothing to transmit
	} else if (head > tail) {
		count = head - tail;
	} else {
		count = txsize + head - tail;
	}
	uint8_t *p;
	if ((txstate & 0x01) == 0) {
		p = tx1;
		txstate |= 0x01;
	} else if ((txstate & 0x02) == 0) {
		p = tx2;
		txstate |= 0x02;
	} else {
		txtimer.start(1200);
		return; // no outgoing buffers available, try again later
	}
	if (++tail >= txsize) tail = 0;
	uint32_t n = txsize - tail;
	if (n > count) n = count;
	memcpy(p, txbuf + tail, n);
	if (n >= count) {
		tail += n - 1;
		if (tail >= txsize) tail = 0;
	} else {
		uint32_t len = count - n;
		memcpy(p + n, txbuf, len);
		tail = len - 1;
	}
	txtail = tail;
	queue_Data_Transfer(txpipe, p, count, this);
}



/************************************************************/
//  User Functions - must disable USBHQ IRQ for EHCI access
/************************************************************/

void USBSerial::begin(uint32_t baud, uint32_t format)
{
	NVIC_DISABLE_IRQ(IRQ_USBHS);
	baudrate = baud;
	pending_control |= 2;
	if (!control_queued) control(NULL);
	NVIC_ENABLE_IRQ(IRQ_USBHS);
}

void USBSerial::end(void)
{
	// TODO: lower DTR
}

int USBSerial::available(void)
{
	if (!device) return 0;
	uint32_t head = rxhead;
	uint32_t tail = rxtail;
	if (head >= tail) return head - tail;
	return rxsize + head - tail;
}

int USBSerial::peek(void)
{
	if (!device) return -1;
	uint32_t head = rxhead;
	uint32_t tail = rxtail;
	if (head == tail) return -1;
	if (++tail >= rxsize) tail = 0;
	return rxbuf[tail];
}

int USBSerial::read(void)
{
	if (!device) return -1;
	uint32_t head = rxhead;
	uint32_t tail = rxtail;
	if (head == tail) return -1;
	if (++tail >= rxsize) tail = 0;
	int c = rxbuf[tail];
	rxtail = tail;
	if ((rxstate & 0x03) != 0x03) {
		NVIC_DISABLE_IRQ(IRQ_USBHS);
		rx_queue_packets(head, tail);
		NVIC_ENABLE_IRQ(IRQ_USBHS);
	}
	return c;
}

int USBSerial::availableForWrite()
{
	if (!device) return 0;
	uint32_t head = txhead;
	uint32_t tail = txtail;
	if (head >= tail) return txsize - 1 - head + tail;
	return tail - head - 1;
}

size_t USBSerial::write(uint8_t c)
{
	if (!device) return 0;
	uint32_t head = txhead;
	if (++head >= txsize) head = 0;
	while (txtail == head) {
		// wait...
	}
	txbuf[head] = c;
	txhead = head;
	//print("head=", head);
	//println(", tail=", txtail);

	// if full packet in buffer and tx packet ready, queue it
	NVIC_DISABLE_IRQ(IRQ_USBHS);
	uint32_t tail = txtail;
	if ((txstate & 0x03) != 0x03) {
		// at least one packet buffer is ready to transmit
		uint32_t count;
		if (head >= tail) {
			count = head - tail;
		} else {
			count = txsize + head - tail;
		}
		uint32_t packetsize = tx2 - tx1;
		if (count >= packetsize) {
			//println("txsize=", txsize);
			uint8_t *p;
			if ((txstate & 0x01) == 0) {
				p = tx1;
				txstate |= 0x01;
			} else /* if ((txstate & 0x02) == 0) */ {
				p = tx2;
				txstate |= 0x02;
			}
			// copy data to packet buffer
			if (++tail >= txsize) tail = 0;
			uint32_t n = txsize - tail;
			if (n > packetsize) n = packetsize;
			//print("memcpy, offset=", tail);
			//println(", len=", n);
			memcpy(p, txbuf + tail, n);
			if (n >= packetsize) {
				tail += n - 1;
				if (tail >= txsize) tail = 0;
			} else {
				//n = txsize - n;
				uint32_t len = packetsize - n;
				//println("memcpy, offset=0, len=", len);
				memcpy(p + n, txbuf, len);
				tail = len - 1;
			}
			txtail = tail;
			//println("queue tx packet, newtail=", tail);
			queue_Data_Transfer(txpipe, p, packetsize, this);
			NVIC_ENABLE_IRQ(IRQ_USBHS);
			return 1;
		}
	}
	// otherwise, set a latency timer to later transmit partial packet
	txtimer.stop();
	txtimer.start(3500);
	NVIC_ENABLE_IRQ(IRQ_USBHS);
	return 1;
}



