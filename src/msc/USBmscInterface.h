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

#ifndef USBmscInterface_h
#define USBmscInterface_h
#include "msc/USBmscInfo.h"
/**
 * \class USBmscInterface
 * \brief Abstract interface for a USB Mass Storage Device.
 */
class USBmscInterface : public BlockDeviceInterface {
 public:
  /** \return error code. */
  virtual uint8_t errorCode() const = 0;
  /** \return error data. */
  virtual uint32_t errorData() const = 0;
  /** \return true if USB is busy. */
  virtual bool isBusy() = 0;
  /** \return true if USB read is busy. */
  virtual bool isBusyRead();
  /** \return true if USB write is busy. */
  virtual bool isBusyWrite();
    /** Read a MSC USB drive's info.
   * \return true for success or false for failure.
   */
  virtual bool readUSBDriveInfo(msDriveInfo_t * driveInfo) = 0;
  /** Return the USB Drive type: USB MSC
   * \return 4 - USB MSC.
   */
  virtual uint8_t usbType() const = 0;
  /**
   * Determine the size of a USB Mass Storage Device.
   *
   * \return The number of 512 byte data sectors in the USB device
   *         or zero if an error occurs.
   */
  virtual uint32_t sectorCount() = 0;
  /** \return USB drive status. */
  virtual uint32_t status() {return 0XFFFFFFFF;}

  virtual bool readSectorsWithCB(uint32_t sector, size_t ns, void (*callback)(uint32_t, uint8_t *), uint32_t token) = 0;

};
#endif  // USBmscInterface_h
