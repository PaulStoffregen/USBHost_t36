#include <Arduino.h>
#include <USBHost_ms.h>
#include "msc/mscFS.h"


void msFilesystem::init() {
	msController::filesystem_ready_for_controller(this);
}

bool msFilesystem::begin(msController *pDrive, bool setCwv, uint8_t part) {
	return mscfs.begin(pDrive, setCwv, part);
}

File msFilesystem::open(const char *filepath, uint8_t mode) {
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
		if (mode == FILE_WRITE_BEGIN) {
			file.timestamp(T_CREATE, tm.year + 1900, tm.mon + 1,
			               tm.mday, tm.hour, tm.min, tm.sec);
		}
	}

	if (file) return File(new MSCFile(file));
	return File();
}
bool msFilesystem::exists(const char *filepath) {
	return mscfs.exists(filepath);
}
bool msFilesystem::mkdir(const char *filepath) {
	_cached_usedSize_valid = false;
	return mscfs.mkdir(filepath);
}
bool msFilesystem::rename(const char *oldfilepath, const char *newfilepath) {
	_cached_usedSize_valid = false;
	return mscfs.rename(oldfilepath, newfilepath);
}
bool msFilesystem::remove(const char *filepath) {
	_cached_usedSize_valid = false;
	return mscfs.remove(filepath);
}
bool msFilesystem::rmdir(const char *filepath) {
	_cached_usedSize_valid = false;
	return mscfs.rmdir(filepath);
}
uint64_t msFilesystem::usedSize() {
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
uint64_t msFilesystem::totalSize() {
	return (uint64_t)mscfs.clusterCount() * (uint64_t)mscfs.bytesPerCluster();
}

PFsLib pfsLIB1;
bool msFilesystem::format(int type, char progressChar, Print& pr) {
  if (mscfs.fatType() == FAT_TYPE_FAT12) {
    pr.printf("    Fat12 not supported\n");
    return false;
  }
  bool success = pfsLIB1.formatter(mscfs, 0, false, false, pr);

  return success;
}



bool msFilesystem::mediaPresent() {
	return true; // need to work on this...
}

//Glue code...
bool msFilesystem::claim_partition(msController *controller, uint8_t part) {
	#ifdef USBHOST_PRINT_DEBUG
	USBHDBGSerial.printf("msFilesystem::claim_partition = %x %x %u\n", (uint32_t)this, (uint32_t)controller, part);
	USBHDBGSerial.flush();
	#endif
	if (controller_ != nullptr) return false; // only grab one partition. 
	bool status = begin(controller, false, part);	
	#ifdef USBHOST_PRINT_DEBUG
	USBHDBGSerial.printf("  begin returned: %u", status);
	USBHDBGSerial.flush();
	#endif
	return status;
}

void msFilesystem::release_partition() {
	// Not sure but... 
	mscfs.end();
}

