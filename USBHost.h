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
class USBDriver;
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
	USBDriver *drivers;
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
	USBDriver *callback_object;
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
	Pipe_t     *pipe;
	void       *buffer;
	uint32_t   length;
	setup_t    *setup;
	USBDriver  *driver;
	uint32_t   unused;
};

/************************************************/
/*  Main USB EHCI Controller                    */
/************************************************/

class USBHost {
public:
	static void begin();
protected:
	static Pipe_t * new_Pipe(Device_t *dev, uint32_t type, uint32_t endpoint,
		uint32_t direction, uint32_t maxlen, uint32_t interval=0);
	static bool queue_Control_Transfer(Device_t *dev, setup_t *setup,
		void *buf, USBDriver *driver);
	static bool queue_Data_Transfer(Pipe_t *pipe, void *buffer,
		uint32_t len, USBDriver *driver);
	static Device_t * new_Device(uint32_t speed, uint32_t hub_addr, uint32_t hub_port);
	static void enumeration(const Transfer_t *transfer);
	static void driver_ready_for_device(USBDriver *driver);
private:
	static void isr();
	static void claim_drivers(Device_t *dev);
	static bool queue_Transfer(Pipe_t *pipe, Transfer_t *transfer);
	static void init_Device_Pipe_Transfer_memory(void);
	static Device_t * allocate_Device(void);
	static void free_Device(Device_t *q);
	static Pipe_t * allocate_Pipe(void);
	static void free_Pipe(Pipe_t *q);
	static Transfer_t * allocate_Transfer(void);
	static void free_Transfer(Transfer_t *q);
	static bool allocate_interrupt_pipe_bandwidth(uint32_t speed, uint32_t maxlen,
		uint32_t interval, uint32_t direction, uint32_t *offset_out,
		uint32_t *smask_out, uint32_t *cmask_out);
protected:
	static void print(const Transfer_t *transfer);
	static void print(const Transfer_t *first, const Transfer_t *last);
	static void print_token(uint32_t token);
	static void print(const Pipe_t *pipe);
	static void print_hexbytes(const void *ptr, uint32_t len);
	static void print(const char *s)	{ Serial.print(s); }
	static void print(int n)		{ Serial.print(n); }
	static void print(unsigned int n)	{ Serial.print(n); }
	static void print(long n)		{ Serial.print(n); }
	static void print(unsigned long n)	{ Serial.print(n); }
	static void println(const char *s)	{ Serial.println(s); }
	static void println(int n)		{ Serial.println(n); }
	static void println(unsigned int n)	{ Serial.println(n); }
	static void println(long n)		{ Serial.println(n); }
	static void println(unsigned long n)	{ Serial.println(n); }
	static void println()			{ Serial.println(); }
	static void print(uint32_t n, uint8_t b) { Serial.print(n, b); }
	static void println(uint32_t n, uint8_t b) { Serial.print(n, b); }
	static void println(const char *s, int n) {
		Serial.print(s); Serial.println(n); }
	static void println(const char *s, unsigned int n) {
		Serial.print(s); Serial.println(n); }
	static void println(const char *s, long n) {
		Serial.print(s); Serial.println(n); }
	static void println(const char *s, unsigned long n) {
		Serial.print(s); Serial.println(n); }
	static void println(const char *s, int n, uint8_t b) {
		Serial.print(s); Serial.println(n, b); }
	static void println(const char *s, unsigned int n, uint8_t b) {
		Serial.print(s); Serial.println(n, b); }
	static void println(const char *s, long n, uint8_t b) {
		Serial.print(s); Serial.println(n, b); }
	static void println(const char *s, unsigned long n, uint8_t b) {
		Serial.print(s); Serial.println(n, b); }

	static void mk_setup(setup_t &s, uint32_t bmRequestType, uint32_t bRequest,
			uint32_t wValue, uint32_t wIndex, uint32_t wLength) {
		s.word1 = bmRequestType | (bRequest << 8) | (wValue << 16);
		s.word2 = wIndex | (wLength << 16);
	}
};


/************************************************/
/*  USB Device Driver Common Base Class         */
/************************************************/

// All USB device drivers inherit from this base class.
class USBDriver : public USBHost {
protected:
	USBDriver() : next(NULL), device(NULL) {}
	// Check if a driver wishes to claim a device or interface or group
	// of interfaces within a device.  When this function returns true,
	// the driver is considered bound or loaded for that device.  When
	// new devices are detected, enumeration.cpp calls this function on
	// all unbound driver objects, to give them an opportunity to bind
	// to the new device.
	//   device has its vid&pid, class/subclass fields initialized
	//   type is 0 for device level, 1 for interface level, 2 for IAD
	//   descriptors points to the specific descriptor data
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);

	// When an unknown (not chapter 9) control transfer completes, this
	// function is called for all drivers bound to the device.  Return
	// true means this driver originated this control transfer, so no
	// more drivers need to be offered an opportunity to process it.
	// This function is optional, only needed if the driver uses control
	// transfers and wishes to be notified when they complete.
	virtual void control(const Transfer_t *transfer) { }

	// When a device disconnects from the USB, this function is called.
	// The driver must free all resources it has allocated.
	virtual void disconnect();

	// Drivers are managed by this single-linked list.  All inactive
	// (not bound to any device) drivers are linked from
	// available_drivers in enumeration.cpp.  When bound to a device,
	// drivers are linked from that Device_t drivers list.
	USBDriver *next;

	// The device this object instance is bound to.  In words, this
	// is the specific device this driver is using.  When not bound
	// to any device, this must be NULL.
	Device_t *device;

	friend class USBHost;
public:
	// TODO: user-level functions
	// check if device is bound/active/online
	// query vid, pid
	// query string: manufacturer, product, serial number
};


/************************************************/
/*  USB Device Drivers                          */
/************************************************/

class USBHub : public USBDriver {
public:
	USBHub();
protected:
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void control(const Transfer_t *transfer);
	virtual void disconnect();
	void poweron(uint32_t port);
	void getstatus(uint32_t port);
	void clearstatus(uint32_t port);
	void reset(uint32_t port);
	static void callback(const Transfer_t *transfer);
	void status_change(const Transfer_t *transfer);
	void new_port_status(uint32_t port, uint32_t status);
	void update_status();
	setup_t setup;
	uint8_t hub_desc[16];
	uint8_t endpoint;
	uint8_t numports;
	uint8_t characteristics;
	uint8_t powertime;
	uint8_t state;
	Pipe_t *changepipe;
	uint32_t changebits;
	uint32_t statusbits;
	uint16_t portstatus[7];
	uint8_t  portstate[7];
};

class KeyboardController : public USBDriver {
public:
	KeyboardController();
	int     available();
	int     read();
	uint8_t getKey();
	uint8_t getModifiers();
	uint8_t getOemKey();
	void    attachPress(void (*keyPressed)());
	void    attachRelease(void (*keyReleased)());
protected:
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void disconnect();
	static void callback(const Transfer_t *transfer);
	void new_data(const Transfer_t *transfer);
private:
	void (*keyPressedFunction)();
	void (*keyReleasedFunction)();
	Pipe_t *datapipe;
	uint8_t report[8];
};


#endif
