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

#ifndef USB_HOST_TEENSY36_
#define USB_HOST_TEENSY36_

#include <stdint.h>

/************************************************/
/*  Data Structure Definitions                  */
/************************************************/

class USBHost;
class USBHostDriver;
typedef struct Device_struct    Device_t;
typedef struct Pipe_struct      Pipe_t;
typedef struct Transfer_struct  Transfer_t;

// setup_t holds the 8 byte USB SETUP packet data.
// These unions & structs allow convenient access to
// the setup fields.
typedef union {
 struct {
  union {
   struct {
        uint8_t bmRequestType;
        uint8_t bRequest;
   };
        uint16_t wRequestAndType;
  };
        uint16_t wValue;
        uint16_t wIndex;
        uint16_t wLength;
 };
 struct {
        uint32_t word1;
        uint32_t word2;
 };
} setup_t;

// Device_t holds all the information about a USB device
struct Device_struct {
	Pipe_t   *control_pipe;
	Device_t *next;
	setup_t  setup;
	uint8_t  speed; // 0=12, 1=1.5, 2=480 Mbit/sec
	uint8_t  address;
	uint8_t  hub_address;
	uint8_t  hub_port;
	uint8_t  enum_state;
	uint8_t  bDeviceClass;
	uint8_t  bDeviceSubClass;
	uint8_t  bDeviceProtocol;
	uint8_t  bmAttributes;
	uint8_t  bMaxPower;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t LanguageID;
};

// Pipe_t holes all information about each USB endpoint/pipe
// The first half is an EHCI QH structure for the pipe.
struct Pipe_struct {
	// Queue Head (QH), EHCI page 46-50
	struct {  // must be aligned to 32 byte boundary
		volatile uint32_t horizontal_link;
		volatile uint32_t capabilities[2];
		volatile uint32_t current;
		volatile uint32_t next;
		volatile uint32_t alt_next;
		volatile uint32_t token;
		volatile uint32_t buffer[5];
	} qh;
	Device_t *device;
	uint8_t  type; // 0=control, 1=isochronous, 2=bulk, 3=interrupt
	uint8_t  direction; // 0=out, 1=in (changes for control, others fixed)
	uint8_t  unusedbyte[2];
	void     *callback_object; // TODO: C++ callbacks??
	void     (*callback_function)(const Transfer_t *);
};

// Transfer_t represents a single transaction on the USB bus.
// The first portion is an EHCI qTD structure.  Transfer_t are
// allocated as-needed from a memory pool, loaded with pointers
// to the actual data buffers, linked into a followup list,
// and placed on ECHI Queue Heads.  When the ECHI interrupt
// occurs, the followup lists are used to find the Transfer_t
// in memory.  Callbacks are made, and then the Transfer_t are
// returned to the memory pool.
struct Transfer_struct {
	// Queue Element Transfer Descriptor (qTD), EHCI pg 40-45
	struct {  // must be aligned to 32 byte boundary
		volatile uint32_t next;
		volatile uint32_t alt_next;
		volatile uint32_t token;
		volatile uint32_t buffer[5];
	} qtd;
	// Linked list of queued, not-yet-completed transfers
	Transfer_t *next_followup;
	Transfer_t *prev_followup;
	// Data to be used by callback function.  When a group
	// of Transfer_t are created, these fields and the
	// interrupt-on-complete bit in the qTD token are only
	// set in the last Transfer_t of the list.
	Pipe_t   *pipe;
	void     *buffer;
	uint32_t length;
	uint32_t unused[3];
};

/************************************************/
/*  Main USB EHCI Controller                    */
/************************************************/

class USBHost {
public:
	static void begin();
protected:
	static Pipe_t * new_Pipe(Device_t *dev, uint32_t type, uint32_t endpoint,
		uint32_t direction, uint32_t max_packet_len);
	static bool new_Transfer(Pipe_t *pipe, void *buffer, uint32_t len);
	static Device_t * new_Device(uint32_t speed, uint32_t hub_addr, uint32_t hub_port);
	static void enumeration(const Transfer_t *transfer);
	static void driver_ready_for_device(USBHostDriver *driver);
private:
	static void isr();
	static void init_Device_Pipe_Transfer_memory(void);
	static Device_t * allocate_Device(void);
	static void free_Device(Device_t *q);
	static Pipe_t * allocate_Pipe(void);
	static void free_Pipe(Pipe_t *q);
	static Transfer_t * allocate_Transfer(void);
	static void free_Transfer(Transfer_t *q);
protected:
	static void print(const Transfer_t *transfer);
	static void print(const Transfer_t *first, const Transfer_t *last);
	static void print_token(uint32_t token);
	static void print(const Pipe_t *pipe);
	static void print_hexbytes(const void *ptr, uint32_t len);
	static void print(const char *s);
	static void print(const char *s, int num);
};


/************************************************/
/*  USB Device Drivers                          */
/************************************************/

class USBHostDriver : public USBHost {
public:
	virtual bool claim_device(Device_t *device, const uint8_t *descriptors) {
		return false;
	}
	virtual bool claim_interface(Device_t *device, const uint8_t *descriptors) {
		return false;
	}
	virtual void disconnect() {
	}

	USBHostDriver *next;
};

class USBHub : public USBHostDriver {
public:
	USBHub(); // { driver_ready_for_device(this); }
	virtual bool claim_device(Device_t *device, const uint8_t *descriptors);

};



#endif
