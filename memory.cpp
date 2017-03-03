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


// Memory allocation

static Device_t memory_Device[4];
static Pipe_t memory_Pipe[8] __attribute__ ((aligned(32)));
static Transfer_t memory_Transfer[34] __attribute__ ((aligned(32)));

static Device_t * free_Device_list = NULL;
static Pipe_t * free_Pipe_list = NULL;
static Transfer_t * free_Transfer_list = NULL;

void USBHost::init_Device_Pipe_Transfer_memory(void)
{
	Device_t *end_device = memory_Device + sizeof(memory_Device)/sizeof(Device_t);
	for (Device_t *device = memory_Device; device < end_device; device++) {
		free_Device(device);
	}
	Pipe_t *end_pipe = memory_Pipe + sizeof(memory_Pipe)/sizeof(Pipe_t);
	for (Pipe_t *pipe = memory_Pipe; pipe < end_pipe; pipe++) {
		free_Pipe(pipe);
	}
	Transfer_t *end_transfer = memory_Transfer + sizeof(memory_Transfer)/sizeof(Transfer_t);
	for (Transfer_t *transfer = memory_Transfer; transfer < end_transfer; transfer++) {
		free_Transfer(transfer);
	}
}

Device_t * USBHost::allocate_Device(void)
{
	Device_t *device = free_Device_list;
	if (device) free_Device_list = *(Device_t **)device;
	return device;
}

void USBHost::free_Device(Device_t *device)
{
	*(Device_t **)device = free_Device_list;
	free_Device_list = device;
}

Pipe_t * USBHost::allocate_Pipe(void)
{
	Pipe_t *pipe = free_Pipe_list;
	if (pipe) free_Pipe_list = *(Pipe_t **)pipe;
	return pipe;
}

void USBHost::free_Pipe(Pipe_t *pipe)
{
	*(Pipe_t **)pipe = free_Pipe_list;
	free_Pipe_list = pipe;
}

Transfer_t * USBHost::allocate_Transfer(void)
{
	Transfer_t *transfer = free_Transfer_list;
	if (transfer) free_Transfer_list = *(Transfer_t **)transfer;
	return transfer;
}

void USBHost::free_Transfer(Transfer_t *transfer)
{
	*(Transfer_t **)transfer = free_Transfer_list;
	free_Transfer_list = transfer;
}

