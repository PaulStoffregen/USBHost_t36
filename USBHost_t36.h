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

#if !defined(__MK66FX1M0__)
#error "USBHost_t36 only works with Teensy 3.6.  Please select it in Tools > Boards"
#endif

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


// #define USBHOST_PRINT_DEBUG

/************************************************/
/*  Data Types                                  */
/************************************************/

// These 6 types are the key to understanding how this USB Host
// library really works.

// USBHost is a static class controlling the hardware.
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

// All USB device drivers inherit use these classes.
// Drivers build user-visible functionality on top
// of these classes, which receive USB events from
// USBHost.
class USBDriver;
class USBDriverTimer;


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

// Device_t holds all the information about a USB device
struct Device_struct {
	Pipe_t   *control_pipe;
	Pipe_t   *data_pipes;
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
	uint8_t  start_mask;
	uint8_t  complete_mask;
	Pipe_t   *next;
	void     (*callback_function)(const Transfer_t *);
	uint16_t periodic_interval;
	uint16_t periodic_offset;
	uint32_t unused1;
	uint32_t unused2;
	uint32_t unused3;
	uint32_t unused4;
	uint32_t unused5;
	uint32_t unused6;
	uint32_t unused7;
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
/*  Main USB EHCI Controller                    */
/************************************************/

class USBHost {
public:
	static void begin();
	static void Task();
protected:
	static Pipe_t * new_Pipe(Device_t *dev, uint32_t type, uint32_t endpoint,
		uint32_t direction, uint32_t maxlen, uint32_t interval=0);
	static bool queue_Control_Transfer(Device_t *dev, setup_t *setup,
		void *buf, USBDriver *driver);
	static bool queue_Data_Transfer(Pipe_t *pipe, void *buffer,
		uint32_t len, USBDriver *driver);
	static Device_t * new_Device(uint32_t speed, uint32_t hub_addr, uint32_t hub_port);
	static void disconnect_Device(Device_t *dev);
	static void enumeration(const Transfer_t *transfer);
	static void driver_ready_for_device(USBDriver *driver);
	static void contribute_Devices(Device_t *devices, uint32_t num);
	static void contribute_Pipes(Pipe_t *pipes, uint32_t num);
	static void contribute_Transfers(Transfer_t *transfers, uint32_t num);
	static volatile bool enumeration_busy;
private:
	static void isr();
	static void claim_drivers(Device_t *dev);
	static uint32_t assign_address(void);
	static bool queue_Transfer(Pipe_t *pipe, Transfer_t *transfer);
	static void init_Device_Pipe_Transfer_memory(void);
	static Device_t * allocate_Device(void);
	static void delete_Pipe(Pipe_t *pipe);
	static void free_Device(Device_t *q);
	static Pipe_t * allocate_Pipe(void);
	static void free_Pipe(Pipe_t *q);
	static Transfer_t * allocate_Transfer(void);
	static void free_Transfer(Transfer_t *q);
	static bool allocate_interrupt_pipe_bandwidth(Pipe_t *pipe,
		uint32_t maxlen, uint32_t interval);
	static void add_qh_to_periodic_schedule(Pipe_t *pipe);
	static bool followup_Transfer(Transfer_t *transfer);
	static void followup_Error(void);
protected:
#ifdef USBHOST_PRINT_DEBUG
	static void print(const Transfer_t *transfer);
	static void print(const Transfer_t *first, const Transfer_t *last);
	static void print_token(uint32_t token);
	static void print(const Pipe_t *pipe);
	static void print_driverlist(const char *name, const USBDriver *driver);
	static void print_qh_list(const Pipe_t *list);
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
	static void println(uint32_t n, uint8_t b) { Serial.println(n, b); }
	static void print(const char *s, int n, uint8_t b = DEC) {
		Serial.print(s); Serial.print(n, b); }
	static void print(const char *s, unsigned int n, uint8_t b = DEC) {
		Serial.print(s); Serial.print(n, b); }
	static void print(const char *s, long n, uint8_t b = DEC) {
		Serial.print(s); Serial.print(n, b); }
	static void print(const char *s, unsigned long n, uint8_t b = DEC) {
		Serial.print(s); Serial.print(n, b); }
	static void println(const char *s, int n, uint8_t b = DEC) {
		Serial.print(s); Serial.println(n, b); }
	static void println(const char *s, unsigned int n, uint8_t b = DEC) {
		Serial.print(s); Serial.println(n, b); }
	static void println(const char *s, long n, uint8_t b = DEC) {
		Serial.print(s); Serial.println(n, b); }
	static void println(const char *s, unsigned long n, uint8_t b = DEC) {
		Serial.print(s); Serial.println(n, b); }
#else
	static void print(const Transfer_t *transfer) {}
	static void print(const Transfer_t *first, const Transfer_t *last) {}
	static void print_token(uint32_t token) {}
	static void print(const Pipe_t *pipe) {}
	static void print_driverlist(const char *name, const USBDriver *driver) {}
	static void print_qh_list(const Pipe_t *list) {}
	static void print_hexbytes(const void *ptr, uint32_t len) {}
	static void print(const char *s) {}
	static void print(int n) {}
	static void print(unsigned int n) {}
	static void print(long n) {}
	static void print(unsigned long n) {}
	static void println(const char *s) {}
	static void println(int n) {}
	static void println(unsigned int n) {}
	static void println(long n) {}
	static void println(unsigned long n) {}
	static void println() {}
	static void print(uint32_t n, uint8_t b) {}
	static void println(uint32_t n, uint8_t b) {}
	static void print(const char *s, int n, uint8_t b = DEC) {}
	static void print(const char *s, unsigned int n, uint8_t b = DEC) {}
	static void print(const char *s, long n, uint8_t b = DEC) {}
	static void print(const char *s, unsigned long n, uint8_t b = DEC) {}
	static void println(const char *s, int n, uint8_t b = DEC) {}
	static void println(const char *s, unsigned int n, uint8_t b = DEC) {}
	static void println(const char *s, long n, uint8_t b = DEC) {}
	static void println(const char *s, unsigned long n, uint8_t b = DEC) {}
#endif
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
public:
	operator bool() { return (device != nullptr); }
	uint16_t idVendor() { return (device != nullptr) ? device->idVendor : 0; }
	uint16_t idProduct() { return (device != nullptr) ? device->idProduct : 0; }
	// TODO: user-level functions
	// check if device is bound/active/online
	// query vid, pid
	// query string: manufacturer, product, serial number
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
};

// Device drivers may create these timer objects to schedule a timer call
class USBDriverTimer {
public:
	USBDriverTimer() { }
	USBDriverTimer(USBDriver *d) : driver(d) { }
	void init(USBDriver *d) { driver = d; };
	void start(uint32_t microseconds);
	void *pointer;
	uint32_t integer;
	uint32_t started_micros; // testing only
private:
	USBDriver      *driver;
	uint32_t       usec;
	USBDriverTimer *next;
	USBDriverTimer *prev;
	friend class USBHost;
};

// Device drivers may inherit from this base class, if they wish to receive
// HID input data fully decoded by the USBHIDParser driver
class USBHIDInput {
public:
	operator bool() { return (mydevice != nullptr); }
	uint16_t idVendor() { return (mydevice != nullptr) ? mydevice->idVendor : 0; }
	uint16_t idProduct() { return (mydevice != nullptr) ? mydevice->idProduct : 0; }
private:
	virtual bool claim_collection(Device_t *dev, uint32_t topusage);
	virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
	virtual void hid_input_data(uint32_t usage, int32_t value);
	virtual void hid_input_end();
	virtual void disconnect_collection(Device_t *dev);
	void add_to_list();
	USBHIDInput *next;
	friend class USBHIDParser;
protected:
	Device_t *mydevice = NULL;
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
	static void callback(const Transfer_t *transfer);
	void status_change(const Transfer_t *transfer);
	void new_port_status(uint32_t port, uint32_t status);
	void start_debounce_timer(uint32_t port);
	void stop_debounce_timer(uint32_t port);
private:
	Device_t mydevices[MAXPORTS];
	Pipe_t mypipes[2] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[4] __attribute__ ((aligned(32)));
	USBDriverTimer debouncetimer;
	USBDriverTimer resettimer;
	setup_t setup;
	Pipe_t *changepipe;
	Device_t *devicelist[MAXPORTS];
	uint32_t changebits;
	uint32_t statusbits;
	uint8_t  hub_desc[16];
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


class USBHIDParser : public USBDriver {
public:
	USBHIDParser(USBHost &host) { init(); }
	static void driver_ready_for_hid_collection(USBHIDInput *driver);
protected:
	enum { TOPUSAGE_LIST_LEN = 4 };
	enum { USAGE_LIST_LEN = 12 };
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void control(const Transfer_t *transfer);
	virtual void disconnect();
	static void in_callback(const Transfer_t *transfer);
	static void out_callback(const Transfer_t *transfer);
	void in_data(const Transfer_t *transfer);
	void out_data(const Transfer_t *transfer);
	bool check_if_using_report_id();
	void parse();
	USBHIDInput * find_driver(uint32_t topusage);
	void parse(uint16_t type_and_report_id, const uint8_t *data, uint32_t len);
	void init();
private:
	Pipe_t *in_pipe;
	Pipe_t *out_pipe;
	static USBHIDInput *available_hid_drivers_list;
	//uint32_t topusage_list[TOPUSAGE_LIST_LEN];
	USBHIDInput *topusage_drivers[TOPUSAGE_LIST_LEN];
	uint16_t in_size;
	uint16_t out_size;
	setup_t setup;
	uint8_t descriptor[512];
	uint8_t report[64];
	uint16_t descsize;
	bool use_report_id;
	Pipe_t mypipes[3] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[4] __attribute__ ((aligned(32)));
};



class KeyboardController : public USBDriver /* , public USBHIDInput */ {
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
	int      available();
	int      read();
	uint16_t getKey() { return keyCode; }
	uint8_t  getModifiers() { return modifiers; }
	uint8_t  getOemKey() { return keyOEM; }
	void     attachPress(void (*f)(int unicode)) { keyPressedFunction = f; }
	void     attachRelease(void (*f)(int unicode)) { keyReleasedFunction = f; }
	void     LEDS(uint8_t leds);
	uint8_t  LEDS() {return leds_.byte;}
	void     updateLEDS(void);
	bool     numLock() {return leds_.numLock;}
	bool     capsLock() {return leds_.capsLock;}
	bool     scrollLock() {return leds_.scrollLock;}
	void	 numLock(bool f);
	void     capsLock(bool f);
	void	 scrollLock(bool f);
protected:
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void control(const Transfer_t *transfer);
	virtual void disconnect();
	static void callback(const Transfer_t *transfer);
	void new_data(const Transfer_t *transfer);
	void init();
private:
	void update();
	uint16_t convert_to_unicode(uint32_t mod, uint32_t key);
	void key_press(uint32_t mod, uint32_t key);
	void key_release(uint32_t mod, uint32_t key);
	void (*keyPressedFunction)(int unicode);
	void (*keyReleasedFunction)(int unicode);
	Pipe_t *datapipe;
	setup_t setup;
	uint8_t report[8];
	uint16_t keyCode;
	uint8_t modifiers;
	uint8_t keyOEM;
	uint8_t prev_report[8];
	KBDLeds_t leds_ = {0};
	bool update_leds_ = false;
	bool processing_new_data_ = false;
	Pipe_t mypipes[2] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[4] __attribute__ ((aligned(32)));
};

class MIDIDevice : public USBDriver {
public:
	enum { SYSEX_MAX_LEN = 60 };
	MIDIDevice(USBHost &host) { init(); }
	MIDIDevice(USBHost *host) { init(); }
	bool read(uint8_t channel=0, uint8_t cable=0);
	uint8_t getType(void) {
		return msg_type;
	};
	uint8_t getChannel(void) {
		return msg_channel;
	};
	uint8_t getData1(void) {
		return msg_data1;
	};
	uint8_t getData2(void) {
		return msg_data2;
	};
	void setHandleNoteOff(void (*f)(uint8_t channel, uint8_t note, uint8_t velocity)) {
		handleNoteOff = f;
	};
	void setHandleNoteOn(void (*f)(uint8_t channel, uint8_t note, uint8_t velocity)) {
		handleNoteOn = f;
	};
	void setHandleVelocityChange(void (*f)(uint8_t channel, uint8_t note, uint8_t velocity)) {
		handleVelocityChange = f;
	};
	void setHandleControlChange(void (*f)(uint8_t channel, uint8_t control, uint8_t value)) {
		handleControlChange = f;
	};
	void setHandleProgramChange(void (*f)(uint8_t channel, uint8_t program)) {
		handleProgramChange = f;
	};
	void setHandleAfterTouch(void (*f)(uint8_t channel, uint8_t pressure)) {
		handleAfterTouch = f;
	};
	void setHandlePitchChange(void (*f)(uint8_t channel, int pitch)) {
		handlePitchChange = f;
	};
	void setHandleSysEx(void (*f)(const uint8_t *data, uint16_t length, bool complete)) {
		handleSysEx = (void (*)(const uint8_t *, uint16_t, uint8_t))f;
	}
	void setHandleRealTimeSystem(void (*f)(uint8_t realtimebyte)) {
		handleRealTimeSystem = f;
	};
	void setHandleTimeCodeQuarterFrame(void (*f)(uint16_t data)) {
		handleTimeCodeQuarterFrame = f;
	};
	void sendNoteOff(uint32_t note, uint32_t velocity, uint32_t channel) {
		write_packed(0x8008 | (((channel - 1) & 0x0F) << 8)
		 | ((note & 0x7F) << 16) | ((velocity & 0x7F) << 24));
	}
	void sendNoteOn(uint32_t note, uint32_t velocity, uint32_t channel) {
		write_packed(0x9009 | (((channel - 1) & 0x0F) << 8)
		 | ((note & 0x7F) << 16) | ((velocity & 0x7F) << 24));
	}
	void sendPolyPressure(uint32_t note, uint32_t pressure, uint32_t channel) {
		write_packed(0xA00A | (((channel - 1) & 0x0F) << 8)
		 | ((note & 0x7F) << 16) | ((pressure & 0x7F) << 24));
	}
	void sendControlChange(uint32_t control, uint32_t value, uint32_t channel) {
		write_packed(0xB00B | (((channel - 1) & 0x0F) << 8)
		 | ((control & 0x7F) << 16) | ((value & 0x7F) << 24));
	}
	void sendProgramChange(uint32_t program, uint32_t channel) {
		write_packed(0xC00C | (((channel - 1) & 0x0F) << 8)
		 | ((program & 0x7F) << 16));
	}
	void sendAfterTouch(uint32_t pressure, uint32_t channel) {
		write_packed(0xD00D | (((channel - 1) & 0x0F) << 8)
		 | ((pressure & 0x7F) << 16));
	}
	void sendPitchBend(uint32_t value, uint32_t channel) {
		write_packed(0xE00E | (((channel - 1) & 0x0F) << 8)
		 | ((value & 0x7F) << 16) | ((value & 0x3F80) << 17));
	}
	void sendSysEx(uint32_t length, const void *data);
	void sendRealTime(uint32_t type) {
		switch (type) {
			case 0xF8: // Clock
			case 0xFA: // Start
			case 0xFC: // Stop
			case 0xFB: // Continue
			case 0xFE: // ActiveSensing
			case 0xFF: // SystemReset
				write_packed((type << 8) | 0x0F);
			break;
				default: // Invalid Real Time marker
			break;
		}
	}
	void sendTimeCodeQuarterFrame(uint32_t type, uint32_t value) {
		uint32_t data = ( ((type & 0x07) << 4) | (value & 0x0F) );
		sendTimeCodeQuarterFrame(data);
	}
	void sendTimeCodeQuarterFrame(uint32_t data) {
		write_packed(0xF108 | ((data & 0x7F) << 16));
	}
protected:
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void disconnect();
	static void rx_callback(const Transfer_t *transfer);
	static void tx_callback(const Transfer_t *transfer);
	void rx_data(const Transfer_t *transfer);
	void tx_data(const Transfer_t *transfer);
	void init();
	void write_packed(uint32_t data);
	void sysex_byte(uint8_t b);
private:
	Pipe_t *rxpipe;
	Pipe_t *txpipe;
	enum { MAX_PACKET_SIZE = 64 };
	enum { RX_QUEUE_SIZE = 80 }; // must be more than MAX_PACKET_SIZE/4
	uint32_t rx_buffer[MAX_PACKET_SIZE/4];
	uint32_t tx_buffer[MAX_PACKET_SIZE/4];
	uint16_t rx_size;
	uint16_t tx_size;
	uint32_t rx_queue[RX_QUEUE_SIZE];
	bool rx_packet_queued;
	uint16_t rx_head;
	uint16_t rx_tail;
	uint8_t rx_ep;
	uint8_t tx_ep;
	uint8_t msg_channel;
	uint8_t msg_type;
	uint8_t msg_data1;
	uint8_t msg_data2;
	uint8_t msg_sysex[SYSEX_MAX_LEN];
	uint8_t msg_sysex_len;
	void (*handleNoteOff)(uint8_t ch, uint8_t note, uint8_t vel);
	void (*handleNoteOn)(uint8_t ch, uint8_t note, uint8_t vel);
	void (*handleVelocityChange)(uint8_t ch, uint8_t note, uint8_t vel);
	void (*handleControlChange)(uint8_t ch, uint8_t control, uint8_t value);
	void (*handleProgramChange)(uint8_t ch, uint8_t program);
	void (*handleAfterTouch)(uint8_t ch, uint8_t pressure);
	void (*handlePitchChange)(uint8_t ch, int pitch);
	void (*handleSysEx)(const uint8_t *data, uint16_t length, uint8_t complete);
	void (*handleRealTimeSystem)(uint8_t rtb);
	void (*handleTimeCodeQuarterFrame)(uint16_t data);
	Pipe_t mypipes[3] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[7] __attribute__ ((aligned(32)));
};


class MouseController : public USBHIDInput {
public:
	MouseController(USBHost &host) { USBHIDParser::driver_ready_for_hid_collection(this); }
	bool	available() { return mouseEvent; }
	void	mouseDataClear();
	uint8_t getButtons() { return buttons; }
	int     getMouseX() { return mouseX; }
	int     getMouseY() { return mouseY; }
	int     getWheel() { return wheel; }
	int     getWheelH() { return wheelH; }
protected:
	virtual bool claim_collection(Device_t *dev, uint32_t topusage);
	virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
	virtual void hid_input_data(uint32_t usage, int32_t value);
	virtual void hid_input_end();
	virtual void disconnect_collection(Device_t *dev);
private:
	uint8_t collections_claimed = 0;
	volatile bool mouseEvent = false;
	uint8_t buttons = 0;
	int     mouseX = 0;
	int     mouseY = 0;
	int     wheel = 0;
	int     wheelH = 0;
};


class JoystickController : public USBHIDInput {
public:
	JoystickController(USBHost &host) { USBHIDParser::driver_ready_for_hid_collection(this); }
	bool    available() { return joystickEvent; }
	void    joystickDataClear();
	uint32_t getButtons() { return buttons; }
	int	getAxis(uint32_t index) { return (index < (sizeof(axis)/sizeof(axis[0]))) ? axis[index] : 0; }
protected:
	virtual bool claim_collection(Device_t *dev, uint32_t topusage);
	virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
	virtual void hid_input_data(uint32_t usage, int32_t value);
	virtual void hid_input_end();
	virtual void disconnect_collection(Device_t *dev);
private:
	uint8_t collections_claimed = 0;
	bool anychange = false;
	volatile bool joystickEvent = false;
	uint32_t buttons = 0;
	int16_t axis[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
};

#endif
