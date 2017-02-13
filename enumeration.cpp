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


static USBDriver *available_drivers = NULL;
static uint8_t enumbuf[256] __attribute__ ((aligned(16)));
static setup_t enumsetup __attribute__ ((aligned(16)));


static uint32_t assign_addr(void);
static void pipe_set_maxlen(Pipe_t *pipe, uint32_t maxlen);
static void pipe_set_addr(Pipe_t *pipe, uint32_t addr);



void USBHost::driver_ready_for_device(USBDriver *driver)
{
	driver->device = NULL;
	driver->next = NULL;
	if (available_drivers == NULL) {
		available_drivers = driver;
	} else {
		// append to end of list
		USBDriver *last = available_drivers;
		while (last->next) last = last->next;
		last->next = driver;
	}
}

// Create a new device and begin the enumeration process
//
Device_t * USBHost::new_Device(uint32_t speed, uint32_t hub_addr, uint32_t hub_port)
{
	Device_t *dev;

	Serial.print("new_Device: ");
	switch (speed) {
	  case 0: Serial.print("12"); break;
	  case 1: Serial.print("1.5"); break;
	  case 2: Serial.print("480"); break;
	  default: Serial.print("??");
	}
	Serial.println(" Mbit/sec");
	dev = allocate_Device();
	if (!dev) return NULL;
	memset(dev, 0, sizeof(Device_t));
	dev->speed = speed;
	dev->address = 0;
	dev->hub_address = hub_addr;
	dev->hub_port = hub_port;
	dev->control_pipe = new_Pipe(dev, 0, 0, 0, 8);
	if (!dev->control_pipe) {
		free_Device(dev);
		return NULL;
	}
	dev->control_pipe->callback_function = &enumeration;
	dev->control_pipe->direction = 1; // 1=IN
	// TODO: exclusive access to enumeration process
	// any new devices detected while enumerating would
	// go onto a waiting list
	mk_setup(enumsetup, 0x80, 6, 0x0100, 0, 8); // 6=GET_DESCRIPTOR
	queue_Control_Transfer(dev, &enumsetup, enumbuf, NULL);
	return dev;
}



void USBHost::enumeration(const Transfer_t *transfer)
{
	uint32_t len;

	Serial.print("      CALLBACK: ");
	print_hexbytes(transfer->buffer, transfer->length);
	//print(transfer);
	Device_t *dev = transfer->pipe->device;

	// If a driver created this control transfer, allow it to process the result
	if (transfer->driver) {
		transfer->driver->control(transfer);
		return;
	}

	while (1) {
		// Within this large switch/case, "break" means we've done
		// some work, but more remains to be done in a different
		// state.  Generally break is used after parsing received
		// data, but what happens next could be different states.
		// When completed, return is used.  Generally, return happens
		// only after a new control transfer is queued, or when
		// enumeration is complete and no more communication is needed.
		switch (dev->enum_state) {
		case 0: // read 8 bytes of device desc, set max packet, and send set address
			pipe_set_maxlen(dev->control_pipe, enumbuf[7]);
			mk_setup(enumsetup, 0, 5, assign_addr(), 0, 0); // 5=SET_ADDRESS
			queue_Control_Transfer(dev, &enumsetup, NULL, NULL);
			dev->enum_state = 1;
			return;
		case 1: // request all 18 bytes of device descriptor
			dev->address = enumsetup.wValue;
			pipe_set_addr(dev->control_pipe, enumsetup.wValue);
			mk_setup(enumsetup, 0x80, 6, 0x0100, 0, 18); // 6=GET_DESCRIPTOR
			queue_Control_Transfer(dev, &enumsetup, enumbuf, NULL);
			dev->enum_state = 2;
			return;
		case 2: // parse 18 device desc bytes
			dev->bDeviceClass = enumbuf[4];
			dev->bDeviceSubClass = enumbuf[5];
			dev->bDeviceProtocol = enumbuf[6];
			dev->idVendor = enumbuf[8] | (enumbuf[9] << 8);
			dev->idProduct = enumbuf[10] | (enumbuf[11] << 8);
			enumbuf[0] = enumbuf[14];
			enumbuf[1] = enumbuf[15];
			enumbuf[2] = enumbuf[16];
			if ((enumbuf[0] | enumbuf[1] | enumbuf[2]) > 0) {
				dev->enum_state = 3;
			} else {
				dev->enum_state = 11;
			}
			break;
		case 3: // request Language ID
			len = sizeof(enumbuf) - 4;
			mk_setup(enumsetup, 0x80, 6, 0x0300, 0, len); // 6=GET_DESCRIPTOR
			queue_Control_Transfer(dev, &enumsetup, enumbuf + 4, NULL);
			dev->enum_state = 4;
			return;
		case 4: // parse Language ID
			if (enumbuf[4] < 4 || enumbuf[5] != 3) {
				dev->enum_state = 11;
			} else {
				dev->LanguageID = enumbuf[6] | (enumbuf[7] << 8);
				if (enumbuf[0]) dev->enum_state = 5;
				else if (enumbuf[1]) dev->enum_state = 7;
				else if (enumbuf[2]) dev->enum_state = 9;
				else dev->enum_state = 11;
			}
			break;
		case 5: // request Manufacturer string
			len = sizeof(enumbuf) - 4;
			mk_setup(enumsetup, 0x80, 6, 0x0300 | enumbuf[0], dev->LanguageID, len);
			queue_Control_Transfer(dev, &enumsetup, enumbuf + 4, NULL);
			dev->enum_state = 6;
			return;
		case 6: // parse Manufacturer string
			// TODO: receive the string...
			if (enumbuf[1]) dev->enum_state = 7;
			else if (enumbuf[2]) dev->enum_state = 9;
			else dev->enum_state = 11;
			break;
		case 7: // request Product string
			len = sizeof(enumbuf) - 4;
			mk_setup(enumsetup, 0x80, 6, 0x0300 | enumbuf[1], dev->LanguageID, len);
			queue_Control_Transfer(dev, &enumsetup, enumbuf + 4, NULL);
			dev->enum_state = 8;
			return;
		case 8: // parse Product string
			// TODO: receive the string...
			if (enumbuf[2]) dev->enum_state = 9;
			else dev->enum_state = 11;
			break;
		case 9: // request Serial Number string
			len = sizeof(enumbuf) - 4;
			mk_setup(enumsetup, 0x80, 6, 0x0300 | enumbuf[2], dev->LanguageID, len);
			queue_Control_Transfer(dev, &enumsetup, enumbuf + 4, NULL);
			dev->enum_state = 10;
			return;
		case 10: // parse Serial Number string
			// TODO: receive the string...
			dev->enum_state = 11;
			break;
		case 11: // request first 9 bytes of config desc
			mk_setup(enumsetup, 0x80, 6, 0x0200, 0, 9); // 6=GET_DESCRIPTOR
			queue_Control_Transfer(dev, &enumsetup, enumbuf, NULL);
			dev->enum_state = 12;
			return;
		case 12: // read 9 bytes, request all of config desc
			len = enumbuf[2] | (enumbuf[3] << 8);
			Serial.print("Config data length = ");
			Serial.println(len);
			if (len > sizeof(enumbuf)) {
				// TODO: how to handle device with too much config data
			}
			mk_setup(enumsetup, 0x80, 6, 0x0200, 0, len); // 6=GET_DESCRIPTOR
			queue_Control_Transfer(dev, &enumsetup, enumbuf, NULL);
			dev->enum_state = 13;
			return;
		case 13: // read all config desc, send set config
			Serial.print("bNumInterfaces = ");
			Serial.println(enumbuf[4]);
			Serial.print("bConfigurationValue = ");
			Serial.println(enumbuf[5]);
			dev->bmAttributes = enumbuf[7];
			dev->bMaxPower = enumbuf[8];
			// TODO: actually do something with interface descriptor?
			mk_setup(enumsetup, 0, 9, enumbuf[5], 0, 0); // 9=SET_CONFIGURATION
			queue_Control_Transfer(dev, &enumsetup, NULL, NULL);
			dev->enum_state = 14;
			return;
		case 14: // device is now configured
			claim_drivers(dev);
			dev->enum_state = 15;
			// TODO: unlock exclusive access to enumeration process
			// if any detected devices are waiting, start the first
			return;
		case 15: // control transfers for other stuff?
			// TODO: handle other standard control: set/clear feature, etc
		default:
			return;
		}
	}
}

void USBHost::claim_drivers(Device_t *dev)
{
	USBDriver *driver, *prev=NULL;

	// first check if any driver wishes to claim the entire device
	for (driver=available_drivers; driver != NULL; driver = driver->next) {
		if (driver->claim(dev, 0, enumbuf + 9)) {
			if (prev) {
				prev->next = driver->next;
			} else {
				available_drivers = driver->next;
			}
			driver->device = dev;
			driver->next = NULL;
			dev->drivers = driver;
			return;
		}
		prev = driver;
	}
	// TODO: parse interfaces from config descriptor
	// try claim_interface on drivers
}

static uint32_t assign_addr(void)
{
	return 29; // TODO: when multiple devices, assign a unique address
}

static void pipe_set_maxlen(Pipe_t *pipe, uint32_t maxlen)
{
	Serial.print("pipe_set_maxlen ");
	Serial.println(maxlen);
	pipe->qh.capabilities[0] = (pipe->qh.capabilities[0] & 0x8000FFFF) | (maxlen << 16);
}

static void pipe_set_addr(Pipe_t *pipe, uint32_t addr)
{
	Serial.print("pipe_set_addr ");
	Serial.println(addr);
	pipe->qh.capabilities[0] = (pipe->qh.capabilities[0] & 0xFFFFFF80) | addr;
}

//static uint32_t pipe_get_addr(Pipe_t *pipe)
//{
//	return pipe->qh.capabilities[0] & 0xFFFFFF80;
//}


