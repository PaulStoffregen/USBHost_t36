/**
 * Copyright (c) 2011-2019 Bill Greiman
 * Modified for use with MSC Copyright (c) 2017-2020 Warren Watson
 * This file is part of the SdFat library for SD memory cards.
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
#ifndef USBFat_h
#define USBFat_h
/**
 * \file
 * \brief main UsbFs include file.
 */
#include "USBHost_t36.h"
#include "msc/USBmsc.h"
#include "PFsLib/PFsLib.h"

//------------------------------------------------------------------------------
/** MSCFat version */
#define MSC_FAT_VERSION "1.0.0"
//==============================================================================
/**
 * \class UsbBase
 * \brief base USB file system template class.
 */
template <class Vol>
class UsbBase : public Vol {
 public:
  //----------------------------------------------------------------------------
  /** Initialize USB drive and file system.
   *
   * \param[in] msController drive.
   * \return true for success or false for failure.
   */
  bool begin(msController *pdrv, bool setCwv = true, uint8_t part = 1) {
	return mscBegin(pdrv, setCwv, part);
  }
  //----------------------------------------------------------------------------
  /** Initialize USB drive and file system for USB Drive.
   *
   * \param[in] msController drive configuration.
   * \return true for success or false for failure.
   */
  bool mscBegin(msController *pDrive, bool setCwv = true, uint8_t part = 1) {
    Serial.printf("UsbBase::mscBegin called %x %x %d\n", (uint32_t)pDrive, setCwv, part); Serial.flush();
    if (!usbDriveBegin(pDrive)) return false;
    Serial.println("    After usbDriveBegin"); Serial.flush();
    return Vol::begin((USBMSCDevice*)m_USBmscDrive, setCwv, part);
  }
  //----------------------------------------------------------------------------
  /** \return Pointer to USB MSC object. */
  mscDevice* usbDrive() {return m_USBmscDrive;}
  //---------------------------------------------------------------------------
  /** Initialize USB MSC drive.
   *
   * \param[in] Pointer to an instance of msc.
   * \return true for success or false for failure.
   */
  bool usbDriveBegin(msController *pDrive) {
    m_USBmscDrive = m_USBmscFactory.newMSCDevice(pDrive);
	thisMscDrive = pDrive;
    return m_USBmscDrive && !m_USBmscDrive->errorCode();
  }
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] pr Print destination.
   */
  void errorHalt(print_t* pr) {
    if (mscErrorCode()) {
      pr->print(F("mscError: 0X"));
      pr->print(mscErrorCode(), HEX);
      pr->print(F(",0X"));
      pr->println(mscErrorData(), HEX);
    } else if (!Vol::fatType()) {
      pr->println(F("Check USB drive format."));
    }
    SysCall::halt();
  }
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] pr Print destination.
   * \param[in] msg Message to print.
   */
  void errorHalt(print_t* pr, const char* msg) {
    pr->print(F("error: "));
    pr->println(msg);
    errorHalt(pr);
  }
  //----------------------------------------------------------------------------
  /** %Print msg and halt.
   *
   * \param[in] pr Print destination.
   * \param[in] msg Message to print.
   */
  void errorHalt(print_t* pr, const __FlashStringHelper* msg) {
    pr->print(F("error: "));
    pr->println(msg);
    errorHalt(pr);
  }
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] pr Print destination.
   */
  void initErrorHalt(print_t* pr) {
    initErrorPrint(pr);
    SysCall::halt();
  }
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] pr Print destination.
   * \param[in] msg Message to print.
   */
  void initErrorHalt(print_t* pr, const char* msg) {
    pr->println(msg);
    initErrorHalt(pr);
  }
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] pr Print destination.
   * \param[in] msg Message to print.
   */
  void initErrorHalt(Print* pr, const __FlashStringHelper* msg) {
    pr->println(msg);
    initErrorHalt(pr);
  }
  //----------------------------------------------------------------------------
  /** Print error details after begin() fails.
   *
   * \param[in] pr Print destination.
   */
  void initErrorPrint(Print* pr) {
    pr->println(F("begin() failed"));
    if (mscErrorCode()) {
      pr->println(F("Do not reformat the USB drive."));
      if (mscErrorCode() == MS_NO_MEDIA_ERR) {
        pr->println(F("Is USB drive connected?"));
      }
    }
    errorPrint(pr);
  }
  //----------------------------------------------------------------------------
  /** %Print volume FAT/exFAT type.
   *
   * \param[in] pr Print destination.
   */
  void printFatType(print_t* pr) {
    if (Vol::fatType() == FAT_TYPE_EXFAT) {
      pr->print(F("exFAT"));
    } else {
      pr->print(F("FAT"));
      pr->print(Vol::fatType());
    }
  }
  //----------------------------------------------------------------------------
  /** %Print USB drive errorCode and errorData.
   *
   * \param[in] pr Print destination.
   */
  void errorPrint(print_t* pr) {
    if (mscErrorCode()) {
      pr->print(F("mscError: 0X"));
      pr->println(mscErrorCode(), HEX);
//      pr->print(F(",0X"));
//      pr->println(mscErrorData(), HEX);
    } else if (!Vol::fatType()) {
      pr->println(F("Check USB drive format."));
    }
  }
  //----------------------------------------------------------------------------
  /** %Print msg, any USB drive error code.
   *
   * \param[in] pr Print destination.
   * \param[in] msg Message to print.
   */
  void errorPrint(print_t* pr, char const* msg) {
    pr->print(F("error: "));
    pr->println(msg);
    errorPrint(pr);
  }

  /** %Print msg, any USB drive error code.
   *
   * \param[in] pr Print destination.
   * \param[in] msg Message to print.
   */
  void errorPrint(Print* pr, const __FlashStringHelper* msg) {
    pr->print(F("error: "));
    pr->println(msg);
    errorPrint(pr);
  }
  //----------------------------------------------------------------------------
  /** %Print error info and return.
   *
   * \param[in] pr Print destination.
   */
  void printMscError(print_t* pr) {
    if (mscErrorCode()) {
      if (mscErrorCode() == 0x28) {
        pr->println(F("No USB drive detected, plugged in?"));
      }
      pr->print(F("USB drive error: "));
      pr->print(F("0x"));
      pr->print(mscErrorCode(), HEX);
      pr->print(F(",0x"));
      pr->print(mscErrorData(), HEX);
      printMscAscError(pr, thisMscDrive);
    } else if (!Vol::fatType()) {
      pr->println(F("Check USB drive format."));
    }
  }
  //----------------------------------------------------------------------------
  /** \return USB drive error code. */
  uint8_t mscErrorCode() {
    if (m_USBmscDrive) {
      return m_USBmscDrive->errorCode();
    }
    return SD_CARD_ERROR_INVALID_CARD_CONFIG; //TODO: change this!
  }
  //----------------------------------------------------------------------------
  /** \return SD card error data. */
  uint8_t mscErrorData() {return m_USBmscDrive ? m_USBmscDrive->errorData() : 0;}
  //----------------------------------------------------------------------------
  /** \return pointer to base volume */
  Vol* vol() {return reinterpret_cast<Vol*>(this);}
  //----------------------------------------------------------------------------
  /** Initialize file system after call to cardBegin.
   *
   * \return true for success or false for failure.
   */
  bool volumeBegin() {
     return Vol::begin(m_USBmscDrive);
  }
#if ENABLE_ARDUINO_SERIAL
  /** Print error details after begin() fails. */
  void initErrorPrint() {
    initErrorPrint(&Serial);
  }
  //----------------------------------------------------------------------------
  /** %Print msg to Serial and halt.
   *
   * \param[in] msg Message to print.
   */
  void errorHalt(const __FlashStringHelper* msg) {
    errorHalt(&Serial, msg);
  }
  //----------------------------------------------------------------------------
  /** %Print error info to Serial and halt. */
  void errorHalt() {errorHalt(&Serial);}
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] msg Message to print.
   */
  void errorHalt(const char* msg) {errorHalt(&Serial, msg);}
  //----------------------------------------------------------------------------
  /** %Print error info and halt. */
  void initErrorHalt() {initErrorHalt(&Serial);}
  //----------------------------------------------------------------------------
  /** %Print msg, any SD error code.
   *
   * \param[in] msg Message to print.
   */
  void errorPrint(const char* msg) {errorPrint(&Serial, msg);}
   /** %Print msg, any SD error code.
   *
   * \param[in] msg Message to print.
   */
  void errorPrint(const __FlashStringHelper* msg) {errorPrint(&Serial, msg);}
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] msg Message to print.
   */
  void initErrorHalt(const char* msg) {initErrorHalt(&Serial, msg);}
  //----------------------------------------------------------------------------
  /** %Print error info and halt.
   *
   * \param[in] msg Message to print.
   */
  void initErrorHalt(const __FlashStringHelper* msg) {
    initErrorHalt(&Serial, msg);
  }
#endif  // ENABLE_ARDUINO_SERIAL
  //----------------------------------------------------------------------------
 private:
  mscDevice*  m_USBmscDrive;
  USBmscFactory m_USBmscFactory;
  msController *thisMscDrive;
};
//------------------------------------------------------------------------------
/**
 * \class UsbFat32
 * \brief MSC file system class for FAT volumes.
 */
class UsbFat32 : public UsbBase<FatVolume> {
 public:
  /** Format a USB drive FAT32/FAT16.
   *
   * \param[in] pr Optional Print information.
   * \return true for success or false for failure.
   */
  bool format(print_t* pr = nullptr) {
    FatFormatter fmt;
    uint8_t* cache = reinterpret_cast<uint8_t*>(cacheClear());
    if (!cache) {
      return false;
    }
    return fmt.format(usbDrive(), cache, pr);
  }
};
//------------------------------------------------------------------------------
/**
 * \class UsbExFat
 * \brief MSC file system class for exFAT volumes.
 */
class UsbExFat : public UsbBase<ExFatVolume> {
 public:
  /** Format a USB drive exFAT.
   *
   * \param[in] pr Optional Print information.
   * \return true for success or false for failure.
   */
  bool format(print_t* pr = nullptr) {
    ExFatFormatter fmt;
    uint8_t* cache = reinterpret_cast<uint8_t*>(cacheClear());
    if (!cache) {
      return false;
    }
    return fmt.format(usbDrive(), cache, pr);
  }
};
//------------------------------------------------------------------------------
/**
 * \class USBFs
 * \brief SD file system class for FAT16, FAT32, and exFAT volumes.
 */
class UsbFs : public UsbBase<PFsVolume> {
 public:
  /** Format a SD card FAT or exFAT.
   *
   * \param[in] pr Optional Print information.
   * \return true for success or false for failure.
   */
  bool format(print_t* pr = nullptr) {
    static_assert(sizeof(m_volMem) >= 512, "m_volMem too small");
    uint32_t sectorCount = usbDrive()->sectorCount();
    if (sectorCount == 0) {
      return false;
    }
    end();
    if (sectorCount > 67108864) {
      ExFatFormatter fmt;
      return fmt.format(usbDrive(), reinterpret_cast<uint8_t*>(m_volMem), pr);
    } else {
      FatFormatter fmt;
      return fmt.format(usbDrive(), reinterpret_cast<uint8_t*>(m_volMem), pr);
    }
  }
};
//------------------------------------------------------------------------------
#if SDFAT_FILE_TYPE == 1
///** Select type for SdFat. */
typedef UsbFat32 UsbFat;
/** Select type for File. */
#if !defined(__has_include) || !__has_include(<FS.h>)
typedef File32 File;
#endif
/** Select type for SdBaseFile. */
typedef FatFile UsbBaseFile;
#elif SDFAT_FILE_TYPE == 2
typedef UsbExFat UsbFat;
#if !defined(__has_include) || !__has_include(<FS.h>)
typedef ExFile File;
#endif
typedef ExFatFile UsbBaseFile;
#elif SDFAT_FILE_TYPE == 3
typedef UsbFs UsbFat;
#if !defined(__has_include) || !__has_include(<FS.h>)
typedef FsFile File;
#endif
typedef FsBaseFile UsbBaseFile;
#else  // SDFAT_FILE_TYPE
#error Invalid SDFAT_FILE_TYPE
#endif  // SDFAT_FILE_TYPE
//------------------------------------------------------------------------------
//typedef UsbFs UsbFat;
//typedef FsBaseFile UsbBaseFile;
/**
 * \class SdFile
 * \brief FAT16/FAT32 file with Print.
 */
class MscFile : public PrintFile<UsbBaseFile> {
 public:
  MscFile() {}
  /** Create an open SdFile.
   * \param[in] path path for file.
   * \param[in] oflag open flags.
   */
  MscFile(const char* path, oflag_t oflag) {
    open(path, oflag);
  }
  /** Set the date/time callback function
   *
   * \param[in] dateTime The user's call back function.  The callback
   * function is of the form:
   *
   * \code
   * void dateTime(uint16_t* date, uint16_t* time) {
   *   uint16_t year;
   *   uint8_t month, day, hour, minute, second;
   *
   *   // User gets date and time from GPS or real-time clock here
   *
   *   // return date using FS_DATE macro to format fields
   *   *date = FS_DATE(year, month, day);
   *
   *   // return time using FS_TIME macro to format fields
   *   *time = FS_TIME(hour, minute, second);
   * }
   * \endcode
   *
   * Sets the function that is called when a file is created or when
   * a file's directory entry is modified by sync(). All timestamps,
   * access, creation, and modify, are set when a file is created.
   * sync() maintains the last access date and last modify date/time.
   *
   */
  static void dateTimeCallback(
    void (*dateTime)(uint16_t* date, uint16_t* time)) {
    FsDateTime::setCallback(dateTime);
  }
  /**  Cancel the date/time callback function. */
  static void dateTimeCallbackCancel() {
    FsDateTime::clearCallback();
  }
};
#endif  // USBFat_h
