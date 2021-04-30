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

#if !defined(__MK66FX1M0__) && !defined(__IMXRT1052__) && !defined(__IMXRT1062__)
#error "USBHost_t36 only works with Teensy 3.6 or Teensy 4.x.  Please select it in Tools > Boards"
#endif
#include "utility/imxrt_usbhs.h"
#include "utility/msc.h"

// Dear inquisitive reader, USB is a complex protocol defined with
// very specific terminology.  To have any chance of understand this
// source code, you absolutely must have solid knowledge of specific
// USB terms such as host, device, endpoint, pipe, enumeration....
// You really must also have at least a basic knowledge of the
// different USB transfers: control, bulk, interrupt, isochronous.
//
// The USB 2.0 specification explains these in chapter 4 (pages 15
// to 24), and provides more detail in the first part of chapter 5
// (pages 25 to 55).  The USB spec is published for free at
// www.usb.org.  Here is a convenient link to just the main PDF:
//
// https://www.pjrc.com/teensy/beta/usb20.pdf
//
// This is a huge file, but chapter 4 is short and easy to read.
// If you're not familiar with the USB lingo, please do yourself
// a favor by reading at least chapter 4 to get up to speed on the
// meaning of these important USB concepts and terminology.
//
// If you wish to ask questions (which belong on the forum, not
// github issues) or discuss development of this library, you
// ABSOLUTELY MUST know the basic USB terminology from chapter 4.
// Please repect other people's valuable time & effort by making
// your best effort to read chapter 4 before asking USB questions!


// Uncomment this line to see lots of debugging info!
//#define USBHOST_PRINT_DEBUG


// This can let you control where to send the debugging messages
//#define USBHDBGSerial	Serial1
#ifndef USBHDBGSerial
#define USBHDBGSerial	Serial
#endif


/************************************************/
/*  Data Types                                  */
/************************************************/

// These 6 types are the key to understanding how this USB Host
// library really works.

// USBHost is a class controlling access to one host port
// (USB2 is the standard host port on the Teensy 4.1; USB1 is
// the standard device port, which may also be used as a host port)
class USBHostPort;

// USBHost is a class controlling the hardware.
// All common USB functionality is implemented here.
class USBHost;

// These 3 structures represent the actual USB entities
// USBHost manipulates.  One Device_t is created for
// each active USB device.  One Pipe_t is create for
// each endpoint.  Transfer_t structures are created
// when any data transfer is added to the EHCI work
// queues, and then returned to the free pool after the
// data transfer completes and the driver has processed
// the results.
typedef struct Device_struct       Device_t;
typedef struct Pipe_struct         Pipe_t;
typedef struct Transfer_struct     Transfer_t;
typedef enum { CLAIM_NO=0, CLAIM_REPORT, CLAIM_INTERFACE} hidclaim_t;

// All USB device drivers inherit use these classes.
// Drivers build user-visible functionality on top
// of these classes, which receive USB events from
// USBHost.
class USBDriver;
class USBDriverTimer;
class USBHIDInput;

/************************************************/
/*  Added Defines                               */
/************************************************/
// Keyboard special Keys
#define KEYD_UP    		0xDA
#define KEYD_DOWN    	0xD9
#define KEYD_LEFT   	0xD8
#define KEYD_RIGHT   	0xD7
#define KEYD_INSERT		0xD1
#define KEYD_DELETE		0xD4
#define KEYD_PAGE_UP    0xD3
#define KEYD_PAGE_DOWN  0xD6
#define KEYD_HOME      	0xD2
#define KEYD_END       	0xD5
#define KEYD_F1        	0xC2
#define KEYD_F2        	0xC3
#define KEYD_F3        	0xC4
#define KEYD_F4        	0xC5
#define KEYD_F5        	0xC6
#define KEYD_F6        	0xC7
#define KEYD_F7        	0xC8
#define KEYD_F8        	0xC9
#define KEYD_F9        	0xCA
#define KEYD_F10       	0xCB
#define KEYD_F11       	0xCC
#define KEYD_F12       	0xCD


// USBSerial formats - Lets encode format into bits
// Bits: 0-4 - Number of data bits 
// Bits: 5-7 - Parity (0=none, 1=odd, 2 = even)
// bits: 8-9 - Stop bits. 0=1, 1=2


#define USBHOST_SERIAL_7E1 0x047
#define USBHOST_SERIAL_7O1 0x027
#define USBHOST_SERIAL_8N1 0x08
#define USBHOST_SERIAL_8N2 0x108
#define USBHOST_SERIAL_8E1 0x048
#define USBHOST_SERIAL_8O1 0x028



/************************************************/
/*  Data Structure Definitions                  */
/************************************************/

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

extern void mk_setup(setup_t &s, uint32_t bmRequestType, uint32_t bRequest,
			uint32_t wValue, uint32_t wIndex, uint32_t wLength);

typedef struct {
	enum {STRING_BUF_SIZE=50};
	enum {STR_ID_MAN=0, STR_ID_PROD, STR_ID_SERIAL, STR_ID_CNT};
	uint8_t iStrings[STR_ID_CNT];	// Index into array for the three indexes
	uint8_t buffer[STRING_BUF_SIZE];
} strbuf_t;

#define DEVICE_STRUCT_STRING_BUF_SIZE 50

// Device_t holds all the information about a USB device
struct Device_struct {
	Pipe_t   *control_pipe;
	Pipe_t   *data_pipes;
	Device_t *next;
	USBDriver *drivers;
	strbuf_t *strbuf;
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
	uint8_t  start_mask;
	uint8_t  complete_mask;
	Pipe_t   *next;
	void     (*callback_function)(const Transfer_t *);
	uint16_t periodic_interval;
	uint16_t periodic_offset;
	uint16_t bandwidth_interval;
	uint16_t bandwidth_offset;
	uint16_t bandwidth_shift;
	uint8_t  bandwidth_stime;
	uint8_t  bandwidth_ctime;
	uint32_t unused1;
	uint32_t unused2;
	uint32_t unused3;
	uint32_t unused4;
	uint32_t unused5;
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
	Pipe_t     *pipe;
	// Data to be used by callback function.  When a group
	// of Transfer_t are created, these fields and the
	// interrupt-on-complete bit in the qTD token are only
	// set in the last Transfer_t of the list.
	void       *buffer;
	uint32_t   length;
	setup_t    setup;
	USBDriver  *driver;
};


/************************************************/
/*  Print Debug Info                            */
/************************************************/

class PrintDebug {
public:
#ifdef USBHOST_PRINT_DEBUG
	static void print_(const Transfer_t *transfer);
	static void print_(const Transfer_t *first, const Transfer_t *last);
	static void print_token(uint32_t token);
	static void print_(const Pipe_t *pipe);
	static void print_driverlist(const char *name, const USBDriver *driver);
	static void print_qh_list(const Pipe_t *list);
	static void print_device_descriptor(const uint8_t *p);
	static void print_config_descriptor(const uint8_t *p, uint32_t maxlen);
	static void print_string_descriptor(const char *name, const uint8_t *p);
	static void print_hexbytes(const void *ptr, uint32_t len);
	static void print_(const char *s)	{ USBHDBGSerial.print(s); }
	static void print_(int n)		{ USBHDBGSerial.print(n); }
	static void print_(unsigned int n)	{ USBHDBGSerial.print(n); }
	static void print_(long n)		{ USBHDBGSerial.print(n); }
	static void print_(unsigned long n)	{ USBHDBGSerial.print(n); }
	static void println_(const char *s)	{ USBHDBGSerial.println(s); }
	static void println_(int n)		{ USBHDBGSerial.println(n); }
	static void println_(unsigned int n)	{ USBHDBGSerial.println(n); }
	static void println_(long n)		{ USBHDBGSerial.println(n); }
	static void println_(unsigned long n)	{ USBHDBGSerial.println(n); }
	static void println_()			{ USBHDBGSerial.println(); }
	static void print_(uint32_t n, uint8_t b) { USBHDBGSerial.print(n, b); }
	static void println_(uint32_t n, uint8_t b) { USBHDBGSerial.println(n, b); }
	static void print_(const char *s, int n, uint8_t b = DEC) {
		USBHDBGSerial.print(s); USBHDBGSerial.print(n, b); }
	static void print_(const char *s, unsigned int n, uint8_t b = DEC) {
		USBHDBGSerial.print(s); USBHDBGSerial.print(n, b); }
	static void print_(const char *s, long n, uint8_t b = DEC) {
		USBHDBGSerial.print(s); USBHDBGSerial.print(n, b); }
	static void print_(const char *s, unsigned long n, uint8_t b = DEC) {
		USBHDBGSerial.print(s); USBHDBGSerial.print(n, b); }
	static void println_(const char *s, int n, uint8_t b = DEC) {
		USBHDBGSerial.print(s); USBHDBGSerial.println(n, b); }
	static void println_(const char *s, unsigned int n, uint8_t b = DEC) {
		USBHDBGSerial.print(s); USBHDBGSerial.println(n, b); }
	static void println_(const char *s, long n, uint8_t b = DEC) {
		USBHDBGSerial.print(s); USBHDBGSerial.println(n, b); }
	static void println_(const char *s, unsigned long n, uint8_t b = DEC) {
		USBHDBGSerial.print(s); USBHDBGSerial.println(n, b); }
	friend class USBDriverTimer; // for access to print & println
#else
	static void print_(const Transfer_t *transfer) {}
	static void print_(const Transfer_t *first, const Transfer_t *last) {}
	static void print_token(uint32_t token) {}
	static void print_(const Pipe_t *pipe) {}
	static void print_driverlist(const char *name, const USBDriver *driver) {}
	static void print_qh_list(const Pipe_t *list) {}
	static void print_device_descriptor(const uint8_t *p) {}
	static void print_config_descriptor(const uint8_t *p, uint32_t maxlen) {}
	static void print_string_descriptor(const char *name, const uint8_t *p) {}
	static void print_hexbytes(const void *ptr, uint32_t len) {}
	static void print_(const char *s) {}
	static void print_(int n) {}
	static void print_(unsigned int n) {}
	static void print_(long n) {}
	static void print_(unsigned long n) {}
	static void println_(const char *s) {}
	static void println_(int n) {}
	static void println_(unsigned int n) {}
	static void println_(long n) {}
	static void println_(unsigned long n) {}
	static void println_() {}
	static void print_(uint32_t n, uint8_t b) {}
	static void println_(uint32_t n, uint8_t b) {}
	static void print_(const char *s, int n, uint8_t b = DEC) {}
	static void print_(const char *s, unsigned int n, uint8_t b = DEC) {}
	static void print_(const char *s, long n, uint8_t b = DEC) {}
	static void print_(const char *s, unsigned long n, uint8_t b = DEC) {}
	static void println_(const char *s, int n, uint8_t b = DEC) {}
	static void println_(const char *s, unsigned int n, uint8_t b = DEC) {}
	static void println_(const char *s, long n, uint8_t b = DEC) {}
	static void println_(const char *s, unsigned long n, uint8_t b = DEC) {}
#endif
};


/**********************************************************************/
/*  Memory allocations for a single USB host port (e.g. USB1 or USB2) */
/**********************************************************************/

// All USB EHCI controller hardware access is done from this file's code.
// Hardware services are made available to the rest of this library by
// three structures:
//
//   Pipe_t: Every USB endpoint is accessed by a pipe.  new_Pipe()
//     sets up the EHCI to support the pipe/endpoint, and delete_Pipe()
//     removes this configuration.
//
//   Transfer_t: These are used for all communication.  Data transfers
//     are placed into work queues, to be executed by the EHCI in
//     the future.  Transfer_t only manages data.  The actual data
//     is stored in a separate buffer (usually from a device driver)
//     which is referenced from Transfer_t.  All data transfer is queued,
//     never done with blocking functions that wait.  When transfers
//     complete, a driver-supplied callback function is called to notify
//     the driver.
//
//   USBDriverTimer: Some drivers require timers.  These allow drivers
//     to share the hardware timer, with each USBDriverTimer object
//     able to schedule a callback function a configurable number of
//     microseconds in the future.
//
// In addition to these 3 services, the EHCI interrupt also responds
// to changes on the main port, creating and deleting the root device.
// See enumeration.cpp for all device-level code.

// Size of the periodic list, in milliseconds.  This determines the
// slowest rate we can poll interrupt endpoints.  Each entry uses
// 12 bytes (4 for a pointer, 8 for bandwidth management).
// Supported values: 8, 16, 32, 64, 128, 256, 512, 1024
#if defined(USBHS_PERIODIC_LIST_SIZE)
#define PERIODIC_LIST_SIZE (USBHS_PERIODIC_LIST_SIZE)
#else
#define PERIODIC_LIST_SIZE  32
#endif

#define PORT_STATE_DISCONNECTED   0
#define PORT_STATE_DEBOUNCE       1
#define PORT_STATE_RESET          2
#define PORT_STATE_RECOVERY       3
#define PORT_STATE_ACTIVE         4

class USBHostPort {
public:
	// 1 => USB1 (which is the device port by default); 2 => USB2 (which is the host port by default)
	uint8_t host_port;

	// The EHCI periodic schedule, used for interrupt pipes/endpoints
	uint32_t periodictable[PERIODIC_LIST_SIZE] __attribute__ ((aligned(4096)));
	uint8_t  uframe_bandwidth[PERIODIC_LIST_SIZE*8];

	// State physical USB host port (PORT_STATE_*)
	uint8_t  port_state = 0;

	// The device currently connected, or NULL when no device
	Device_t *rootdev = NULL;

	// List of all connected devices, regardless of their status.  If
	// it's connected to the EHCI port or any port on any hub, it needs
	// to be linked into this list.
	Device_t *devlist = NULL;

	// List of all queued transfers in the asychronous schedule (control & bulk).
	// When the EHCI completes these transfers, this list is how we locate them
	// in memory.
	Transfer_t *async_followup_first = NULL;
	Transfer_t *async_followup_last = NULL;

	// List of all queued transfers in the asychronous schedule (interrupt endpoints)
	// When the EHCI completes these transfers, this list is how we locate them
	// in memory.
	Transfer_t *periodic_followup_first = NULL;
	Transfer_t *periodic_followup_last = NULL;

	// List of all pending timers.  This double linked list is stored in
	// chronological order.  Each timer is stored with the number of
	// microseconds which need to elapsed from the prior timer on this
	// list, to allow efficient servicing from the timer interrupt.
	USBDriverTimer *active_timers = NULL;

	// List of all inactive drivers.  At the end of enumeration, when
	// drivers claim the device or its interfaces, they are removed
	// from this list and linked into the list of active drivers on
	// that device.  When devices disconnect, the drivers are returned
	// to this list, making them again available for enumeration of new
	// devices.
	USBDriver *available_drivers = NULL;

	// True while any device is present but not yet fully configured.
	// Only one USB device may be in this state at a time (responding
	// to address zero) and using the enumeration buffer.
	volatile bool enumeration_busy = false;

	// Buffers used during enumeration.  One a single USB device
	// may enumerate at once, because USB address zero is used, and
	// because this buffer & state info can't be shared.
	uint8_t enumbuf[512] __attribute__ ((aligned(16)));
	setup_t enumsetup __attribute__ ((aligned(16)));
	uint16_t enumlen;

	// Lists of "free" memory
	Device_t * free_Device_list = NULL;
	Pipe_t * free_Pipe_list = NULL;
	Transfer_t * free_Transfer_list = NULL;
	strbuf_t * free_strbuf_list = NULL;

	void (* isr_callback)();
	void isr();
	
	void (* enumeration_callback)(const Transfer_struct*);
	void enumeration(const Transfer_struct *transfer);	
	
	USBHostPort(uint8_t host_port,
	            void (* isr_callback)(void),
	            void (* enumeration_callback)(const Transfer_struct*))
			: host_port(host_port),
			  isr_callback(isr_callback),
			  enumeration_callback(enumeration_callback) { }

	Transfer_t * allocate_Transfer(void);
	void free_Transfer(Transfer_t *q);
	Device_t * allocate_Device(void);
	void delete_Pipe(Pipe_t *pipe);
	void free_Device(Device_t *q);
	Pipe_t * allocate_Pipe(void);
	void free_Pipe(Pipe_t *q);
	strbuf_t * allocate_string_buffer(void);
	void free_string_buffer(strbuf_t *strbuf);

	bool queue_Transfer(Pipe_t *pipe, Transfer_t *transfer);
	bool queue_Control_Transfer(Device_t *dev, setup_t *setup,
		void *buf, USBDriver *driver);
	bool queue_Data_Transfer(Pipe_t *pipe, void *buffer,
		uint32_t len, USBDriver *driver);
	bool followup_Transfer(Transfer_t *transfer);
	void followup_Error(void);
	void add_to_async_followup_list(Transfer_t *, Transfer_t *);
	void remove_from_async_followup_list(Transfer_t *);
	void add_to_periodic_followup_list(Transfer_t *, Transfer_t *);
	void remove_from_periodic_followup_list(Transfer_t *);

	Pipe_t * new_Pipe(Device_t *dev, uint32_t type, uint32_t endpoint,
		uint32_t direction, uint32_t maxlen, uint32_t interval=0);
	Device_t * new_Device(uint32_t speed, uint32_t hub_addr, uint32_t hub_port);
	void disconnect_Device(Device_t *dev);
	void driver_ready_for_device(USBDriver *driver);

private:
	uint32_t assign_address(void);
	bool address_in_use(uint32_t addr);
	void claim_drivers(Device_t *dev);
	bool allocate_interrupt_pipe_bandwidth(Pipe_t *pipe,
		uint32_t maxlen, uint32_t interval);
	void add_qh_to_periodic_schedule(Pipe_t *pipe);
};

// USBHostPort singletons for each host port
extern USBHostPort* usb_host_ports[NUM_USB_HOST_PORTS];


/************************************************/
/*  Main USB EHCI Controller                    */
/************************************************/

class USBHost {
public:
	USBHost(const uint8_t host_port) : usb_host_port(usb_host_ports[host_port - 1]) { }
	USBHost() : usb_host_port(usb_host_ports[DEFAULT_HOST_PORT - 1]) { }
	void begin();
	void Task();
	void enumeration(const Transfer_t *transfer);
	void countFree(uint32_t &devices, uint32_t &pipes, uint32_t &trans, uint32_t &strs);
	// Maybe others may want/need to contribute memory example HID devices may want to add transfers.
	void contribute_Devices(Device_t *devices, uint32_t num);
	void contribute_Pipes(Pipe_t *pipes, uint32_t num);
	void contribute_Transfers(Transfer_t *transfers, uint32_t num);
	void contribute_String_Buffers(strbuf_t *strbuf, uint32_t num);
    USBHostPort *usb_host_port;
private:
	// A small amount of non-driver memory, just to get things started
	// TODO: is this really necessary?  Can these be eliminated, so we
	// use only memory from the drivers?
	Device_t memory_Device[1];
	Pipe_t memory_Pipe[1] __attribute__ ((aligned(32)));
	Transfer_t memory_Transfer[4] __attribute__ ((aligned(32)));
	void init_Device_Pipe_Transfer_memory(void);
};


/************************************************/
/*  USB Device Driver Common Base Class         */
/************************************************/

// All USB device drivers inherit from this base class.
class USBDriver : public USBHost {
public:
	operator bool() {
		Device_t *dev = *(Device_t * volatile *)&device;
		return dev != nullptr;
	}
	uint16_t idVendor() {
		Device_t *dev = *(Device_t * volatile *)&device;
		return (dev != nullptr) ? dev->idVendor : 0;
	}
	uint16_t idProduct() {
		Device_t *dev = *(Device_t * volatile *)&device;
		return (dev != nullptr) ? dev->idProduct : 0;
	}
	const uint8_t *manufacturer() {
		Device_t *dev = *(Device_t * volatile *)&device;
		if (dev == nullptr || dev->strbuf == nullptr) return nullptr;
		return &dev->strbuf->buffer[dev->strbuf->iStrings[strbuf_t::STR_ID_MAN]];
	}
	const uint8_t *product() {
		Device_t *dev = *(Device_t * volatile *)&device;
		if (dev == nullptr || dev->strbuf == nullptr) return nullptr;
		return &dev->strbuf->buffer[dev->strbuf->iStrings[strbuf_t::STR_ID_PROD]];
	}
	const uint8_t *serialNumber() {
		Device_t *dev = *(Device_t * volatile *)&device;
		if (dev == nullptr || dev->strbuf == nullptr) return nullptr;
		return &dev->strbuf->buffer[dev->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]];
	}

	// When an unknown (not chapter 9) control transfer completes, this
	// function is called for all drivers bound to the device.  Return
	// true means this driver originated this control transfer, so no
	// more drivers need to be offered an opportunity to process it.
	// This function is optional, only needed if the driver uses control
	// transfers and wishes to be notified when they complete.
	virtual void control(const Transfer_t *transfer) { }

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

	// When any of the USBDriverTimer objects a driver creates generates
	// a timer event, this function is called.
	virtual void timer_event(USBDriverTimer *whichTimer) { }

	// When the user calls USBHost::Task, this Task function for all
	// active drivers is called, so they may update state and/or call
	// any attached user callback functions.
	virtual void Task() { }

	// When a device disconnects from the USB, this function is called.
	// The driver must free all resources it allocated and update any
	// internal state necessary to deal with the possibility of user
	// code continuing to call its API.  However, pipes and transfers
	// are the handled by lower layers, so device drivers do not free
	// pipes they created or cancel transfers they had in progress.
	virtual void disconnect();

	// Drivers are managed by this single-linked list.  All inactive
	// (not bound to any device) drivers are linked from
	// available_drivers in enumeration.cpp.  When bound to a device,
	// drivers are linked from that Device_t drivers list.
	USBDriver *next;

	// The device this object instance is bound to.  In words, this
	// is the specific device this driver is using.  When not bound
	// to any device, this must be NULL.  Drivers may set this to
	// any non-NULL value if they are in a state where they do not
	// wish to claim any device or interface (eg, if getting data
	// from the HID parser).
	Device_t *device;
	friend class USBHost;
	friend class USBHostPort;
	friend class PrintDebug;
};

// Device drivers may create these timer objects to schedule a timer call
class USBDriverTimer {
public:
	USBDriverTimer() { }
	USBDriverTimer(USBDriver *d) : driver(d) { }
	USBDriverTimer(USBHIDInput *hd) : driver(nullptr), hidinput(hd) { }

	void init(USBDriver *d) { driver = d; };
	void start(uint32_t microseconds);
	void stop();
	void *pointer;
	uint32_t integer;
	uint32_t started_micros; // testing only
private:
	USBDriver      *driver;
	USBHIDInput    *hidinput;
	uint32_t       usec;
	USBDriverTimer *next;
	USBDriverTimer *prev;
	friend class USBHost;
	friend class USBHostPort;
};

// Device drivers may inherit from this base class, if they wish to receive
// HID input data fully decoded by the USBHIDParser driver
class USBHIDParser;

class USBHIDInput {
public:
	operator bool() { return (mydevice != nullptr); }
	uint16_t idVendor() { return (mydevice != nullptr) ? mydevice->idVendor : 0; }
	uint16_t idProduct() { return (mydevice != nullptr) ? mydevice->idProduct : 0; }
	const uint8_t *manufacturer()
		{  return  ((mydevice == nullptr) || (mydevice->strbuf == nullptr)) ? nullptr : &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_MAN]]; }
	const uint8_t *product()
		{  return  ((mydevice == nullptr) || (mydevice->strbuf == nullptr)) ? nullptr : &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_PROD]]; }
	const uint8_t *serialNumber()
		{  return  ((mydevice == nullptr) || (mydevice->strbuf == nullptr)) ? nullptr : &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]]; }


private:
	virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
	virtual bool hid_process_in_data(const Transfer_t *transfer) {return false;}
	virtual bool hid_process_out_data(const Transfer_t *transfer) {return false;}
	virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
	virtual void hid_input_data(uint32_t usage, int32_t value);
	virtual void hid_input_end();
	virtual void disconnect_collection(Device_t *dev);
	virtual void hid_timer_event(USBDriverTimer *whichTimer) { }
	void add_to_list();
	USBHIDInput *next = NULL;
	friend class USBHIDParser;
protected:
	Device_t *mydevice = NULL;
};


// Device drivers may inherit from this base class, if they wish to receive
// HID input like data from Bluetooth HID device. 
class BluetoothController;

class BTHIDInput {
public:
	operator bool() { return (btdevice != nullptr); }
	uint16_t idVendor() { return (btdevice != nullptr) ? btdevice->idVendor : 0; }
	uint16_t idProduct() { return (btdevice != nullptr) ? btdevice->idProduct : 0; }
	const uint8_t *manufacturer()
		{  return  ((btdevice == nullptr) || (btdevice->strbuf == nullptr)) ? nullptr : &btdevice->strbuf->buffer[btdevice->strbuf->iStrings[strbuf_t::STR_ID_MAN]]; }
	const uint8_t *product()
		{  return  remote_name_; }
	const uint8_t *serialNumber()
		{  return  ((btdevice == nullptr) || (btdevice->strbuf == nullptr)) ? nullptr : &btdevice->strbuf->buffer[btdevice->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]]; }
private:
	virtual bool claim_bluetooth(BluetoothController *driver, uint32_t bluetooth_class, uint8_t *remoteName) {return false;}
	virtual bool process_bluetooth_HID_data(const uint8_t *data, uint16_t length) {return false;}
	virtual void release_bluetooth() {};
	virtual bool remoteNameComplete(const uint8_t *remoteName) {return true;}
	virtual void connectionComplete(void) {};
	void add_to_list();
	BTHIDInput *next = NULL;
	friend class BluetoothController;
protected:
	enum {SP_NEED_CONNECT=0x1, SP_DONT_NEED_CONNECT=0x02, SP_PS3_IDS=0x4};
	enum {REMOTE_NAME_SIZE=32};
	uint8_t  special_process_required = 0;
	Device_t *btdevice = NULL;
	uint8_t remote_name_[REMOTE_NAME_SIZE] = {0};

};


/************************************************/
/*  USB Device Drivers                          */
/************************************************/

class USBHub : public USBDriver {
public:
	USBHub(USBHost &host) : debouncetimer(this), resettimer(this) { init(); }
	USBHub(USBHost *host) : debouncetimer(this), resettimer(this) { init(); }
	// Hubs with more more than 7 ports are built from two tiers of hubs
	// using 4 or 7 port hub chips.  While the USB spec seems to allow
	// hubs to have up to 255 ports, in practice all hub chips on the
	// market are only 2, 3, 4 or 7 ports.
	enum { MAXPORTS = 7 };
	typedef uint8_t portbitmask_t;
	enum {
		PORT_OFF =        0,
		PORT_DISCONNECT = 1,
		PORT_DEBOUNCE1 =  2,
		PORT_DEBOUNCE2 =  3,
		PORT_DEBOUNCE3 =  4,
		PORT_DEBOUNCE4 =  5,
		PORT_DEBOUNCE5 =  6,
		PORT_RESET =      7,
		PORT_RECOVERY =   8,
		PORT_ACTIVE =     9
	};
protected:
	virtual bool claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len);
	virtual void control(const Transfer_t *transfer);
	virtual void timer_event(USBDriverTimer *whichTimer);
	virtual void disconnect();
	void init();
	bool can_send_control_now();
	void send_poweron(uint32_t port);
	void send_getstatus(uint32_t port);
	void send_clearstatus_connect(uint32_t port);
	void send_clearstatus_enable(uint32_t port);
	void send_clearstatus_suspend(uint32_t port);
	void send_clearstatus_overcurrent(uint32_t port);
	void send_clearstatus_reset(uint32_t port);
	void send_setreset(uint32_t port);
	void send_setinterface();
	static void callback(const Transfer_t *transfer);
	void status_change(const Transfer_t *transfer);
	void new_port_status(uint32_t port, uint32_t status);
	void start_debounce_timer(uint32_t port);
	void stop_debounce_timer(uint32_t port);
private:
	Device_t mydevices[MAXPORTS];
	Pipe_t mypipes[2] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[4] __attribute__ ((aligned(32)));
	strbuf_t mystring_bufs[1];
	USBDriverTimer debouncetimer;
	USBDriverTimer resettimer;
	setup_t setup;
	Pipe_t *changepipe;
	Device_t *devicelist[MAXPORTS];
	uint32_t changebits;
	uint32_t statusbits;
	uint8_t  hub_desc[16];
	uint8_t  interface_count;
	uint8_t  interface_number;
	uint8_t  altsetting;
	uint8_t  protocol;
	uint8_t  endpoint;
	uint8_t  interval;
	uint8_t  numports;
	uint8_t  characteristics;
	uint8_t  powertime;
	uint8_t  sending_control_transfer;
	uint8_t  port_doing_reset;
	uint8_t  port_doing_reset_speed;
	uint8_t  portstate[MAXPORTS];
	portbitmask_t send_pending_poweron;
	portbitmask_t send_pending_getstatus;
	portbitmask_t send_pending_clearstatus_connect;
	portbitmask_t send_pending_clearstatus_enable;
	portbitmask_t send_pending_clearstatus_suspend;
	portbitmask_t send_pending_clearstatus_overcurrent;
	portbitmask_t send_pending_clearstatus_reset;
	portbitmask_t send_pending_setreset;
	portbitmask_t debounce_in_use;
	static volatile bool reset_busy;
};

//--------------------------------------------------------------------------


class USBHIDParser : public USBDriver {
public:
	USBHIDParser(USBHost &host) : hidTimer(this), usb_host_port(host.usb_host_port) { init(); }
	static void driver_ready_for_hid_collection(USBHIDInput *driver);
	bool sendPacket(const uint8_t *buffer, int cb=-1);
	void setTXBuffers(uint8_t *buffer1, uint8_t *buffer2, uint8_t cb);

	bool sendControlPacket(uint32_t bmRequestType, uint32_t bRequest,
			uint32_t wValue, uint32_t wIndex, uint32_t wLength, void *buf);

	// Atempt for RAWhid and SEREMU to take over processing of data 
	// 
	uint16_t inSize(void) {return in_size;}
	uint16_t outSize(void) {return out_size;}
	void startTimer(uint32_t microseconds) {hidTimer.start(microseconds);}
	void stopTimer() {hidTimer.stop();}
	uint8_t interfaceNumber() { return bInterfaceNumber;}
protected:
	enum { TOPUSAGE_LIST_LEN = 4 };
	enum { USAGE_LIST_LEN = 24 };
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void control(const Transfer_t *transfer);
	virtual void disconnect();
	static void in_callback(const Transfer_t *transfer);
	static void out_callback(const Transfer_t *transfer);
	virtual void timer_event(USBDriverTimer *whichTimer);
	void in_data(const Transfer_t *transfer);
	void out_data(const Transfer_t *transfer);
	bool check_if_using_report_id();
	void parse();
	USBHIDInput * find_driver(uint32_t topusage);
	void parse(uint16_t type_and_report_id, const uint8_t *data, uint32_t len);
	void init();


	uint8_t activeSendMask(void) {return txstate;} 

private:
	Pipe_t *in_pipe;
	Pipe_t *out_pipe;
	static USBHIDInput *available_hid_drivers_list;
	//uint32_t topusage_list[TOPUSAGE_LIST_LEN];
	USBHIDInput *topusage_drivers[TOPUSAGE_LIST_LEN];
	uint16_t in_size;
	uint16_t out_size;
	setup_t setup;
	uint8_t descriptor[800];
	uint8_t report[64];
	uint16_t descsize;
	bool use_report_id;
	Pipe_t mypipes[3] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[4] __attribute__ ((aligned(32)));
	strbuf_t mystring_bufs[1];
	uint8_t txstate = 0;
	uint8_t *tx1 = nullptr;
	uint8_t *tx2 = nullptr;
	bool hid_driver_claimed_control_ = false;
	USBDriverTimer hidTimer;
	uint8_t bInterfaceNumber = 0;
	USBHostPort *usb_host_port;
};

//--------------------------------------------------------------------------

class KeyboardController : public USBDriver , public USBHIDInput, public BTHIDInput {
public:
typedef union {
   struct {
        uint8_t numLock : 1;
        uint8_t capsLock : 1;
        uint8_t scrollLock : 1;
        uint8_t compose : 1;
        uint8_t kana : 1;
        uint8_t reserved : 3;
        };
    uint8_t byte;
} KBDLeds_t;
public:
	KeyboardController(USBHost &host) { init(); }
	KeyboardController(USBHost *host) { init(); }

	// need their own versions as both USBDriver and USBHIDInput provide
	uint16_t idVendor();
	uint16_t idProduct();
	const uint8_t *manufacturer();
	const uint8_t *product();
	const uint8_t *serialNumber();

	operator bool() { return ((device != nullptr) || (btdevice != nullptr)); }
	// Main boot keyboard functions. 
	uint16_t getKey() { return keyCode; }
	uint8_t  getModifiers() { return modifiers; }
	uint8_t  getOemKey() { return keyOEM; }
	void     attachPress(void (*f)(int unicode)) { keyPressedFunction = f; }
	void     attachRelease(void (*f)(int unicode)) { keyReleasedFunction = f; }
	void     attachRawPress(void (*f)(uint8_t keycode)) { rawKeyPressedFunction = f; }
	void     attachRawRelease(void (*f)(uint8_t keycode)) { rawKeyReleasedFunction = f; }
	void     LEDS(uint8_t leds);
	uint8_t  LEDS() {return leds_.byte;}
	void     updateLEDS(void);
	bool     numLock() {return leds_.numLock;}
	bool     capsLock() {return leds_.capsLock;}
	bool     scrollLock() {return leds_.scrollLock;}
	void	 numLock(bool f);
	void     capsLock(bool f);
	void	 scrollLock(bool f);

	// Added for extras information.
	void     attachExtrasPress(void (*f)(uint32_t top, uint16_t code)) { extrasKeyPressedFunction = f; }
	void     attachExtrasRelease(void (*f)(uint32_t top, uint16_t code)) { extrasKeyReleasedFunction = f; }
	void	 forceBootProtocol();
	enum {MAX_KEYS_DOWN=4};


protected:
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void control(const Transfer_t *transfer);
	virtual void disconnect();
	static void callback(const Transfer_t *transfer);
	void new_data(const Transfer_t *transfer);
	void init();

	// Bluetooth data
	virtual bool claim_bluetooth(BluetoothController *driver, uint32_t bluetooth_class, uint8_t *remoteName);
	virtual bool process_bluetooth_HID_data(const uint8_t *data, uint16_t length);
	virtual bool remoteNameComplete(const uint8_t *remoteName);
	virtual void release_bluetooth();


protected:	// HID functions for extra keyboard data. 
	virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
	virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
	virtual void hid_input_data(uint32_t usage, int32_t value);
	virtual void hid_input_end();
	virtual void disconnect_collection(Device_t *dev);

private:
	void update();
	uint16_t convert_to_unicode(uint32_t mod, uint32_t key);
	void key_press(uint32_t mod, uint32_t key);
	void key_release(uint32_t mod, uint32_t key);
	void (*keyPressedFunction)(int unicode);
	void (*keyReleasedFunction)(int unicode);
	void (*rawKeyPressedFunction)(uint8_t keycode) = nullptr;
	void (*rawKeyReleasedFunction)(uint8_t keycode) = nullptr;
	Pipe_t *datapipe;
	setup_t setup;
	uint8_t report[8];
	uint16_t keyCode;
	uint8_t modifiers;
	uint8_t keyOEM;
	uint8_t prev_report[8];
	KBDLeds_t leds_ = {0};
	Pipe_t mypipes[2] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[4] __attribute__ ((aligned(32)));
	strbuf_t mystring_bufs[1];

	// Added to process secondary HID data. 
	void (*extrasKeyPressedFunction)(uint32_t top, uint16_t code);
	void (*extrasKeyReleasedFunction)(uint32_t top, uint16_t code);
	uint32_t topusage_ = 0;					// What top report am I processing?
	uint8_t collections_claimed_ = 0;
	volatile bool hid_input_begin_ = false;
	volatile bool hid_input_data_ = false; 	// did we receive any valid data with report?
	uint8_t count_keys_down_ = 0;
	uint16_t keys_down[MAX_KEYS_DOWN];
	bool 	force_boot_protocol;  // User or VID/PID said force boot protocol?
	bool control_queued;
};


class MouseController : public USBHIDInput, public BTHIDInput {
public:
	MouseController(USBHost &host) { init(); }
	bool	available() { return mouseEvent; }
	void	mouseDataClear();
	uint8_t getButtons() { return buttons; }
	int     getMouseX() { return mouseX; }
	int     getMouseY() { return mouseY; }
	int     getWheel() { return wheel; }
	int     getWheelH() { return wheelH; }
protected:
	virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
	virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
	virtual void hid_input_data(uint32_t usage, int32_t value);
	virtual void hid_input_end();
	virtual void disconnect_collection(Device_t *dev);

	// Bluetooth data
	virtual bool claim_bluetooth(BluetoothController *driver, uint32_t bluetooth_class, uint8_t *remoteName);
	virtual bool process_bluetooth_HID_data(const uint8_t *data, uint16_t length);
	virtual void release_bluetooth();


private:
	void init();
	BluetoothController *btdriver_ = nullptr;

	uint8_t collections_claimed = 0;
	volatile bool mouseEvent = false;
	volatile bool hid_input_begin_ = false;
	uint8_t buttons = 0;
	int     mouseX = 0;
	int     mouseY = 0;
	int     wheel = 0;
	int     wheelH = 0;
};

//--------------------------------------------------------------------------

class DigitizerController : public USBHIDInput, public BTHIDInput {
public:
	DigitizerController(USBHost &host) { init(); }
	bool	available() { return digitizerEvent; }
	void	digitizerDataClear();
	uint8_t getButtons() { return buttons; }
	int     getMouseX() { return mouseX; }
	int     getMouseY() { return mouseY; }
	int     getWheel() { return wheel; }
	int     getWheelH() { return wheelH; }
	int		getAxis(uint32_t index) { return (index < (sizeof(digiAxes)/sizeof(digiAxes[0]))) ? digiAxes[index] : 0; }

protected:
	virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
	virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
	virtual void hid_input_data(uint32_t usage, int32_t value);
	virtual void hid_input_end();
	virtual void disconnect_collection(Device_t *dev);


private:
	void init();

	uint8_t collections_claimed = 0;
	volatile bool digitizerEvent = false;
	volatile bool hid_input_begin_ = false;
	uint8_t buttons = 0;
	int     mouseX = 0;
	int     mouseY = 0;
	int     wheel = 0;
	int     wheelH = 0;
	int     digiAxes[16];
};


//--------------------------------------------------------------------------

class JoystickController : public USBDriver, public USBHIDInput, public BTHIDInput {
public:
	JoystickController(USBHost &host) : usb_host_port(host.usb_host_port) { init(); }

	uint16_t idVendor();
	uint16_t idProduct();

	const uint8_t *manufacturer();
	const uint8_t *product();
	const uint8_t *serialNumber();
	operator bool() { return (((device != nullptr) || (mydevice != nullptr || (btdevice != nullptr))) && connected_); }	// override as in both USBDriver and in USBHIDInput

	bool    available() { return joystickEvent; }
	void    joystickDataClear();
	uint32_t getButtons() { return buttons; }
	int		getAxis(uint32_t index) { return (index < (sizeof(axis)/sizeof(axis[0]))) ? axis[index] : 0; }
	uint64_t axisMask() {return axis_mask_;}
	uint64_t axisChangedMask() { return axis_changed_mask_;}
	uint64_t axisChangeNotifyMask() {return axis_change_notify_mask_;}
	void 	 axisChangeNotifyMask(uint64_t notify_mask) {axis_change_notify_mask_ = notify_mask;}

	// set functions functionality depends on underlying joystick. 
    bool setRumble(uint8_t lValue, uint8_t rValue, uint8_t timeout=0xff);
    // setLEDs on PS4(RGB), PS3 simple LED setting (only uses lb)
    bool setLEDs(uint8_t lr, uint8_t lg, uint8_t lb);  // sets Leds, 
    bool inline setLEDs(uint32_t leds) {return setLEDs((leds >> 16) & 0xff, (leds >> 8) & 0xff, leds & 0xff);}  // sets Leds - passing one arg for all leds 
	enum { STANDARD_AXIS_COUNT = 10, ADDITIONAL_AXIS_COUNT = 54, TOTAL_AXIS_COUNT = (STANDARD_AXIS_COUNT+ADDITIONAL_AXIS_COUNT) };
	typedef enum { UNKNOWN=0, PS3, PS4, XBOXONE, XBOX360, PS3_MOTION, SpaceNav, SWITCH} joytype_t;
	joytype_t joystickType() {return joystickType_;} 

	// PS3 pair function. hack, requires that it be connect4ed by USB and we have the address of the Bluetooth dongle...
	bool PS3Pair(uint8_t* bdaddr);

	
	
protected:
	// From USBDriver
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void control(const Transfer_t *transfer);
	virtual void disconnect();

	// From USBHIDInput
	virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
	virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
	virtual void hid_input_data(uint32_t usage, int32_t value);
	virtual void hid_input_end();
	virtual void disconnect_collection(Device_t *dev);
	virtual bool hid_process_out_data(const Transfer_t *transfer);

		// Bluetooth data
	virtual bool claim_bluetooth(BluetoothController *driver, uint32_t bluetooth_class, uint8_t *remoteName);
	virtual bool process_bluetooth_HID_data(const uint8_t *data, uint16_t length);
	virtual void release_bluetooth();
	virtual bool remoteNameComplete(const uint8_t *remoteName);
	virtual void connectionComplete(void);

	joytype_t joystickType_ = UNKNOWN;
private:

	// Class specific
	void init();
	USBHIDParser *driver_ = nullptr;
	BluetoothController *btdriver_ = nullptr;

	joytype_t mapVIDPIDtoJoystickType(uint16_t idVendor, uint16_t idProduct, bool exclude_hid_devices);
	bool transmitPS4UserFeedbackMsg();
	bool transmitPS3UserFeedbackMsg();
	bool transmitPS3MotionUserFeedbackMsg();
	bool mapNameToJoystickType(const uint8_t *remoteName);

	bool anychange = false;
	volatile bool joystickEvent = false;
	uint32_t buttons = 0;
	int axis[TOTAL_AXIS_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	uint64_t axis_mask_ = 0;	// which axis have valid data
	uint64_t axis_changed_mask_ = 0;
	uint64_t axis_change_notify_mask_ = 0x3ff;	// assume the low 10 values only. 

	uint16_t additional_axis_usage_page_ = 0;
	uint16_t additional_axis_usage_start_ = 0;
	uint16_t additional_axis_usage_count_ = 0;

	// State values to output to Joystick.
	uint8_t rumble_lValue_ = 0; 
	uint8_t rumble_rValue_ = 0;
	uint8_t rumble_timeout_ = 0;
	uint8_t leds_[3] = {0,0,0};
	uint8_t connected_ = 0;	// what type of device if any is connected xbox 360... 


	// Used by HID code
	uint8_t collections_claimed = 0;

	// Used by USBDriver code
	static void rx_callback(const Transfer_t *transfer);
	static void tx_callback(const Transfer_t *transfer);
	void rx_data(const Transfer_t *transfer);
	void tx_data(const Transfer_t *transfer);

	Pipe_t mypipes[3] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[7] __attribute__ ((aligned(32)));
	strbuf_t mystring_bufs[1];

	uint8_t			rx_ep_ = 0;	// remember which end point this object is...
	uint16_t 		rx_size_ = 0;
	uint16_t 		tx_size_ = 0;
	Pipe_t 			*rxpipe_;
	Pipe_t 			*txpipe_;
	uint8_t 		rxbuf_[64];	// receive circular buffer
	uint8_t			txbuf_[64];		// buffer to use to send commands to joystick 
	// Mapping table to say which devices we handle
	typedef struct {
		uint16_t 	idVendor;
		uint16_t 	idProduct;
		joytype_t	joyType;
		bool 		hidDevice;
	} product_vendor_mapping_t;
	static product_vendor_mapping_t pid_vid_mapping[];

	USBHostPort *usb_host_port;
};


//--------------------------------------------------------------------------

class MIDIDeviceBase : public USBDriver {
public:
	enum { SYSEX_MAX_LEN = 290 };

	// Message type names for compatibility with Arduino MIDI library 4.3.1
	enum MidiType {
		InvalidType           = 0x00, // For notifying errors
		NoteOff               = 0x80, // Note Off
		NoteOn                = 0x90, // Note On
		AfterTouchPoly        = 0xA0, // Polyphonic AfterTouch
		ControlChange         = 0xB0, // Control Change / Channel Mode
		ProgramChange         = 0xC0, // Program Change
		AfterTouchChannel     = 0xD0, // Channel (monophonic) AfterTouch
		PitchBend             = 0xE0, // Pitch Bend
		SystemExclusive       = 0xF0, // System Exclusive
		TimeCodeQuarterFrame  = 0xF1, // System Common - MIDI Time Code Quarter Frame
		SongPosition          = 0xF2, // System Common - Song Position Pointer
		SongSelect            = 0xF3, // System Common - Song Select
		TuneRequest           = 0xF6, // System Common - Tune Request
		Clock                 = 0xF8, // System Real Time - Timing Clock
		Start                 = 0xFA, // System Real Time - Start
		Continue              = 0xFB, // System Real Time - Continue
		Stop                  = 0xFC, // System Real Time - Stop
		ActiveSensing         = 0xFE, // System Real Time - Active Sensing
		SystemReset           = 0xFF, // System Real Time - System Reset
	};
	MIDIDeviceBase(USBHost &host, uint32_t *rx, uint32_t *tx1, uint32_t *tx2,
		uint16_t bufsize, uint32_t *rqueue, uint16_t qsize) :
			txtimer(this), rx_buffer(rx), tx_buffer1(tx1), tx_buffer2(tx2),
			rx_queue(rqueue), max_packet_size(bufsize), rx_queue_size(qsize),
			usb_host_port(host.usb_host_port) {
				init();
		}
	void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel, uint8_t cable=0) {
		send(0x80, note, velocity, channel, cable);
	}
	void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel, uint8_t cable=0) {
		send(0x90, note, velocity, channel, cable);
	}
	void sendPolyPressure(uint8_t note, uint8_t pressure, uint8_t channel, uint8_t cable=0) {
		send(0xA0, note, pressure, channel, cable);
	}
	void sendAfterTouchPoly(uint8_t note, uint8_t pressure, uint8_t channel, uint8_t cable=0) {
		send(0xA0, note, pressure, channel, cable);
	}
	void sendControlChange(uint8_t control, uint8_t value, uint8_t channel, uint8_t cable=0) {
		send(0xB0, control, value, channel, cable);
	}
	void sendProgramChange(uint8_t program, uint8_t channel, uint8_t cable=0) {
		send(0xC0, program, 0, channel, cable);
	}
	void sendAfterTouch(uint8_t pressure, uint8_t channel, uint8_t cable=0) {
		send(0xD0, pressure, 0, channel, cable);
	}
	void sendPitchBend(int value, uint8_t channel, uint8_t cable=0) {
		if (value < -8192) {
			value = -8192;
		} else if (value > 8191) {
			value = 8191;
		}
		value += 8192;
		send(0xE0, value, value >> 7, channel, cable);
	}
	void sendSysEx(uint32_t length, const uint8_t *data, bool hasTerm=false, uint8_t cable=0) {
		//if (cable >= MIDI_NUM_CABLES) return;
		if (hasTerm) {
			send_sysex_buffer_has_term(data, length, cable);
		} else {
			send_sysex_add_term_bytes(data, length, cable);
		}
	}
	void sendRealTime(uint8_t type, uint8_t cable=0) {
		switch (type) {
			case 0xF8: // Clock
			case 0xFA: // Start
			case 0xFB: // Continue
			case 0xFC: // Stop
			case 0xFE: // ActiveSensing
			case 0xFF: // SystemReset
				send(type, 0, 0, 0, cable);
				break;
			default: // Invalid Real Time marker
				break;
		}
	}
	void sendTimeCodeQuarterFrame(uint8_t type, uint8_t value, uint8_t cable=0) {
		send(0xF1, ((type & 0x07) << 4) | (value & 0x0F), 0, 0, cable);
	}
	void sendSongPosition(uint16_t beats, uint8_t cable=0) {
		send(0xF2, beats, beats >> 7, 0, cable);
	}
	void sendSongSelect(uint8_t song, uint8_t cable=0) {
		send(0xF3, song, 0, 0, cable);
	}
	void sendTuneRequest(uint8_t cable=0) {
		send(0xF6, 0, 0, 0, cable);
	}
	void beginRpn(uint16_t number, uint8_t channel, uint8_t cable=0) {
		sendControlChange(101, number >> 7, channel, cable);
		sendControlChange(100, number, channel, cable);
	}
	void sendRpnValue(uint16_t value, uint8_t channel, uint8_t cable=0) {
		sendControlChange(6, value >> 7, channel, cable);
		sendControlChange(38, value, channel, cable);
	}
	void sendRpnIncrement(uint8_t amount, uint8_t channel, uint8_t cable=0) {
		sendControlChange(96, amount, channel, cable);
	}
	void sendRpnDecrement(uint8_t amount, uint8_t channel, uint8_t cable=0) {
		sendControlChange(97, amount, channel, cable);
	}
	void endRpn(uint8_t channel, uint8_t cable=0) {
		sendControlChange(101, 0x7F, channel, cable);
		sendControlChange(100, 0x7F, channel, cable);
	}
	void beginNrpn(uint16_t number, uint8_t channel, uint8_t cable=0) {
		sendControlChange(99, number >> 7, channel, cable);
		sendControlChange(98, number, channel, cable);
	}
	void sendNrpnValue(uint16_t value, uint8_t channel, uint8_t cable=0) {
		sendControlChange(6, value >> 7, channel, cable);
		sendControlChange(38, value, channel, cable);
	}
	void sendNrpnIncrement(uint8_t amount, uint8_t channel, uint8_t cable=0) {
		sendControlChange(96, amount, channel, cable);
	}
	void sendNrpnDecrement(uint8_t amount, uint8_t channel, uint8_t cable=0) {
		sendControlChange(97, amount, channel, cable);
	}
	void endNrpn(uint8_t channel, uint8_t cable=0) {
		sendControlChange(99, 0x7F, channel, cable);
		sendControlChange(98, 0x7F, channel, cable);
	}
	void send(uint8_t type, uint8_t data1, uint8_t data2, uint8_t channel, uint8_t cable=0) {
		//if (cable >= MIDI_NUM_CABLES) return;
		if (type < 0xF0) {
			if (type < 0x80) return;
			type &= 0xF0;
			write_packed((type << 8) | (type >> 4) | ((cable & 0x0F) << 4)
			  | (((channel - 1) & 0x0F) << 8) | ((data1 & 0x7F) << 16)
			  | ((data2 & 0x7F) << 24));
		} else if (type >= 0xF8 || type == 0xF6) {
			write_packed((type << 8) | 0x0F | ((cable & 0x0F) << 4));
		} else if (type == 0xF1 || type == 0xF3) {
			write_packed((type << 8) | 0x02 | ((cable & 0x0F) << 4)
			  | ((data1 & 0x7F) << 16));
		} else if (type == 0xF2) {
			write_packed((type << 8) | 0x03 | ((cable & 0x0F) << 4)
			  | ((data1 & 0x7F) << 16) | ((data2 & 0x7F) << 24));
		}
	}
	void send_now(void) __attribute__((always_inline)) {
	}
	bool read(uint8_t channel=0);
	uint8_t getType(void) {
		return msg_type;
	};
	uint8_t getCable(void) {
		return msg_cable;
	}
	uint8_t getChannel(void) {
		return msg_channel;
	};
	uint8_t getData1(void) {
		return msg_data1;
	};
	uint8_t getData2(void) {
		return msg_data2;
	};
	uint8_t * getSysExArray(void) {
		return msg_sysex;
	}
	uint16_t getSysExArrayLength(void) {
		return msg_data2 << 8 | msg_data1;
	}
	void setHandleNoteOff(void (*fptr)(uint8_t channel, uint8_t note, uint8_t velocity)) {
		// type: 0x80  NoteOff
		handleNoteOff = fptr;
	}
	void setHandleNoteOn(void (*fptr)(uint8_t channel, uint8_t note, uint8_t velocity)) {
		// type: 0x90  NoteOn
		handleNoteOn = fptr;
	}
	void setHandleVelocityChange(void (*fptr)(uint8_t channel, uint8_t note, uint8_t velocity)) {
		// type: 0xA0  AfterTouchPoly
		handleVelocityChange = fptr;
	}
	void setHandleAfterTouchPoly(void (*fptr)(uint8_t channel, uint8_t note, uint8_t pressure)) {
		// type: 0xA0  AfterTouchPoly
		handleVelocityChange = fptr;
	}
	void setHandleControlChange(void (*fptr)(uint8_t channel, uint8_t control, uint8_t value)) {
		// type: 0xB0  ControlChange
		handleControlChange = fptr;
	}
	void setHandleProgramChange(void (*fptr)(uint8_t channel, uint8_t program)) {
		// type: 0xC0  ProgramChange
		handleProgramChange = fptr;
	}
	void setHandleAfterTouch(void (*fptr)(uint8_t channel, uint8_t pressure)) {
		// type: 0xD0  AfterTouchChannel
		handleAfterTouch = fptr;
	}
	void setHandleAfterTouchChannel(void (*fptr)(uint8_t channel, uint8_t pressure)) {
		// type: 0xD0  AfterTouchChannel
		handleAfterTouch = fptr;
	}
	void setHandlePitchChange(void (*fptr)(uint8_t channel, int pitch)) {
		// type: 0xE0  PitchBend
		handlePitchChange = fptr;
	}
	void setHandleSysEx(void (*fptr)(const uint8_t *data, uint16_t length, bool complete)) {
		// type: 0xF0  SystemExclusive - multiple calls for message bigger than buffer
		handleSysExPartial = (void (*)(const uint8_t *, uint16_t, uint8_t))fptr;
	}
	void setHandleSystemExclusive(void (*fptr)(const uint8_t *data, uint16_t length, bool complete)) {
		// type: 0xF0  SystemExclusive - multiple calls for message bigger than buffer
		handleSysExPartial = (void (*)(const uint8_t *, uint16_t, uint8_t))fptr;
	}
	void setHandleSystemExclusive(void (*fptr)(uint8_t *data, unsigned int size)) {
		// type: 0xF0  SystemExclusive - single call, message larger than buffer is truncated
		handleSysExComplete = fptr;
	}
	void setHandleTimeCodeQuarterFrame(void (*fptr)(uint8_t data)) {
		// type: 0xF1  TimeCodeQuarterFrame
		handleTimeCodeQuarterFrame = fptr;
	}
	void setHandleSongPosition(void (*fptr)(uint16_t beats)) {
		// type: 0xF2  SongPosition
		handleSongPosition = fptr;
	}
	void setHandleSongSelect(void (*fptr)(uint8_t songnumber)) {
		// type: 0xF3  SongSelect
		handleSongSelect = fptr;
	}
	void setHandleTuneRequest(void (*fptr)(void)) {
		// type: 0xF6  TuneRequest
		handleTuneRequest = fptr;
	}
	void setHandleClock(void (*fptr)(void)) {
		// type: 0xF8  Clock
		handleClock = fptr;
	}
	void setHandleStart(void (*fptr)(void)) {
		// type: 0xFA  Start
		handleStart = fptr;
	}
	void setHandleContinue(void (*fptr)(void)) {
		// type: 0xFB  Continue
		handleContinue = fptr;
	}
	void setHandleStop(void (*fptr)(void)) {
		// type: 0xFC  Stop
		handleStop = fptr;
	}
	void setHandleActiveSensing(void (*fptr)(void)) {
		// type: 0xFE  ActiveSensing
		handleActiveSensing = fptr;
	}
	void setHandleSystemReset(void (*fptr)(void)) {
		// type: 0xFF  SystemReset
		handleSystemReset = fptr;
	}
	void setHandleRealTimeSystem(void (*fptr)(uint8_t realtimebyte)) {
		// type: 0xF8-0xFF - if more specific handler not configured
		handleRealTimeSystem = fptr;
	}
protected:
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void disconnect();
	virtual void timer_event(USBDriverTimer *timer);
	static void rx_callback(const Transfer_t *transfer);
	static void tx_callback(const Transfer_t *transfer);
	void rx_data(const Transfer_t *transfer);
	void tx_data(const Transfer_t *transfer);
	void init();
	void write_packed(uint32_t data);
	void send_sysex_buffer_has_term(const uint8_t *data, uint32_t length, uint8_t cable);
	void send_sysex_add_term_bytes(const uint8_t *data, uint32_t length, uint8_t cable);
	void sysex_byte(uint8_t b);
private:
	Pipe_t *rxpipe;
	Pipe_t *txpipe;
	USBDriverTimer txtimer;
	//enum { MAX_PACKET_SIZE = 64 };
	//enum { RX_QUEUE_SIZE = 80 }; // must be more than MAX_PACKET_SIZE/4
	//uint32_t rx_buffer[MAX_PACKET_SIZE/4];
	//uint32_t tx_buffer1[MAX_PACKET_SIZE/4];
	//uint32_t tx_buffer2[MAX_PACKET_SIZE/4];
	uint32_t * const rx_buffer;
	uint32_t * const tx_buffer1;
	uint32_t * const tx_buffer2;
	uint16_t rx_size;
	uint16_t tx_size;
	//uint32_t rx_queue[RX_QUEUE_SIZE];
	uint32_t * const rx_queue;
	bool rx_packet_queued;
	const uint16_t max_packet_size;
	const uint16_t rx_queue_size;
	uint16_t rx_head;
	uint16_t rx_tail;
	volatile uint8_t tx1_count;
	volatile uint8_t tx2_count;
	uint8_t rx_ep;
	uint8_t tx_ep;
	uint8_t rx_ep_type;
	uint8_t tx_ep_type;
	uint8_t msg_cable;
	uint8_t msg_channel;
	uint8_t msg_type;
	uint8_t msg_data1;
	uint8_t msg_data2;
	uint8_t msg_sysex[SYSEX_MAX_LEN];
	uint16_t msg_sysex_len;
	void (*handleNoteOff)(uint8_t ch, uint8_t note, uint8_t vel);
	void (*handleNoteOn)(uint8_t ch, uint8_t note, uint8_t vel);
	void (*handleVelocityChange)(uint8_t ch, uint8_t note, uint8_t vel);
	void (*handleControlChange)(uint8_t ch, uint8_t control, uint8_t value);
	void (*handleProgramChange)(uint8_t ch, uint8_t program);
	void (*handleAfterTouch)(uint8_t ch, uint8_t pressure);
	void (*handlePitchChange)(uint8_t ch, int pitch);
	void (*handleSysExPartial)(const uint8_t *data, uint16_t length, uint8_t complete);
	void (*handleSysExComplete)(uint8_t *data, unsigned int size);
	void (*handleTimeCodeQuarterFrame)(uint8_t data);
	void (*handleSongPosition)(uint16_t beats);
	void (*handleSongSelect)(uint8_t songnumber);
	void (*handleTuneRequest)(void);
	void (*handleClock)(void);
	void (*handleStart)(void);
	void (*handleContinue)(void);
	void (*handleStop)(void);
	void (*handleActiveSensing)(void);
	void (*handleSystemReset)(void);
	void (*handleRealTimeSystem)(uint8_t rtb);
	Pipe_t mypipes[3] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[7] __attribute__ ((aligned(32)));
	strbuf_t mystring_bufs[1];
    USBHostPort *usb_host_port;
};

class MIDIDevice : public MIDIDeviceBase {
public:
	MIDIDevice(USBHost &host) :
		MIDIDeviceBase(host, rx, tx1, tx2, MAX_PACKET_SIZE, queue, RX_QUEUE_SIZE) {};
	// MIDIDevice(USBHost *host) : ....
private:
	enum { MAX_PACKET_SIZE = 64 };
	enum { RX_QUEUE_SIZE = 80 }; // must be more than MAX_PACKET_SIZE/4
	uint32_t rx[MAX_PACKET_SIZE/4];
	uint32_t tx1[MAX_PACKET_SIZE/4];
	uint32_t tx2[MAX_PACKET_SIZE/4];
	uint32_t queue[RX_QUEUE_SIZE];
};

class MIDIDevice_BigBuffer : public MIDIDeviceBase {
public:
	MIDIDevice_BigBuffer(USBHost &host) :
		MIDIDeviceBase(host, rx, tx1, tx2, MAX_PACKET_SIZE, queue, RX_QUEUE_SIZE) {};
	// MIDIDevice(USBHost *host) : ....
private:
	enum { MAX_PACKET_SIZE = 512 };
	enum { RX_QUEUE_SIZE = 400 }; // must be more than MAX_PACKET_SIZE/4
	uint32_t rx[MAX_PACKET_SIZE/4];
	uint32_t tx1[MAX_PACKET_SIZE/4];
	uint32_t tx2[MAX_PACKET_SIZE/4];
	uint32_t queue[RX_QUEUE_SIZE];
};


//--------------------------------------------------------------------------

class USBSerialBase: public USBDriver, public Stream {
	public:

	// FIXME: need different USBSerial, with bigger buffers for 480 Mbit & faster speed
	enum { BUFFER_SIZE = 648 }; // must hold at least 6 max size packets, plus 2 extra bytes
	enum { DEFAULT_WRITE_TIMEOUT = 3500};

	USBSerialBase(USBHost &host, uint32_t *big_buffer, uint16_t buffer_size, 
		uint16_t min_pipe_rxtx, uint16_t max_pipe_rxtx) :
			usb_host_port(host.usb_host_port),
			txtimer(this), 
			_bigBuffer(big_buffer), 
			_big_buffer_size(buffer_size), 
			_min_rxtx(min_pipe_rxtx), 
			_max_rxtx(max_pipe_rxtx)
		{ 
			init(); 
		}

	void begin(uint32_t baud, uint32_t format=USBHOST_SERIAL_8N1);
	void end(void);
	uint32_t writeTimeout() {return write_timeout_;}
	void writeTimeOut(uint32_t write_timeout) {write_timeout_ = write_timeout;} // Will not impact current ones.
	virtual int available(void);
	virtual int peek(void);
	virtual int read(void);
	virtual int availableForWrite();
	virtual size_t write(uint8_t c);
	virtual void flush(void);

	using Print::write;
protected:
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void control(const Transfer_t *transfer);
	virtual void disconnect();
	virtual void timer_event(USBDriverTimer *whichTimer);
private:
	static void rx_callback(const Transfer_t *transfer);
	static void tx_callback(const Transfer_t *transfer);
	void rx_data(const Transfer_t *transfer);
	void tx_data(const Transfer_t *transfer);
	void rx_queue_packets(uint32_t head, uint32_t tail);
	void init();
	static bool check_rxtx_ep(uint32_t &rxep, uint32_t &txep);
	bool init_buffers(uint32_t rsize, uint32_t tsize);
	void ch341_setBaud(uint8_t byte_index);
private:
	USBHostPort *usb_host_port;
	Pipe_t mypipes[3] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[7] __attribute__ ((aligned(32)));
	strbuf_t mystring_bufs[1];
	USBDriverTimer txtimer;
	uint32_t *_bigBuffer;
	uint16_t _big_buffer_size;
	uint16_t  _min_rxtx;
	uint16_t _max_rxtx;

	setup_t setup;
	uint8_t setupdata[16]; // 
	uint32_t baudrate;
	uint32_t format_;
	uint32_t write_timeout_ = DEFAULT_WRITE_TIMEOUT;
	Pipe_t *rxpipe;
	Pipe_t *txpipe;
	uint8_t *rx1;	// location for first incoming packet
	uint8_t *rx2;	// location for second incoming packet
	uint8_t *rxbuf;	// receive circular buffer
	uint8_t *tx1;	// location for first outgoing packet
	uint8_t *tx2;	// location for second outgoing packet
	uint8_t *txbuf;
	volatile uint16_t rxhead;// receive head
	volatile uint16_t rxtail;// receive tail
	volatile uint16_t txhead;
	volatile uint16_t txtail;
	uint16_t rxsize;// size of receive circular buffer
	uint16_t txsize;// size of transmit circular buffer
	volatile uint8_t  rxstate;// bitmask: which receive packets are queued
	volatile uint8_t  txstate;
	uint8_t pending_control;
	uint8_t setup_state;	// PL2303 - has several steps... Could use pending control?
	uint8_t pl2303_v1;		// Which version do we have
	uint8_t pl2303_v2;
	uint8_t interface;
	bool 	control_queued;	// Is there already a queued control messaged
	typedef enum { UNKNOWN=0, CDCACM, FTDI, PL2303, CH341, CP210X } sertype_t;
	sertype_t sertype;

	typedef struct {
		uint16_t 	idVendor;
		uint16_t 	idProduct;
		sertype_t 	sertype;
		int			claim_at_type;
	} product_vendor_mapping_t;
	static product_vendor_mapping_t pid_vid_mapping[];

};

class USBSerial : public USBSerialBase {
public:
	USBSerial(USBHost &host) :
		// hard code the normal one to 1 and 64 bytes for most likely most are 64
		USBSerialBase(host, bigbuffer, sizeof(bigbuffer), 1, 64) {};
private:
	enum { BUFFER_SIZE = 648 }; // must hold at least 6 max size packets, plus 2 extra bytes
	uint32_t bigbuffer[(BUFFER_SIZE+3)/4];
};

class USBSerial_BigBuffer: public USBSerialBase {
public:
	// Default to larger than can be handled by other serial, but can overide
	USBSerial_BigBuffer(USBHost &host, uint16_t min_rxtx=65) :
		// hard code the normal one to 1 and 64 bytes for most likely most are 64
		USBSerialBase(host, bigbuffer, sizeof(bigbuffer), min_rxtx, 512) {};
private:
	enum { BUFFER_SIZE = 4096 }; // must hold at least 6 max size packets, plus 2 extra bytes
	uint32_t bigbuffer[(BUFFER_SIZE+3)/4];
};

//--------------------------------------------------------------------------

class AntPlus: public USBDriver {
// Please post any AntPlus feedback or contributions on this forum thread:
// https://forum.pjrc.com/threads/43110-Ant-libarary-and-USB-driver-for-Teensy-3-5-6
public:
	AntPlus(USBHost &host) : /* txtimer(this),*/  updatetimer(this), usb_host_port(host.usb_host_port) { init(); }
	void begin(const uint8_t key=0);
	void onStatusChange(void (*function)(int channel, int status)) {
		user_onStatusChange = function;
	}
	void onDeviceID(void (*function)(int channel, int devId, int devType, int transType)) {
		user_onDeviceID = function;
	}
	void onHeartRateMonitor(void (*f)(int bpm, int msec, int seqNum), uint32_t devid=0) {
		profileSetup_HRM(&ant.dcfg[PROFILE_HRM], devid);
		memset(&hrm, 0, sizeof(hrm));
		user_onHeartRateMonitor = f;
	}
	void onSpeedCadence(void (*f)(float speed, float distance, float rpm), uint32_t devid=0) {
		profileSetup_SPDCAD(&ant.dcfg[PROFILE_SPDCAD], devid);
		memset(&spdcad, 0, sizeof(spdcad));
		user_onSpeedCadence = f;
	}
	void onSpeed(void (*f)(float speed, float distance), uint32_t devid=0) {
		profileSetup_SPEED(&ant.dcfg[PROFILE_SPEED], devid);
		memset(&spd, 0, sizeof(spd));
		user_onSpeed = f;
	}
	void onCadence(void (*f)(float rpm), uint32_t devid=0) {
		profileSetup_CADENCE(&ant.dcfg[PROFILE_CADENCE], devid);
		memset(&cad, 0, sizeof(cad));
		user_onCadence = f;
	}
	void setWheelCircumference(float meters) {
		wheelCircumference = meters * 1000.0f;
	}
protected:
	virtual void Task();
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void disconnect();
	virtual void timer_event(USBDriverTimer *whichTimer);
private:
	static void rx_callback(const Transfer_t *transfer);
	static void tx_callback(const Transfer_t *transfer);
	void rx_data(const Transfer_t *transfer);
	void tx_data(const Transfer_t *transfer);
	void init();
	size_t write(const void *data, const size_t size);
	int read(void *data, const size_t size);
	void transmit();
private:
	Pipe_t mypipes[2] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[3] __attribute__ ((aligned(32)));
	strbuf_t mystring_bufs[1];
	//USBDriverTimer txtimer;
	USBDriverTimer updatetimer;
	Pipe_t *rxpipe;
	Pipe_t *txpipe;
	bool first_update;
	uint8_t txbuffer[240];
	uint8_t rxpacket[64];
	volatile uint16_t txhead;
	volatile uint16_t txtail;
	volatile bool     txready;
	volatile uint8_t  rxlen;
	volatile bool     do_polling;
	USBHostPort *usb_host_port;
private:
	enum _eventi {
		EVENTI_MESSAGE = 0,
		EVENTI_CHANNEL,
		EVENTI_TOTAL
	};
	enum _profiles {
		PROFILE_HRM = 0,
		PROFILE_SPDCAD,
		PROFILE_POWER,
		PROFILE_STRIDE,
		PROFILE_SPEED,
		PROFILE_CADENCE,
		PROFILE_TOTAL
	};
	typedef struct {
		uint8_t channel;
		uint8_t RFFreq;
		uint8_t networkNumber;
		uint8_t stub;
		uint8_t searchTimeout;
		uint8_t channelType;
		uint8_t deviceType;
		uint8_t transType;
		uint16_t channelPeriod;
		uint16_t searchWaveform;
		uint32_t deviceNumber; // deviceId
		struct {
			uint8_t chanIdOnce;
			uint8_t keyAccepted;
			uint8_t profileValid;
			uint8_t channelStatus;
			uint8_t channelStatusOld;
		} flags;
	} TDCONFIG;
	struct {
		uint8_t initOnce;
		uint8_t key; // key index
		int iDevice; // index to the antplus we're interested in, if > one found
		TDCONFIG dcfg[PROFILE_TOTAL]; // channel config, we're using one channel per device
	} ant;
	void (*user_onStatusChange)(int channel, int status);
	void (*user_onDeviceID)(int channel, int devId, int devType, int transType);
	void (*user_onHeartRateMonitor)(int beatsPerMinute, int milliseconds, int sequenceNumber);
	void (*user_onSpeedCadence)(float speed, float distance, float cadence);
	void (*user_onSpeed)(float speed, float distance);
	void (*user_onCadence)(float cadence);
	void dispatchPayload(TDCONFIG *cfg, const uint8_t *payload, const int len);
	static const uint8_t *getAntKey(const uint8_t keyIdx);
	static uint8_t calcMsgChecksum (const uint8_t *buffer, const uint8_t len);
	static uint8_t * findStreamSync(uint8_t *stream, const size_t rlen, int *pos);
	static int msgCheckIntegrity(uint8_t *stream, const int len);
	static int msgGetLength(uint8_t *stream);
	int handleMessages(uint8_t *buffer, int tBytes);
	void sendMessageChannelStatus(TDCONFIG *cfg, const uint32_t channelStatus);
	void message_channel(const int chan, const int eventId,
		const uint8_t *payload, const size_t dataLength);
	void message_response(const int chan, const int msgId,
		const uint8_t *payload, const size_t dataLength);
	void message_event(const int channel, const int msgId,
		const uint8_t *payload, const size_t dataLength);
	int ResetSystem();
	int RequestMessage(const int channel, const int message);
	int SetNetworkKey(const int netNumber, const uint8_t *key);
	int SetChannelSearchTimeout(const int channel, const int searchTimeout);
	int SetChannelPeriod(const int channel, const int period);
	int SetChannelRFFreq(const int channel, const int freq);
	int SetSearchWaveform(const int channel, const int wave);
	int OpenChannel(const int channel);
	int CloseChannel(const int channel);
	int AssignChannel(const int channel, const int channelType, const int network);
	int SetChannelId(const int channel, const int deviceNum, const int deviceType,
		const int transmissionType);
	int SendBurstTransferPacket(const int channelSeq, const uint8_t *data);
	int SendBurstTransfer(const int channel, const uint8_t *data, const int nunPackets);
	int SendBroadcastData(const int channel, const uint8_t *data);
	int SendAcknowledgedData(const int channel, const uint8_t *data);
	int SendExtAcknowledgedData(const int channel, const int devNum, const int devType,
		const int TranType, const uint8_t *data);
	int SendExtBroadcastData(const int channel, const int devNum, const int devType,
		const int TranType, const uint8_t *data);
	int SendExtBurstTransferPacket(const int chanSeq, const int devNum,
		const int devType, const int TranType, const uint8_t *data);
	int SendExtBurstTransfer(const int channel, const int devNum, const int devType,
		const int tranType, const uint8_t *data, const int nunPackets);
	static void profileSetup_HRM(TDCONFIG *cfg, const uint32_t deviceId);
	static void profileSetup_SPDCAD(TDCONFIG *cfg, const uint32_t deviceId);
	static void profileSetup_POWER(TDCONFIG *cfg, const uint32_t deviceId);
	static void profileSetup_STRIDE(TDCONFIG *cfg, const uint32_t deviceId);
	static void profileSetup_SPEED(TDCONFIG *cfg, const uint32_t deviceId);
	static void profileSetup_CADENCE(TDCONFIG *cfg, const uint32_t deviceId);
	struct {
		struct {
			uint8_t bpm;
			uint8_t sequence;
			uint16_t time;
		} previous;
	} hrm;
	void payload_HRM(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength);
	struct {
		struct {
			uint16_t cadenceTime;
			uint16_t cadenceCt;
			uint16_t speedTime;
			uint16_t speedCt;
		} previous;
		float distance;
	} spdcad;
	void payload_SPDCAD(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength);
	/* struct {
		struct {
			uint8_t sequence;
			uint16_t pedalPowerContribution;
			uint8_t pedalPower;
			uint8_t instantCadence;
			uint16_t sumPower;
			uint16_t instantPower;
		} current;
		struct {
			uint16_t stub;
		} previous;
	} pwr; */
	void payload_POWER(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength);
	/* struct {
		struct {
			uint16_t speed;
			uint16_t cadence;
			uint8_t strides;
		} current;
		struct {
			uint8_t strides;
			uint16_t speed;
			uint16_t cadence;
		} previous;
	} stride; */
	void payload_STRIDE(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength);
	struct {
		struct {
			uint16_t speedTime;
			uint16_t speedCt;
		} previous;
		float distance;
	} spd;
	void payload_SPEED(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength);
	struct {
		struct {
			uint16_t cadenceTime;
			uint16_t cadenceCt;
		} previous;
	} cad;
	void payload_CADENCE(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength);
	uint16_t wheelCircumference; // default is WHEEL_CIRCUMFERENCE (2122cm)
};

//--------------------------------------------------------------------------

class RawHIDController : public USBHIDInput {
public:
	RawHIDController(USBHost &host, uint32_t usage = 0) : fixed_usage_(usage) { init(host); }
	uint32_t usage(void) {return usage_;}
	void attachReceive(bool (*f)(uint32_t usage, const uint8_t *data, uint32_t len)) {receiveCB = f;}
	bool sendPacket(const uint8_t *buffer);
protected:
	virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
	virtual bool hid_process_in_data(const Transfer_t *transfer);
	virtual bool hid_process_out_data(const Transfer_t *transfer);
	virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
	virtual void hid_input_data(uint32_t usage, int32_t value);
	virtual void hid_input_end();
	virtual void disconnect_collection(Device_t *dev);
private:
	void init(USBHost&);
	USBHIDParser *driver_;
	enum { MAX_PACKET_SIZE = 64 };
	bool (*receiveCB)(uint32_t usage, const uint8_t *data, uint32_t len) = nullptr;
	uint8_t collections_claimed = 0;
	//volatile bool hid_input_begin_ = false;
	uint32_t fixed_usage_;
	uint32_t usage_ = 0;

	// See if we can contribute transfers
	Transfer_t mytransfers[2] __attribute__ ((aligned(32)));

};

//--------------------------------------------------------------------------

class USBSerialEmu : public USBHIDInput, public Stream {
public:
	USBSerialEmu(USBHost &host) { init(host); }
	uint32_t usage(void) {return usage_;}

	// Stream stuff. 
	uint32_t writeTimeout() {return write_timeout_;}
	void writeTimeOut(uint32_t write_timeout) {write_timeout_ = write_timeout;} // Will not impact current ones.
	virtual int available(void);
	virtual int peek(void);
	virtual int read(void);
	virtual int availableForWrite();
	virtual size_t write(uint8_t c);
	virtual void flush(void);

	using Print::write;


protected:
	virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
	virtual bool hid_process_in_data(const Transfer_t *transfer);
	virtual bool hid_process_out_data(const Transfer_t *transfer);
	virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
	virtual void hid_input_data(uint32_t usage, int32_t value);
	virtual void hid_input_end();
	virtual void disconnect_collection(Device_t *dev);
	virtual void hid_timer_event(USBDriverTimer *whichTimer);

	bool sendPacket();

private:
	void init(USBHost &host);
	USBHIDParser *driver_;
	enum { MAX_PACKET_SIZE = 64 };
	bool (*receiveCB)(uint32_t usage, const uint8_t *data, uint32_t len) = nullptr;
	uint8_t collections_claimed = 0;
	uint32_t usage_ = 0;

	// We have max of 512 byte packets coming in.  How about enough room for 3+3
	enum { RX_BUFFER_SIZE=1024, TX_BUFFER_SIZE = 512 };
	enum { DEFAULT_WRITE_TIMEOUT = 3500};

	uint8_t rx_buffer_[RX_BUFFER_SIZE];
	uint8_t tx_buffer_[TX_BUFFER_SIZE];

	volatile uint8_t tx_out_data_pending_ = 0;

	volatile uint16_t rx_head_;// receive head
	volatile uint16_t rx_tail_;// receive tail
	volatile uint16_t tx_head_;
	uint16_t rx_pipe_size_;// size of receive circular buffer
	uint16_t tx_pipe_size_;// size of transmit circular buffer
	uint32_t write_timeout_ = DEFAULT_WRITE_TIMEOUT;


	// See if we can contribute transfers
	Transfer_t mytransfers[2] __attribute__ ((aligned(32)));

};

//--------------------------------------------------------------------------

class BluetoothController: public USBDriver {
public:
	static const uint8_t MAX_CONNECTIONS = 4;
	typedef struct {
    BTHIDInput * 	device_driver_ = nullptr;;
    uint16_t		connection_rxid_ = 0;
    uint16_t		control_dcid_ = 0x70;
    uint16_t		interrupt_dcid_ = 0x71;
    uint16_t		interrupt_scid_;
    uint16_t		control_scid_;

    uint8_t			device_bdaddr_[6];// remember devices address
    uint8_t			device_ps_repetion_mode_ ; // mode
    uint8_t			device_clock_offset_[2];
    uint32_t		device_class_;	// class of device. 
    uint16_t		device_connection_handle_;	// handle to connection 
	uint8_t    		remote_ver_;
	uint16_t		remote_man_;
	uint8_t			remote_subv_;
	uint8_t			connection_complete_ = false;	//
	} connection_info_t;

	BluetoothController(USBHost &host, bool pair = false, const char *pin = "0000") : do_pair_device_(pair), pair_pincode_(pin), delayTimer_(this) 
			 { init(); }

	enum {MAX_ENDPOINTS=4, NUM_SERVICES=4, };  // Max number of Bluetooth services - if you need more than 4 simply increase this number
	enum {BT_CLASS_DEVICE= 0x0804}; // Toy - Robot
	static void driver_ready_for_bluetooth(BTHIDInput *driver);

    const uint8_t* 	myBDAddr(void) {return my_bdaddr_;}

	// BUGBUG version to allow some of the controlled objects to call?
    enum {CONTROL_SCID=-1, INTERRUPT_SCID=-2};
    void sendL2CapCommand(uint8_t* data, uint8_t nbytes, int channel = (int)0x0001);

protected:
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void control(const Transfer_t *transfer);
	virtual void disconnect();
	virtual void timer_event(USBDriverTimer *whichTimer);

	BTHIDInput * find_driver(uint32_t device_type, uint8_t *remoteName=nullptr);

	// Hack to allow PS3 to maybe change values
    uint16_t		next_dcid_ = 0x70;		// Lets try not hard coding control and interrupt dcid
#if 0    
    uint16_t		connection_rxid_ = 0;
    uint16_t		control_dcid_ = 0x70;
    uint16_t		interrupt_dcid_ = 0x71;
    uint16_t		interrupt_scid_;
    uint16_t		control_scid_;
#else
    connection_info_t connections_[MAX_CONNECTIONS];
    uint8_t count_connections_ = 0;
    uint8_t current_connection_ = 0;	// need to figure out when this changes and/or... 
#endif    

private:
	friend class BTHIDInput;
	static void rx_callback(const Transfer_t *transfer);
	static void rx2_callback(const Transfer_t *transfer);
	static void tx_callback(const Transfer_t *transfer);
	void rx_data(const Transfer_t *transfer);
	void rx2_data(const Transfer_t *transfer);
	void tx_data(const Transfer_t *transfer);

	void init();

	// HCI support functions...
	void sendHCICommand(uint16_t hciCommand, uint16_t cParams, const uint8_t* data);
	//void sendHCIReadLocalSupportedFeatures();
	void inline sendHCI_INQUIRY();
	void inline sendHCIInquiryCancel();
	void inline sendHCICreateConnection();
	void inline sendHCIAuthenticationRequested();
	void inline sendHCIAcceptConnectionRequest();
	void inline sendHCILinkKeyNegativeReply();
	void inline sendHCIPinCodeReply();
    void inline sendResetHCI();
    void inline sendHDCWriteClassOfDev();
	void inline sendHCIReadBDAddr();
	void inline sendHCIReadLocalVersionInfo();
	void inline sendHCIWriteScanEnable(uint8_t scan_op);
	void inline sendHCIHCIWriteInquiryMode(uint8_t inquiry_mode);
	void inline sendHCISetEventMask();

	void inline sendHCIRemoteNameRequest();
	void inline sendHCIRemoteVersionInfoRequest();
	void handle_hci_command_complete();
	void handle_hci_command_status();
	void handle_hci_inquiry_result(bool fRSSI=false);
	void handle_hci_extended_inquiry_result();
	void handle_hci_inquiry_complete();
	void handle_hci_incoming_connect();
	void handle_hci_connection_complete();
	void handle_hci_disconnect_complete();
	void handle_hci_authentication_complete();
	void handle_hci_remote_name_complete();
	void handle_hci_remote_version_information_complete();
	void handle_hci_pin_code_request();
	void handle_hci_link_key_notification();
	void handle_hci_link_key_request();
	void queue_next_hci_command();

	void sendl2cap_ConnectionResponse(uint16_t handle, uint8_t rxid, uint16_t dcid, uint16_t scid, uint8_t result);
	void sendl2cap_ConnectionRequest(uint16_t handle, uint8_t rxid, uint16_t scid, uint16_t psm);
	void sendl2cap_ConfigRequest(uint16_t handle, uint8_t rxid, uint16_t dcid);
	void sendl2cap_ConfigResponse(uint16_t handle, uint8_t rxid, uint16_t scid);
    void sendL2CapCommand(uint16_t handle, uint8_t* data, uint8_t nbytes, uint8_t channelLow = 0x01, uint8_t channelHigh = 0x00);

	void process_l2cap_connection_request(uint8_t *data);
	void process_l2cap_connection_response(uint8_t *data);
	void process_l2cap_config_request(uint8_t *data);
	void process_l2cap_config_response(uint8_t *data);
	void process_l2cap_command_reject(uint8_t *data);
	void process_l2cap_disconnect_request(uint8_t *data);

	void setHIDProtocol(uint8_t protocol);
	void handleHIDTHDRData(uint8_t *buffer);	// Pass the whole buffer...
	static BTHIDInput *available_bthid_drivers_list;


	setup_t setup;
	Pipe_t mypipes[4] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[7] __attribute__ ((aligned(32)));
	strbuf_t mystring_bufs[2];		// 2 string buffers - one for our device - one for remote device...
	uint16_t 		pending_control_ = 0;
	uint16_t		pending_control_tx_ = 0;
	uint16_t 		rx_size_ = 0;
	uint16_t 		rx2_size_ = 0;
	uint16_t 		tx_size_ = 0;
	Pipe_t 			*rxpipe_;
	Pipe_t 			*rx2pipe_;
	Pipe_t 			*txpipe_;
	uint8_t 		rxbuf_[256];	// used to receive data from RX, which may come with several packets...
	uint8_t 		rx_packet_data_remaining=0; // how much data remaining
	uint8_t 		rx2buf_[64];	// receive buffer from Bulk end point
	uint8_t			txbuf_[256];	// buffer to use to send commands to bluetooth 
	uint8_t			hciVersion;		// what version of HCI do we have?

	bool 			do_pair_device_;	// Should we do a pair for a new device?
	const char		*pair_pincode_;	// What pin code to use for the pairing
	USBDriverTimer 	delayTimer_;
    uint8_t 		my_bdaddr_[6];	// The bluetooth dongles Bluetooth address.
    uint8_t			features[8];	// remember our local features.

	typedef struct {
		uint16_t 	idVendor;
		uint16_t 	idProduct;
	} product_vendor_mapping_t;
	static product_vendor_mapping_t pid_vid_mapping[];

};

class ADK: public USBDriver {
public:
	ADK(USBHost &host) { init(); }
	bool ready();
	void begin(char *adk_manufacturer, char *adk_model, char *adk_desc, char *adk_version, char *adk_uri, char *adk_serial);
	void end();
	int available(void);
	int peek(void);
	int read(void);
	size_t write(size_t len, uint8_t *buf);
protected:
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void disconnect();
	virtual void control(const Transfer_t *transfer);
	static void rx_callback(const Transfer_t *transfer);
	static void tx_callback(const Transfer_t *transfer);
	void rx_data(const Transfer_t *transfer);
	void tx_data(const Transfer_t *transfer);
	void init();
	void rx_queue_packets(uint32_t head, uint32_t tail);
	void sendStr(Device_t *dev, uint8_t index, char *str);
private:
	int state = 0;
	Pipe_t *rxpipe;
	Pipe_t *txpipe;
	enum { MAX_PACKET_SIZE = 512 };
	enum { RX_QUEUE_SIZE = 1024 }; // must be more than MAX_PACKET_SIZE
	uint8_t rx_buffer[MAX_PACKET_SIZE];
	uint8_t tx_buffer[MAX_PACKET_SIZE];
	uint16_t rx_size;
	uint16_t tx_size;
	uint8_t rx_queue[RX_QUEUE_SIZE];
	bool rx_packet_queued;
	uint16_t rx_head;
	uint16_t rx_tail;
	uint8_t rx_ep;
	uint8_t tx_ep;
	char *manufacturer;
	char *model;
	char *desc;
	char *version;
	char *uri;
	char *serial;
	Pipe_t mypipes[3] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[7] __attribute__ ((aligned(32)));
};

//--------------------------------------------------------------------------
class msController : public USBDriver {
public:
	msController(USBHost &host) : usb_host_port(host.usb_host_port) { init(); }
	msController(USBHost *host) : usb_host_port(host->usb_host_port) { init(); }

	msSCSICapacity_t msCapacity;
	msInquiryResponse_t msInquiry;
	msRequestSenseResponse_t msSense;
	msDriveInfo_t msDriveInfo;

	bool mscTransferComplete = false;
	uint8_t mscInit(void);
	void msReset(void);
	uint8_t msGetMaxLun(void);
	void msCurrentLun(uint8_t lun) {currentLUN = lun;}
	uint8_t msCurrentLun() {return currentLUN;}
	bool available() { delay(0); return deviceAvailable; }
	uint8_t checkConnectedInitialized(void);
	uint16_t getIDVendor() {return idVendor; }
	uint16_t getIDProduct() {return idProduct; }
	uint8_t getHubNumber() { return hubNumber; }
	uint8_t getHubPort() { return hubPort; }
	uint8_t getDeviceAddress() { return deviceAddress; }
	uint8_t WaitMediaReady();
	uint8_t msTestReady();
	uint8_t msReportLUNs(uint8_t *Buffer);
	uint8_t msStartStopUnit(uint8_t mode);
	uint8_t msReadDeviceCapacity(msSCSICapacity_t * const Capacity);
	uint8_t msDeviceInquiry(msInquiryResponse_t * const Inquiry);
	uint8_t msProcessError(uint8_t msStatus);
	uint8_t msRequestSense(msRequestSenseResponse_t * const Sense);
	uint8_t msRequestSense(void *Sense);

	uint8_t msReadBlocks(const uint32_t BlockAddress, const uint16_t Blocks,
						 const uint16_t BlockSize, void * sectorBuffer);
	uint8_t msWriteBlocks(const uint32_t BlockAddress, const uint16_t Blocks,
                        const uint16_t BlockSize,	const void * sectorBuffer);
protected:
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void control(const Transfer_t *transfer);
	virtual void disconnect();
	static void callbackIn(const Transfer_t *transfer);
	static void callbackOut(const Transfer_t *transfer);
	void new_dataIn(const Transfer_t *transfer);
	void new_dataOut(const Transfer_t *transfer);
	void init();
	uint8_t msDoCommand(msCommandBlockWrapper_t *CBW, void *buffer);
	uint8_t msGetCSW(void);
private:
	Pipe_t mypipes[3] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[7] __attribute__ ((aligned(32)));
	strbuf_t mystring_bufs[1];
	uint32_t packetSizeIn;
	uint32_t packetSizeOut;
	Pipe_t *datapipeIn;
	Pipe_t *datapipeOut;
	uint8_t bInterfaceNumber;
	uint32_t endpointIn = 0;
	uint32_t endpointOut = 0;
	setup_t setup;
	uint8_t report[8];
	uint8_t maxLUN = 0;
	uint8_t currentLUN = 0;
//	msSCSICapacity_t msCapacity;
//	msInquiryResponse_t msInquiry;
//	msRequestSenseResponse_t msSense;
	uint16_t idVendor = 0;
	uint16_t idProduct = 0;
	uint8_t hubNumber = 0;
	uint8_t hubPort = 0;
	uint8_t deviceAddress = 0;
	volatile bool msOutCompleted = false;
	volatile bool msInCompleted = false;
	volatile bool msControlCompleted = false;
	uint32_t CBWTag = 0;
	volatile bool deviceAvailable = false;
	USBHostPort *usb_host_port;
};

#endif
