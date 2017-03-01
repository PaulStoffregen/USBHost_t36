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

USBHub::USBHub() : debouncetimer(this), /* mytimer(this), */ othertimer(this)
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
	debouncetimer.pointer = (void *)"Debounce Timer";
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
	sending_control_transfer = 0;
	memset(portstate, 0, sizeof(portstate));

	mk_setup(setup[0], 0xA0, 6, 0x2900, 0, sizeof(hub_desc));
	queue_Control_Transfer(dev, &setup[0], hub_desc, this);

	return true;
}


bool USBHub::can_send_control_now()
{
	if (sending_control_transfer) return false;
	sending_control_transfer = 1;
	return true;
}

void USBHub::send_poweron(uint32_t port)
{
	if (port == 0 || port > numports) return;
	if (can_send_control_now()) {
		mk_setup(setup[port], 0x23, 3, 8, port, 0);
		queue_Control_Transfer(device, &setup[port], NULL, this);
		send_pending_poweron &= ~(1 << port);
	} else {
		send_pending_poweron |= (1 << port);
	}
}

void USBHub::send_getstatus(uint32_t port)
{
	if (port > numports) return;
	if (can_send_control_now()) {
	println("getstatus, port = ", port);
		mk_setup(setup[port], ((port > 0) ? 0xA3 : 0xA0), 0, 0, port, 4);
		queue_Control_Transfer(device, &setup[port], &statusbits[port], this);
		send_pending_getstatus &= ~(1 << port);
	} else {
		send_pending_getstatus |= (1 << port);
	}
}

void USBHub::send_clearstatus_connect(uint32_t port)
{
	if (port == 0 || port > numports) return;
	if (can_send_control_now()) {
		mk_setup(setup[port], 0x23, 1, 16, port, 0); // 16=C_PORT_CONNECTION
		queue_Control_Transfer(device, &setup[port], NULL, this);
		send_pending_clearstatus_connect &= ~(1 << port);
	} else {
		send_pending_clearstatus_connect |= (1 << port);
	}
}

void USBHub::send_clearstatus_enable(uint32_t port)
{
	if (port == 0 || port > numports) return;
	if (can_send_control_now()) {
		mk_setup(setup[port], 0x23, 1, 17, port, 0); // 17=C_PORT_ENABLE
		queue_Control_Transfer(device, &setup[port], NULL, this);
		send_pending_clearstatus_enable &= ~(1 << port);
	} else {
		send_pending_clearstatus_enable |= (1 << port);
	}
}

void USBHub::send_clearstatus_suspend(uint32_t port)
{
	if (port == 0 || port > numports) return;
	if (can_send_control_now()) {
		mk_setup(setup[port], 0x23, 1, 18, port, 0); // 18=C_PORT_SUSPEND
		queue_Control_Transfer(device, &setup[port], NULL, this);
		send_pending_clearstatus_suspend &= ~(1 << port);
	} else {
		send_pending_clearstatus_suspend |= (1 << port);
	}
}

void USBHub::send_clearstatus_overcurrent(uint32_t port)
{
	if (port == 0 || port > numports) return;
	if (can_send_control_now()) {
		mk_setup(setup[port], 0x23, 1, 19, port, 0); // 19=C_PORT_OVER_CURRENT
		queue_Control_Transfer(device, &setup[port], NULL, this);
		send_pending_clearstatus_overcurrent &= ~(1 << port);
	} else {
		send_pending_clearstatus_overcurrent |= (1 << port);
	}
}

void USBHub::send_clearstatus_reset(uint32_t port)
{
	if (port == 0 || port > numports) return;
	if (can_send_control_now()) {
		mk_setup(setup[port], 0x23, 1, 20, port, 0); // 20=C_PORT_RESET
		queue_Control_Transfer(device, &setup[port], NULL, this);
		send_pending_clearstatus_reset &= ~(1 << port);
	} else {
		send_pending_clearstatus_reset |= (1 << port);
	}
}

void USBHub::send_setreset(uint32_t port)
{
	if (port == 0 || port > numports) return;
	println("send_setreset");
	if (can_send_control_now()) {
		mk_setup(setup[port], 0x23, 3, 4, port, 0); // set feature PORT_RESET
		queue_Control_Transfer(device, &setup[port], NULL, this);
		send_pending_setreset &= ~(1 << port);
	} else {
		send_pending_setreset |= (1 << port);
	}
}

static uint32_t lowestbit(uint32_t bitmask)
{
	return 31 - __builtin_clz(bitmask);
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
		break;
	  case 0x000000A3: // get port status
		println("New Port Status");
		if (transfer->length == 4) {
			uint32_t status = *(uint32_t *)(transfer->buffer);
			if (status != statusbits[port]) println("ERROR: status not same");
			new_port_status(port, status);
		}
		//if (changebits & (1 << port)) {
			//changebits &= ~(1 << port);
			//send_clearstatus(port);
		//}
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
	// After we've completed processing for this control
	// transfer, check if any more need to be sent.  These
	// allow only a single control transfer to occur at once
	// which isn't fast, but requires only 3 Transfer_t and
	// allows reusing the setup and other buffers
	sending_control_transfer = 0;
	if (send_pending_poweron) {
		send_poweron(lowestbit(send_pending_poweron));
	} else if (send_pending_clearstatus_connect) {
		send_clearstatus_connect(lowestbit(send_pending_clearstatus_connect));
	} else if (send_pending_clearstatus_enable) {
		send_clearstatus_enable(lowestbit(send_pending_clearstatus_enable));
	} else if (send_pending_clearstatus_suspend) {
		send_clearstatus_suspend(lowestbit(send_pending_clearstatus_suspend));
	} else if (send_pending_clearstatus_overcurrent) {
		send_clearstatus_overcurrent(lowestbit(send_pending_clearstatus_overcurrent));
	} else if (send_pending_clearstatus_reset) {
		send_clearstatus_reset(lowestbit(send_pending_clearstatus_reset));
	} else if (send_pending_getstatus) {
		send_getstatus(lowestbit(send_pending_getstatus));
	} else if (send_pending_setreset) {
		send_setreset(lowestbit(send_pending_setreset));
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
#if 1
	print("  status=");
	print(status, HEX);
	println("  port=", port);
	println("  state=", portstate[port-1]);
	// status bits, USB 2.0: 11.24.2.7.1 page 427
	if (status & 0x0001) println("  Device is present: ");
	if (status & 0x0002) {
		print("  Enabled, speed = ");
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
#endif
	uint8_t &state = portstate[port-1];
	switch (state) {
	  case PORT_OFF:
	  case PORT_DISCONNECT:
		if (status & 0x0001) { // connected
			state = PORT_DEBOUNCE1;
			start_debounce_timer(port);
			send_clearstatus_connect(port);
		}
		break;
	  case PORT_DEBOUNCE1:
	  case PORT_DEBOUNCE2:
	  case PORT_DEBOUNCE3:
	  case PORT_DEBOUNCE4:
	  case PORT_DEBOUNCE5:
		if (status & 0x0001) {
			if (++state > PORT_DEBOUNCE5) {
				// TODO: check for exclusive access to
				// enumeration process... stay in debounce
				// and add to wait list if enumeration busy
				stop_debounce_timer(port);
				println("sending reset");
				send_setreset(port);
			}
		} else {
			state = PORT_DISCONNECT;
		}
		break;
	  case PORT_RESET:
		if (status & 0x0002) {
			// port is now enabled
			//while (1) ;
			//send_clearstatus_enable(port);
			//send_clearstatus_connect(port);
			send_clearstatus_reset(port);
		}
	  case PORT_RECOVERY:
	  case PORT_ACTIVE:
		break;
	}

/*
	if ((status & 0x0001) && !(priorstatus & 0x0001)) {
		println("    connect");
		// 100 ms debounce (USB 2.0: TATTDB, page 150 & 188)
		//delay(100);  // TODO: horribly bad... need timing events
		//reset(port);
		// TODO... reset timer?

	} else if (!(status & 0x0001) && (priorstatus & 0x0001)) {
		println("    disconnect");


	}
*/

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
	if (timer == &debouncetimer) {
		uint32_t in_use = debounce_in_use;
		if (in_use != 0) {
			for (uint32_t i=1; i < numports; i++) {
				if (in_use & (1 << i)) send_getstatus(i);
			}
			debouncetimer.start(20000);
		}
	}
}

void USBHub::start_debounce_timer(uint32_t port)
{
	if (debounce_in_use == 0) debouncetimer.start(20000);
	debounce_in_use |= (1 << port);
}

void USBHub::stop_debounce_timer(uint32_t port)
{
	debounce_in_use &= ~(1 << port);
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


