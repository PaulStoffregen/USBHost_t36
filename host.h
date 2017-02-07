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
	// data to be used by callback function
	Pipe_t   *pipe;
	void     *buffer;
	uint32_t length;
	uint32_t unused[4];
};

void init_Device_Pipe_Transfer_memory(void);
Device_t * allocate_Device(void);
void free_Device(Device_t *q);
Pipe_t * allocate_Pipe(void);
void free_Pipe(Pipe_t *q);
Transfer_t * allocate_Transfer(void);
void free_Transfer(Transfer_t *q);

#endif
