/* MSC library compatibility wrapper for use of SdFat on Teensy
 * Copyright (c) 2020, Warren Watson, wwatson4506@gmail.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __MSCFS_H__
#define __MSCFS_H__

#include <Arduino.h>
#include "msc/USBFat.h"
// Use FILE_READ & FILE_WRITE as defined by FS.h
#if defined(FILE_READ) && !defined(FS_H)
#undef FILE_READ
#endif
#if defined(FILE_WRITE) && !defined(FS_H)
#undef FILE_WRITE
#endif
#include <FS.h>
#if defined(__arm__)
  // Support everything on 32 bit boards with enough memory
  #define MSCFAT_BASE UsbFs
  #define MSC_MAX_FILENAME_LEN 256
#endif

class MSCFile : public FileImpl
{
private:
	// Classes derived from File are never meant to be constructed
	// anywhere other than open() in the parent FS class and
	// openNextFile() while traversing a directory.
	// Only the abstract File class which references these derived
	// classes is meant to have a public constructor!
	MSCFile(const PFsFile &file) : mscfatfile(file), filename(nullptr) { }
	friend class msFilesystem;
public:
	virtual ~MSCFile(void) {
		if (mscfatfile) mscfatfile.close();
		if (filename) free(filename);
	}

	virtual size_t write(const void *buf, size_t size) {
		return mscfatfile.write(buf, size);
	}
	virtual int peek() {
		return mscfatfile.peek();
	}
	virtual int available() {
		return mscfatfile.available();
	}
	virtual void flush() {
		mscfatfile.flush();
	}
	virtual size_t read(void *buf, size_t nbyte) {
		return mscfatfile.read(buf, nbyte);
	}
	virtual bool truncate(uint64_t size=0) {
		return mscfatfile.truncate(size);
	}
	virtual bool seek(uint64_t pos, int mode = SeekSet) {
		if (mode == SeekSet) return mscfatfile.seekSet(pos);
		if (mode == SeekCur) return mscfatfile.seekCur(pos);
		if (mode == SeekEnd) return mscfatfile.seekEnd(pos);
		return false;
	}
	virtual uint64_t position() {
		return mscfatfile.curPosition();
	}
	virtual uint64_t size() {
		return mscfatfile.size();
	}
	virtual void close() {
		if (filename) {
			free(filename);
			filename = nullptr;
		}
		mscfatfile.close();
	}
	virtual bool isOpen() {
		return mscfatfile.isOpen();
	}
	virtual const char * name() {
		if (!filename) {
			filename = (char *)malloc(MSC_MAX_FILENAME_LEN);
			if (filename) {
				mscfatfile.getName(filename, MSC_MAX_FILENAME_LEN);
			} else {
				static char zeroterm = 0;
				filename = &zeroterm;
			}
		}
		return filename;
	}
	virtual boolean isDirectory(void) {
		return mscfatfile.isDirectory();
	}
	virtual File openNextFile(uint8_t mode=0) {
		PFsFile file = mscfatfile.openNextFile();
		if (file) return File(new MSCFile(file));
		return File();
	}
	virtual void rewindDirectory(void) {
		mscfatfile.rewindDirectory();
	}

	virtual bool getCreateTime(DateTimeFields &tm) {
		uint16_t fat_date, fat_time;
		if (!mscfatfile.getCreateDateTime(&fat_date, &fat_time)) { return false; }
		tm.sec = FS_SECOND(fat_time);
		tm.min = FS_MINUTE(fat_time);
		tm.hour = FS_HOUR(fat_time);
		tm.mday = FS_DAY(fat_date);
		tm.mon = FS_MONTH(fat_date) - 1;
		tm.year = FS_YEAR(fat_date) - 1900;
		return true;
	}
	virtual bool getModifyTime(DateTimeFields &tm) {
		uint16_t fat_date, fat_time;
		if (!mscfatfile.getModifyDateTime(&fat_date, &fat_time)) { return false; }
		tm.sec = FS_SECOND(fat_time);
		tm.min = FS_MINUTE(fat_time);
		tm.hour = FS_HOUR(fat_time);
		tm.mday = FS_DAY(fat_date);
		tm.mon = FS_MONTH(fat_date) - 1;
		tm.year = FS_YEAR(fat_date) - 1900;
		return true;
	}
	virtual bool setCreateTime(const DateTimeFields &tm) {
		if (tm.year < 80 || tm.year > 207) { return false; }
		return mscfatfile.timestamp(T_CREATE, tm.year + 1900, tm.mon + 1,
			tm.mday, tm.hour, tm.min, tm.sec);
	}
	virtual bool setModifyTime(const DateTimeFields &tm) {
		  uint32_t now1 = Teensy3Clock.get();
		  DateTimeFields tm1;  
		  breakTime(now1, tm1);
		if (tm1.year < 80 || tm1.year > 207) { return false; }
		return mscfatfile.timestamp(T_WRITE, tm1.year + 1900, tm1.mon + 1,
			tm1.mday, tm1.hour, tm1.min, tm1.sec);
	}
	//using Print::write;
private:
	PFsFile mscfatfile;
	char *filename;
};

#if 0
class MSCClass : public FS
{
public:
	MSCClass() { }
	bool begin(msController *pDrive, bool setCwv = true, uint8_t part = 1) {
		return mscfs.begin(pDrive, setCwv, part);
	}
	File open(const char *filepath, uint8_t mode = FILE_READ) {
		oflag_t flags = O_READ;
		if (mode == FILE_WRITE) {flags = O_RDWR | O_CREAT | O_AT_END; _cached_usedSize_valid = false;}
		else if (mode == FILE_WRITE_BEGIN) {flags = O_RDWR | O_CREAT;  _cached_usedSize_valid = false; }
		PFsFile file = mscfs.open(filepath, flags);
		uint32_t now1 = Teensy3Clock.get();
		DateTimeFields tm;  
		breakTime(now1, tm);
		if (tm.year < 80 || tm.year > 207) 
		{
			Serial.println("Date out of Range....");
		} else {
			file.timestamp(T_WRITE, tm.year + 1900, tm.mon + 1,
				tm.mday, tm.hour, tm.min, tm.sec);
			if(mode == FILE_WRITE_BEGIN) {
				file.timestamp(T_CREATE, tm.year + 1900, tm.mon + 1,
					tm.mday, tm.hour, tm.min, tm.sec);
			}
		}
		
		if (file) return File(new MSCFile(file));
			return File();
	}
	bool exists(const char *filepath) {
		return mscfs.exists(filepath);
	}
	bool mkdir(const char *filepath) {
		_cached_usedSize_valid = false;
		return mscfs.mkdir(filepath);
	}
	bool rename(const char *oldfilepath, const char *newfilepath) {
		_cached_usedSize_valid = false;
		return mscfs.rename(oldfilepath, newfilepath);
	}
	bool remove(const char *filepath) {
		_cached_usedSize_valid = false;
		return mscfs.remove(filepath);
	}
	bool rmdir(const char *filepath) {
		_cached_usedSize_valid = false;
		return mscfs.rmdir(filepath);
	}
	uint64_t usedSize() {	
		#if 1
		return  (uint64_t)(mscfs.clusterCount() - mscfs.freeClusterCount())
		  		* (uint64_t)mscfs.bytesPerCluster();
		#else
		Serial.printf("$$$mscFS::usedSize %u %llu\n", _cached_usedSize_valid, _cached_usedSize);
		if (!_cached_usedSize_valid) {
			_cached_usedSize_valid = true;
			_cached_usedSize =  (uint64_t)(mscfs.clusterCount() - mscfs.freeClusterCount())
		  		* (uint64_t)mscfs.bytesPerCluster();
	  	}
		return _cached_usedSize;
		#endif
	}
	uint64_t totalSize() {
		return (uint64_t)mscfs.clusterCount() * (uint64_t)mscfs.bytesPerCluster();
	}
	bool format(int type=0, char progressChar=0, Print& pr=Serial) { return false; }
	
public: // allow access, so users can mix MSC & SdFat APIs
	UsbFs mscfs;
protected:
	uint64_t _cached_usedSize;
	bool 	_cached_usedSize_valid = false;
};

extern MSCClass MSC;

// do not expose these defines in Arduino sketches or other libraries
#undef MSCFAT_FILE
#undef MSCFAT_BASE
#undef MSC_MAX_FILENAME_LEN

#define SD_CARD_TYPE_USB 4

class MSC2Drive
{
public:
	bool init(msController *pDrive) {
		return MSC.begin(pDrive);
	}
	uint8_t usbType() {
		return MSC.mscfs.usbDrive()->usbType();
	}
};

class MSCVolume
{
public:
	bool init(MSC2Drive &usbDrive) {
		return MSC.mscfs.vol() != nullptr;
	}
	uint8_t fatType() {
		return MSC.mscfs.vol()->fatType();
	}
	uint32_t blocksPerCluster() {
		return MSC.mscfs.vol()->sectorsPerCluster();
	}
	uint32_t clusterCount() {
		return MSC.mscfs.vol()->clusterCount();
	}
};
#endif

#endif
