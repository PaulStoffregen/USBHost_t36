/**
 * Copyright (c) 2011-2019 Bill Greiman
 * This file is part of the SdFat library for SD memory cards.
 *
 * Modified 2020 for use with SdFat and MSC. By Warren Watson.
 * 
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef USBmscInfo_h
#define USBmscInfo_h
#include <stdint.h>
#include "SdFat.h"
#include "USBHost_t36.h"
#include "msc/mscSenseKeyList.h"
#include "msc/mscASCList.h"

const char *decodeSenseKey(uint8_t senseKey);
const char *decodeAscAscq(uint8_t asc, uint8_t ascq);

void printMscAscError(print_t* pr, msController *pDrive);

const uint8_t SD_CARD_TYPE_USB = 4;
//-----------------------------------------------------------------------------

inline uint32_t USBmscCapacity(msController *pDrv) {
	return (pDrv->msDriveInfo.capacity.Blocks); 
}

#endif  // USBmscInfo_h
