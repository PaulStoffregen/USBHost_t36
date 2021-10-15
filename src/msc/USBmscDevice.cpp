/**
 * Copyright (c) 2017-2020 Warren Watson
 * This file is part of the SdFat library for use with MSC.
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

#include "msc/USBMSCDevice.h"
#include "msc/USBmscInfo.h"

//#ifdef HAS_USB_MSC_CLASS
const uint32_t BUSY_TIMEOUT_MICROS = 1000000;

//static bool yieldTimeout(bool (*fcn)()); //Not used yet, if at all
//static bool waitTimeout(bool (*fcn)());  //Not used yet, if at all

static bool m_initDone = false;
static bool (*m_busyFcn)() = 0;
static uint8_t m_errorCode = MS_NO_MEDIA_ERR;
static uint32_t m_errorLine = 0;
//static msController *thisDrive = nullptr;
bool isBusyRead();
bool isBusyWrite();

//==============================================================================
// Error function and macro.
#define sdError(code) setSdErrorCode(code, __LINE__)
inline bool setSdErrorCode(uint8_t code, uint32_t line) {
  m_errorCode = code;
  m_errorLine = line;
  return false;
}

/* Not used yet if at all
//------------------------------------------------------------------------------
// Return true if timeout occurs.
static bool yieldTimeout(bool (*fcn)()) {
  m_busyFcn = fcn;
  uint32_t m = micros();
  while (fcn()) {
    if ((micros() - m) > BUSY_TIMEOUT_MICROS) {
      m_busyFcn = 0;
      return true;
    }
    SysCall::yield();
  }
  m_busyFcn = 0;
  return false;  // Caller will set errorCode.
}
//------------------------------------------------------------------------------
// Return true if timeout occurs.
static bool waitTimeout(bool (*fcn)()) {
  uint32_t m = micros();
  while (fcn()) {
    if ((micros() - m) > BUSY_TIMEOUT_MICROS) {
      return true;
    }
  }
  return false;  // Caller will set errorCode.
}
*/

bool USBMSCDevice::isBusyRead() {
	return thisDrive->mscTransferComplete;
}

bool USBMSCDevice::isBusyWrite() {
	return thisDrive->mscTransferComplete;
}

//------------------------------------------------------------------------------
uint8_t USBMSCDevice::errorCode() const {
  return m_errorCode;
}
//------------------------------------------------------------------------------
uint32_t USBMSCDevice::errorData() const {
  return 0;
}
//------------------------------------------------------------------------------
uint32_t USBMSCDevice::errorLine() const {
  return m_errorLine;
}

//------------------------------------------------------------------------------
bool USBMSCDevice::isBusy() {
  return m_busyFcn ? m_busyFcn() : !m_initDone && !thisDrive->mscTransferComplete;
}

//------------------------------------------------------------------------------
bool USBMSCDevice::readUSBDriveInfo(msDriveInfo_t * driveInfo) {
	memcpy(driveInfo, &thisDrive->msDriveInfo, sizeof(msDriveInfo_t));
  return true;
}

//------------------------------------------------------------------------------
bool USBMSCDevice::syncDevice() {
  return true;
}

//------------------------------------------------------------------------------
uint8_t USBMSCDevice::usbType() const {
  return  SD_CARD_TYPE_USB;
}

//------------------------------------------------------------------------------
uint32_t USBMSCDevice::status() {
  return m_errorCode;
}

//------------------------------------------------------------------------------
uint32_t USBMSCDevice::sectorCount() {
  return thisDrive->msDriveInfo.capacity.Blocks;
}

//==============================================================================
// Start of USBMSCDevice member functions.
//==============================================================================
bool USBMSCDevice::begin(msController *pDrive) {
	m_errorCode = MS_CBW_PASS;
	thisDrive = pDrive;
	pDrive->mscInit(); // Do initial init of each instance of a MSC object.
	if((m_errorCode = pDrive->checkConnectedInitialized())) {// Check for Connected USB drive.
		m_initDone = false;
	} else {
		m_initDone = true;
	}
	return m_initDone;
}

//------------------------------------------------------------------------------
bool USBMSCDevice::readSector(uint32_t sector, uint8_t* dst) {
  return readSectors(sector, dst, 1);
}
//------------------------------------------------------------------------------
bool USBMSCDevice::readSectors(uint32_t sector, uint8_t* dst, size_t n) {
	// Check if device is plugged in and initialized
	if((m_errorCode = ((msController *)thisDrive)->checkConnectedInitialized()) != MS_CBW_PASS) {
		return false;
	}
	m_errorCode = thisDrive->msReadBlocks(sector, n,
	              (uint16_t)thisDrive->msDriveInfo.capacity.BlockSize, dst);
	if(m_errorCode) {
		return false;
	}
	return true;
}

//------------------------------------------------------------------------------
bool USBMSCDevice::readSectorsWithCB(uint32_t sector, size_t ns, void (*callback)(uint32_t, uint8_t *), uint32_t token) {
  // Check if device is plugged in and initialized
  if((m_errorCode = ((msController *)thisDrive)->checkConnectedInitialized()) != MS_CBW_PASS) {
    return false;
  }
  m_errorCode = thisDrive->msReadSectorsWithCB(sector, ns, callback, token);
  if(m_errorCode) {
    return false;
  }
  return true;

}


//------------------------------------------------------------------------------
bool USBMSCDevice::writeSector(uint32_t sector, const uint8_t* src) {
  return writeSectors(sector, src, 1);
}
//------------------------------------------------------------------------------
bool USBMSCDevice::writeSectors(uint32_t sector, const uint8_t* src, size_t n) {
	// Check if device is plugged in and initialized
	if((m_errorCode = ((msController *)thisDrive)->checkConnectedInitialized()) != MS_CBW_PASS) {
		return false;
	}
	m_errorCode = thisDrive->msWriteBlocks(sector, n,
	              (uint16_t)thisDrive->msDriveInfo.capacity.BlockSize, src);
	if(m_errorCode) {
		return false;
	}
  return true;
}
//#endif // HAS_USB_MSC_CLASS
