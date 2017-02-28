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

USBHub::USBHub() : /* mytimer(this), */ othertimer(this)
{
	// TODO: free Device_t, Pipe_t & Transfer_t we will need
	driver_ready_for_device(this);
}

bool USBHub::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	// only claim entire device, never at interface level
	if (type != 0) return false;

	println("USBHub memory usage = ", sizeof(USBHub));
	println("USBHub claim_device this=", (uint32_t)this, HEX);

	// timer testing  TODO: remove this later
	mytimer.init(this);
	mytimer.pointer = (void *)"This is mytimer";
	//mytimer.start(99129);
	othertimer.pointer = (void *)"Hello, I'm othertimer";
	//othertimer.start(12345);
	for (int i=0; i < 7; i++) {
		mytimers[i].init(this);
		//mytimers[i].start((i + 1) * 10000);
	}

	// check for HUB type
	if (dev->bDeviceClass != 9 || dev->bDeviceSubClass != 0) return false;
	// protocol must be 0=FS, 1=HS Single-TT, or 2=HS Multi-TT
	if (dev->bDeviceProtocol > 2) return false;
	// check for endpoint descriptor
	if (descriptors[9] != 7 || descriptors[10] != 5) return false;
	// endpoint must be IN direction
	if ((descriptors[11] & 0xF0) != 0x80) return false;
	// endpoint type must be interrupt
	if (descriptors[12] != 3) return false;
	// get the endpoint number, must not be zero
	endpoint = descriptors[11] & 0x0F;
	if (endpoint == 0) return false;
	// get the maximum packet size
	uint32_t maxsize = descriptors[13] | (descriptors[14] << 8);
	if (maxsize == 0) return false;
	if (maxsize > 1) return false; // do hub chips with > 7 ports exist?
	interval = descriptors[15];
	println("  polling interval = ", interval);
	println(descriptors[9]);
	println(descriptors[10]);
	println(descriptors[11], HEX);
	println(maxsize);
	// bDeviceProtocol = 0 is full speed
	// bDeviceProtocol = 1 is high speed single TT
	// bDeviceProtocol = 2 is high speed multiple TT

	println("bDeviceClass = ", dev->bDeviceClass);
	println("bDeviceSubClass = ", dev->bDeviceSubClass);
	println("bDeviceProtocol = ", dev->bDeviceProtocol);

	numports = 0; // unknown until hub descriptor is read
	changepipe = NULL;
	changebits = 0;
	memset(portstatus, 0, sizeof(portstatus));
	memset(portstate, 0, sizeof(portstate));

	mk_setup(setup[0], 0xA0, 6, 0x2900, 0, sizeof(hub_desc));
	queue_Control_Transfer(dev, &setup[0], hub_desc, this);

	return true;
}

void USBHub::timer_event(USBDriverTimer *timer)
{
	uint32_t us = micros() - timer->started_micros;
	print("timer event (");
	print(us);
	print(" us): ");
	print((char *)timer->pointer);
	print(", this = ");
	print((uint32_t)this, HEX);
	println(", timer = ", (uint32_t)timer, HEX);
}


void USBHub::send_poweron(uint32_t port)
{
	if (port == 0 || port > numports) return;
	mk_setup(setup[port], 0x23, 3, 8, port, 0);
	queue_Control_Transfer(device, &setup[port], NULL, this);
}

void USBHub::send_getstatus(uint32_t port)
{
	if (port > numports) return;
	println("getstatus, port = ", port);
	mk_setup(setup[port], ((port > 0) ? 0xA3 : 0xA0), 0, 0, port, 4);
	queue_Control_Transfer(device, &setup[port], &statusbits[port], this);
}

void USBHub::send_clearstatus(uint32_t port)
{
	if (port > numports) return;
	mk_setup(setup[port], ((port > 0) ? 0x23 : 0x20), 1, 0x10, port, 0);
	queue_Control_Transfer(device, &setup[port], NULL, this);
}

void USBHub::send_reset(uint32_t port)
{
	if (port == 0 || port > numports) return;
	mk_setup(setup[port], 0x23, 3, 4, port, 0); // set feature PORT_RESET
	queue_Control_Transfer(device, &setup[port], NULL, this);
}


void USBHub::control(const Transfer_t *transfer)
{
	println("USBHub control callback");
	print_hexbytes(transfer->buffer, transfer->length);

	uint32_t port = transfer->setup.wIndex;
	uint32_t mesg = transfer->setup.word1;

	switch (mesg) {
	  case 0x290006A0: // read hub descriptor
		numports = hub_desc[2];
		characteristics = hub_desc[3];
		powertime = hub_desc[5];
		// TODO: do we need to use the DeviceRemovable
		// bits to make synthetic device connect events?
		println("Hub ports = ", numports);
		for (uint32_t i=1; i <= numports; i++) {
			send_poweron(i);
		}
		break;
	  case 0x00080323: // power turned on
		if (port == numports && changepipe == NULL) {
			println("power turned on to all ports");
			println("device addr = ", device->address);
			changepipe = new_Pipe(device, 3, endpoint, 1, 1, interval);
			println("pipe cap1 = ", changepipe->qh.capabilities[0], HEX);
			changepipe->callback_function = callback;
			queue_Data_Transfer(changepipe, &changebits, 1, this);
		}
		break;

	  case 0x000000A0: // get hub status
		println("New Hub Status");
		send_clearstatus(0);
		break;
	  case 0x000000A3: // get port status
		println("New Port Status");
		if (transfer->length == 4) {
			uint32_t status = *(uint32_t *)(transfer->buffer);
			if (status != statusbits[port]) println("ERROR: status not same");
			new_port_status(port, status);
		}
		send_clearstatus(port);
		break;
	  case 0x00100120: // clear hub status
		println("Hub Status Cleared");
		break;
	  case 0x00100123: // clear port status
		println("Port Status Cleared, port=", port);
		break;
	  default:
		println("unhandled setup, message = ", mesg, HEX);
	}
}

void USBHub::callback(const Transfer_t *transfer)
{
	//println("HUB Callback (static)");
	if (transfer->driver) ((USBHub *)(transfer->driver))->status_change(transfer);
}

void USBHub::status_change(const Transfer_t *transfer)
{
	println("HUB Callback (member)");
	println("status = ", changebits, HEX);
	for (uint32_t i=0; i <= numports; i++) {
		if (changebits & (1 << i)) {
			send_getstatus(i);
		}
	}
	queue_Data_Transfer(changepipe, &changebits, 1, this);
}

void USBHub::new_port_status(uint32_t port, uint32_t status)
{
	if (port == 0 || port > numports) return;
	uint32_t priorstatus = portstatus[port - 1];
	portstatus[port] = status;

	print("  Status: port=");
	print(port);
	print(", status=");
	println(status, HEX);

	// status bits, USB 2.0: 11.24.2.7.1 page 427
	if (status & 0x0001) println("  Device is present: ");
	if (status & 0x0002) {
		println("  Enabled, speed = ");
		if (status & 0x0200) {
			print("1.5");
		} else {
			if (status & 0x0400) {
				print("480");
			} else {
				print("12");
			}
		}
		println(" Mbit/sec");
	}
	if (status & 0x0004) println("  Suspended");
	if (status & 0x0008) println("  Over-current");
	if (status & 0x0010) println("  Reset");
	if (status & 0x0100) println("  Has Power");
	if (status & 0x0800) println("  Test Mode");
	if (status & 0x1000) println("  Software Controls LEDs");

	if ((status & 0x0001) && !(priorstatus & 0x0001)) {
		println("    connect");
		// 100 ms debounce (USB 2.0: TATTDB, page 150 & 188)
		//delay(100);  // TODO: horribly bad... need timing events
		//reset(port);
		// TODO... reset timer?

	} else if (!(status & 0x0001) && (priorstatus & 0x0001)) {
		println("    disconnect");


	}

}


void USBHub::disconnect()
{
	// TODO: free resources
}


/*
config descriptor from a Multi-TT hub
09 02 29 00 01 01 00 E0 32
09 04 00 00 01 09 00 01 00
07 05 81 03 01 00 0C
09 04 00 01 01 09 00 02 00
07 05 81 03 01 00 0C
*/


