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

#ifndef USBmscDevice_h
#define USBmscDevice_h
//#include "../common/SysCall.h"

//#ifdef HAS_USB_MSC_CLASS
#include "msc/USBmscInterface.h"
#include "USBHost_t36.h"

/**
 * \class USBMSCDevice
 * \brief Raw USB Drive accesss.
 */
class USBMSCDevice : public USBmscInterface {
 public:
  /** Initialize the USB MSC device.
   * \param[in] Pointer to an instance of msc.
   * \return true for success or false for failure.
   */
  bool begin(msController *pDrive);
  uint32_t sectorCount();
  /**
   * \return code for the last error. See USBmscInfo.h for a list of error codes.
   */
  uint8_t errorCode() const;
  /** \return error data for last error. */
  uint32_t errorData() const;
  /** \return error line for last error. Tmp function for debug. */
  uint32_t errorLine() const;
  /**
   * Check for busy with CMD13.
   *
   * \return true if busy else false.
   */
  bool isBusy();
  /** Check for busy with MSC read operation
   *
   * \return true if busy else false.
   */
  bool isBusyRead();
  /** Check for busy with MSC read operation
   *
   * \return true if busy else false.
   */
  bool isBusyWrite();
  /**
   * Read a USB drive's information. This contains the drive's identification
   * information such as Manufacturer ID, Product name, Product serial
   * number and Manufacturing date pluse more.
   *
   * \param[out]  msDriveInfo_t pointer to area for returned data.
   *
   * \return true for success or false for failure.
   */
  bool readUSBDriveInfo(msDriveInfo_t * driveInfo);
  /** Return the card type: SD V1, SD V2 or SDHC
   * \return 0 - SD V1, 1 - SD V2, or 3 - SDHC.
   */
  uint8_t usbType() const;
  /**
   * Read a 512 byte sector from an USB MSC drive.
   *
   * \param[in] sector Logical sector to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool readSector(uint32_t sector, uint8_t* dst);
  /**
   * Read multiple 512 byte sectors from an USB MSC drive.
   *
   * \param[in] sector Logical sector to be read.
   * \param[in] ns Number of sectors to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool readSectors(uint32_t sector, uint8_t* dst, size_t ns);
  /** \return USB MSC drive status. */
  uint32_t status();
  /** \return success if sync successful. Not for user apps. */
  bool syncDevice();
  /**
   * Writes a 512 byte sector to an USB MSC drive.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeSector(uint32_t sector, const uint8_t* src);
  /**
   * Write multiple 512 byte sectors to an USB MSC drive.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] ns Number of sectors to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeSectors(uint32_t sector, const uint8_t* src, size_t ns);

  /**
   * Read multiple 512 byte sectors from an USB MSC drive, using 
   * a callback per sector
   *
   * \param[in] sector Logical sector to be read.
   * \param[in] ns Number of sectors to be read.
   * \param[in] callback function to call for each sector read.
   * \return true for success or false for failure.
   */
  bool readSectorsWithCB(uint32_t sector, size_t ns, void (*callback)(uint32_t, uint8_t *), uint32_t token);


private:
  msController *thisDrive;
};
//#endif // HAS_USB_MSC_CLASS
#endif  // USBmscDevice_h
