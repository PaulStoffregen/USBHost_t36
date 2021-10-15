/**
 * Copyright (c) 2011-2020 Bill Greiman
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
#ifndef PFsLib_h
#define PFsLib_h
/**
 * \file
 * \brief PFsLib include file.
 */
#include "PFsVolume.h"
#include "PFsFile.h"
#include "PFsFatFormatter.h"
#include "PFsExFatFormatter.h"

class PFsLib : public PFsFatFormatter, public PFsExFatFormatter
{
 public:
	bool deletePartition(BlockDeviceInterface *blockDev, uint8_t part, print_t* pr, Stream &Serialx); 
	void InitializeDrive(BlockDeviceInterface *dev, uint8_t fat_type, print_t* pr);
	bool formatter(PFsVolume &partVol, uint8_t fat_type=0, bool dump_drive=false, bool g_exfat_dump_changed_sectors=false, Stream &Serialx=Serial);
	void dump_hexbytes(const void *ptr, int len);
	void print_partion_info(PFsVolume &partVol, Stream &Serialx);
	uint32_t mbrDmp(BlockDeviceInterface *blockDev, uint32_t device_sector_count, Stream &Serialx);
	void compare_dump_hexbytes(const void *ptr, const uint8_t *compare_buf, int len);

 private:
	BlockDevice* m_dev;
	print_t*m_pr;

};
 
#endif  // PFsLib_h
