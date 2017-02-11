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

typedef struct Device_struct    Device_t;
typedef struct Pipe_struct      Pipe_t;
typedef struct Transfer_struct  Transfer_t;


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
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t LanguageID;
};

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


struct Transfer_struct {
	// Queue Element Transfer Descriptor (qTD), EHCI pg 40-45
	struct {  // must be aligned to 32 byte boundary
		volatile uint32_t next;
		volatile uint32_t alt_next;
		volatile uint32_t token;
		volatile uint32_t buffer[5];
	} qtd;
	// linked list of queued, not-yet-completed transfers
	Transfer_t *next_followup;
	Transfer_t *prev_followup;
	// data to be used by callback function
	Pipe_t   *pipe;
	void     *buffer;
	uint32_t length;
	uint32_t unused[3];
};

void begin();
Pipe_t * new_Pipe(Device_t *dev, uint32_t type, uint32_t endpoint, uint32_t direction,
        uint32_t max_packet_len);
bool new_Transfer(Pipe_t *pipe, void *buffer, uint32_t len);
bool followup_Transfer(Transfer_t *transfer);
void add_to_async_followup_list(Transfer_t *first, Transfer_t *last);
void remove_from_async_followup_list(Transfer_t *transfer);
void add_to_periodic_followup_list(Transfer_t *first, Transfer_t *last);
void remove_from_periodic_followup_list(Transfer_t *transfer);


Device_t * new_Device(uint32_t speed, uint32_t hub_addr, uint32_t hub_port);
void enumeration(const Transfer_t *transfer);
void mk_setup(setup_t &s, uint32_t bmRequestType, uint32_t bRequest,
                uint32_t wValue, uint32_t wIndex, uint32_t wLength);
uint32_t assign_addr(void);
void pipe_set_maxlen(Pipe_t *pipe, uint32_t maxlen);
void pipe_set_addr(Pipe_t *pipe, uint32_t addr);
uint32_t pipe_get_addr(Pipe_t *pipe);



void init_Device_Pipe_Transfer_memory(void);
Device_t * allocate_Device(void);
void free_Device(Device_t *q);
Pipe_t * allocate_Pipe(void);
void free_Pipe(Pipe_t *q);
Transfer_t * allocate_Transfer(void);
void free_Transfer(Transfer_t *q);

void print(const Transfer_t *transfer);
void print(const Transfer_t *first, const Transfer_t *last);
void print_token(uint32_t token);
void print(const Pipe_t *pipe);
void print_hexbytes(const void *ptr, uint32_t len);
void print(const char *s);
void print(const char *s, int num);


class USBHost {
public:
	static void begin();
protected:
	static void enumeration(const Transfer_t *transfer);
	static void isr();





};


class USBHostDriver : public USBHost {
public:
	virtual bool claim_device(Device_t *device) {
		return false;
	}
	virtual bool claim_interface(Device_t *device) {
		return false;
	}
	virtual void disconnect() {
	}

	USBHostDriver *next;
};

class USBHub : public USBHostDriver {



};



#endif
