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

#include "msc/USBmscInfo.h"

const char *decodeSenseKey(uint8_t senseKey) {
	static char msg[64];
#undef SENSE_KEY_MAP
	switch (senseKey) {
#define SENSE_KEY_MAP(_name_, _val_) \
		case _val_: return #_name_ ;
		SENSE_KEY_LIST
	}
#undef SENSE_KEY_MAP

	snprintf(msg, sizeof(msg), "UNKNOWN SENSE KEY(%02Xh)", senseKey);
	return msg;
}

const char *decodeAscAscq(uint8_t asc, uint8_t ascq) {
	static char msg[64];
	uint16_t ascAscq = asc<<8 | ascq;

	switch (ascAscq) {
#define SENSE_CODE_KEYED(_asc_, _fmt_)
#define SENSE_CODE(_asc_, _ascq_, _msg_) case _asc_<<8 | _ascq_: return _msg_;
	ASC_NUM_LIST
#undef SENSE_CODE
#undef SENSE_CODE_KEYED
	}

#define SENSE_CODE_KEYED(_asc_, _fmt_) if (asc == _asc_) { snprintf(msg, sizeof(msg), _fmt_, ascq); return msg; }
#define SENSE_CODE(_asc_, _ascq_, _msg_)
	ASC_NUM_LIST
#undef SENSE_CODE
#undef SENSE_CODE_KEYED

	snprintf(msg, sizeof(msg), "UNKNOWN ASC/ASCQ (%02Xh/%02Xh)", asc, ascq);
	return msg;
}

//------------------------------------------------------------------------------
void printMscAscError(print_t* pr, msController *pDrive) {
		Serial.printf(" --> Type: %s Cause: %s\n",
		decodeSenseKey(pDrive->msSense.SenseKey),
		decodeAscAscq(pDrive->msSense.AdditionalSenseCode,
		pDrive->msSense.AdditionalSenseQualifier));

}
