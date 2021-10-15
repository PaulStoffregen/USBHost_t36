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
 *
 * This file contains the MSC Extensions to the USB Host code. 
 */

#ifndef _USBHOST_MSC_H_
#define _USBHOST_MSC_H_

#include "USBHost_t36.h"

#include "msc/mscFS.h"

//--------------------------------------------------------------------------
// MSC FS object to process one partition of a 
class msFilesystem : public FS {
public:
	msFilesystem(USBHost &host) { init(); }
	msFilesystem(USBHost *host) { init(); }

	bool begin(msController *pDrive, bool setCwv = true, uint8_t part = 1);
	void init() {};

	File open(const char *filepath, uint8_t mode = FILE_READ);
	bool exists(const char *filepath);
	bool mkdir(const char *filepath);
	bool rename(const char *oldfilepath, const char *newfilepath);
	bool remove(const char *filepath);
	bool rmdir(const char *filepath);
	uint64_t usedSize();
	uint64_t totalSize();
	bool format(int type=0, char progressChar=0, Print& pr=Serial);
	bool mediaPresent();	
public: // allow access, so users can mix MSC & SdFat APIs
	UsbFs mscfs;
protected:
	uint64_t _cached_usedSize;
	bool 	_cached_usedSize_valid = false;
};




#endif
