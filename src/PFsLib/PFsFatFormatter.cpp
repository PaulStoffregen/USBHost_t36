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
#include "PFsLib.h"
#include "PFsFatFormatter.h"

//Set to 0 for debug info
#define DBG_Print	1
#if defined(DBG_Print)
#define DBGPrintf Serial.printf
#else
void inline DBGPrintf(...) {};
#endif

// Set nonzero to use calculated CHS in MBR.  Should not be required.
#define USE_LBA_TO_CHS 1

// Constants for file system structure optimized for flash.
uint16_t const BU16 = 128;
uint16_t  BU32 = 8192;
// Assume 512 byte sectors.
const uint16_t BYTES_PER_SECTOR = 512;
const uint16_t SECTORS_PER_MB = 0X100000/BYTES_PER_SECTOR;
const uint16_t FAT16_ROOT_ENTRY_COUNT = 512;
const uint16_t FAT16_ROOT_SECTOR_COUNT =
               32*FAT16_ROOT_ENTRY_COUNT/BYTES_PER_SECTOR;
//------------------------------------------------------------------------------
#define PRINT_FORMAT_PROGRESS 1
#if !PRINT_FORMAT_PROGRESS
#define writeMsg(str)
#elif defined(__AVR__)
#define writeMsg(str) if (m_pr) m_pr->print(F(str))
#else  // PRINT_FORMAT_PROGRESS
#define writeMsg(str) if (m_pr) m_pr->write(str)
#endif  // PRINT_FORMAT_PROGRESS

PFsLib pfslib;  // 
//------------------------------------------------------------------------------
bool PFsFatFormatter::format(PFsVolume &partVol, uint8_t fat_type, uint8_t* secBuf, print_t* pr) {
  DBGPrintf("\n### PFsFatFormatter::format called\n");

  bool rtn;
  m_secBuf = secBuf;
  m_pr = pr;
  m_dev = partVol.blockDevice();
  m_part = partVol.part()-1;  // convert to 0 biased. 

  
  uint32_t firstLBA;
  uint32_t sectorCount;
  uint32_t mbrLBA; 
  uint8_t mbrPart;


  PFsLib::voltype_t vt = pfslib.getPartitionInfo(m_dev, partVol.part(), pr, secBuf, firstLBA, sectorCount, mbrLBA, mbrPart);

  DBGPrintf("Part:%u vt:%u first:%u, count:%u MBR:%u MBR Part:%u\n", partVol.part(), (uint8_t)vt, firstLBA, sectorCount, mbrLBA, mbrPart);

  if (vt == PFsLib::INVALID_VOL) return false;

  // yes could have used some of the directly...
  m_sectorCount = sectorCount;
  m_part_relativeSectors = firstLBA;
  m_mbrLBA = mbrLBA;
  m_mbrPart = mbrPart;

  m_capacityMB = (m_sectorCount + SECTORS_PER_MB - 1)/SECTORS_PER_MB;
  
  bool has_volume_label = partVol.getVolumeLabel(volName, sizeof(volName));

  if (has_volume_label) {
	 DBGPrintf("Volume name:(%s)", volName);
  }
  DBGPrintf("\nPFsFatFormatter::format................");
  DBGPrintf("Sector Count: %d, Sectors/MB: %d\n", m_sectorCount, SECTORS_PER_MB);
  DBGPrintf("Partition Capacity (MB): %d\n", m_capacityMB);
  DBGPrintf("Fat Type: %d\n", partVol.fatType());
  DBGPrintf("    m_dataStart:%u\n", partVol.dataStartSector());
  DBGPrintf("    m_sectorsPerCluster:%u\n",partVol.sectorsPerCluster());
  DBGPrintf("    m_relativeSectors:%u\n", m_part_relativeSectors);
  //Serial.printf("    m_sectorsPerFat: %u\n", partVol.getFatVol()->sectorsPerFat());
  DBGPrintf("\n");
  
  if (m_capacityMB <= 6) {
    writeMsg("Card is too small.\r\n");
    return false;
  } else if (m_capacityMB <= 16) {
    m_sectorsPerCluster = 2;
  } else if (m_capacityMB <= 32) {
    m_sectorsPerCluster = 4;
  } else if (m_capacityMB <= 64) {
    m_sectorsPerCluster = 8;
  } else if (m_capacityMB <= 128) {
    m_sectorsPerCluster = 16;
  } else if (m_capacityMB <= 1024) {
    m_sectorsPerCluster = 32;
  } else if (m_capacityMB <= 32768) {
    m_sectorsPerCluster = 64;
  } else {
    // SDXC cards
    m_sectorsPerCluster = 128;
  }
    
  //rtn = m_sectorCount < 0X400000 ? makeFat16() :makeFat32();
  
  if(fat_type == 16 && m_sectorCount < 0X400000 ) {
	writeMsg("format makeFAT16\r\n");  
	rtn = makeFat16();
  } else if(fat_type == 32) {
	writeMsg("format makeFAT32\r\n");  
	rtn = makeFat32();
  }	else {
	  rtn = false;
  }
 
  if (rtn) {
    DBGPrintf("free clusters after format: %u\n", partVol.freeClusterCount());
    // what happens if I tell the partion to begin again?
    partVol.begin(m_dev, true, m_part+1);  // need to 1 bias again...
    DBGPrintf("free clusters after begin on partVol: %u\n", partVol.freeClusterCount());
	m_dev->syncDevice();
	
    if (has_volume_label) partVol.setVolumeLabel(volName);
    writeMsg("Format Done\r\n");
  } else {
    writeMsg("Format Failed\r\n");
  }
  setWriteSandBox(0, 0xffffffff);
  
  return rtn;
}

//----------------------------------------------------------------------------
bool PFsFatFormatter::writeSector(uint32_t sector, const uint8_t* src) {
  // sandbox support
  if ((sector < m_minSector) || (sector > m_maxSector)) {
    DBGPrintf("!!! Sandbox Error: %u <= %u <= %u - Press any key to continue\n", 
      m_minSector, sector, m_maxSector);
    while (Serial.read() == -1);
    while (Serial.read() != -1) ;
  }

  return m_dev->writeSector(sector, src);
}

//-----------------------------------------------------------------------------
bool PFsFatFormatter::createFatPartition(BlockDevice* dev, uint8_t fat_type, uint32_t startSector, uint32_t sectorCount, uint8_t* secBuf, print_t* pr) {
  bool rtn;
  
  m_dev = dev;
  m_secBuf = secBuf;
  m_pr = pr;
  m_sectorCount = sectorCount;
  m_part_relativeSectors = startSector;
  
  m_part = addPartitionToMbr();  
  if (m_part == 0xff) return false; // error in adding a partition to the MBR
  // Note the add partition code may change the sector count...
  m_capacityMB = (m_sectorCount + SECTORS_PER_MB - 1)/SECTORS_PER_MB;

  if (m_capacityMB <= 6) {
    writeMsg("Card is too small.\r\n");
    return false;
  } else if (m_capacityMB <= 16) {
    m_sectorsPerCluster = 2;
  } else if (m_capacityMB <= 32) {
    m_sectorsPerCluster = 4;
  } else if (m_capacityMB <= 64) {
    m_sectorsPerCluster = 8;
  } else if (m_capacityMB <= 128) {
    m_sectorsPerCluster = 16;
  } else if (m_capacityMB <= 1024) {
    m_sectorsPerCluster = 32;
  } else if (m_capacityMB <= 32768) {
    m_sectorsPerCluster = 64;
  } else {
    // SDXC cards
    m_sectorsPerCluster = 128;
  }

if (fat_type == 0) {
  if (m_capacityMB < 2048) fat_type = FAT_TYPE_FAT16;
  else fat_type = FAT_TYPE_FAT32;
}

  DBGPrintf("\nCreate Partition::format................");
  DBGPrintf("Sector Count: %d, Sectors/MB: %d\n", m_sectorCount, SECTORS_PER_MB);
  DBGPrintf("Partition Capacity (MB): %d\n", m_capacityMB);
  DBGPrintf("Fat Type: %d\n", fat_type);
  DBGPrintf("    m_relativeSectors:%u\n", m_part_relativeSectors);
  DBGPrintf("\n");

  if(fat_type == 16 && m_sectorCount < 0X400000 ) {
	 writeMsg("format makeFAT16\r\n");  
	 rtn = makeFat16();
  } else if(fat_type == 32) {
	 writeMsg("format makeFAT32\r\n");  
	 rtn = makeFat32();
  }	else {
	 rtn = false;
  }
  
  return rtn;
}

//====================================================================================
bool PFsFatFormatter::makeFat16() {
	
  DBGPrintf(" MAKEFAT16\n");
  uint32_t nc;
  uint32_t r;
  PbsFat_t* pbs = reinterpret_cast<PbsFat_t*>(m_secBuf);
  
  for (m_dataStart = 2*BU16; ; m_dataStart += BU16) {
    nc = (m_sectorCount - m_dataStart)/m_sectorsPerCluster;
    m_fatSize = (nc + 2 + (BYTES_PER_SECTOR/2) - 1)/(BYTES_PER_SECTOR/2);
    r = BU16 + 1 + 2*m_fatSize + FAT16_ROOT_SECTOR_COUNT;
    if (m_dataStart >= r) {
      m_relativeSectors = m_dataStart - r + BU16;
      break;
    }
  }

  //nc = (m_sectorCount - m_dataStart)/m_sectorsPerCluster;
  //m_fatSize = (nc + 2 + (BYTES_PER_SECTOR/2) - 1)/(BYTES_PER_SECTOR/2);
  
  DBGPrintf("m_relativeSectors: %u, m_fatSize: %u, m_dataStart: %u\n",m_relativeSectors, m_fatSize, m_dataStart) ;
  
  // check valid cluster count for FAT16 volume
  if (nc < 4085 || nc >= 65525) {
    writeMsg("Bad cluster count\r\n");
    return false;
  }
  m_reservedSectorCount = 1;
  m_fatStart = m_relativeSectors + m_reservedSectorCount;
  m_totalSectors = nc*m_sectorsPerCluster
                   + 2*m_fatSize + m_reservedSectorCount + 32;
  if (m_totalSectors < 65536) {
    m_partType = 0X04;
  } else {
    m_partType = 0X06;
  }

	//Added to keep relative sectors straight
	m_relativeSectors = m_part_relativeSectors;
	m_fatStart = m_relativeSectors + m_reservedSectorCount;
  	m_dataStart= m_fatStart + 2 * m_fatSize + FAT16_ROOT_SECTOR_COUNT;
	m_totalSectors = m_sectorCount;

  DBGPrintf("partType: %d, m_relativeSectors: %u, fatStart: %u, fatDatastart: %u, totalSectors: %u\n", m_partType, m_relativeSectors, m_fatStart, m_dataStart, m_totalSectors);

  // write MBR
  if (!writeMbr()) {
	return false;
  }

  initPbs();
  setLe16(pbs->bpb.bpb16.rootDirEntryCount, FAT16_ROOT_ENTRY_COUNT);
  setLe16(pbs->bpb.bpb16.sectorsPerFat16, m_fatSize);
  pbs->bpb.bpb16.physicalDriveNumber = 0X80;
  pbs->bpb.bpb16.extSignature = EXTENDED_BOOT_SIGNATURE;
  setLe32(pbs->bpb.bpb16.volumeSerialNumber, 1234567);
  
  for (size_t i = 0; i < sizeof(pbs->bpb.bpb16.volumeLabel); i++) {
    pbs->bpb.bpb16.volumeLabel[i] = ' ';
  }
  pbs->bpb.bpb16.volumeType[0] = 'F';
  pbs->bpb.bpb16.volumeType[1] = 'A';
  pbs->bpb.bpb16.volumeType[2] = 'T';
  pbs->bpb.bpb16.volumeType[3] = '1';
  pbs->bpb.bpb16.volumeType[4] = '6';
  if (!writeSector(m_relativeSectors, m_secBuf)) {
    return false;
  }
  
  return initFatDir(16, m_dataStart - m_fatStart);
  
}


//------------------------------------------------------------------------------
bool PFsFatFormatter::makeFat32() {
	DBGPrintf(" MAKEFAT32\n");
  uint32_t nc;
  uint32_t r;
  
  PbsFat_t* pbs = reinterpret_cast<PbsFat_t*>(m_secBuf);
  FsInfo_t* fsi = reinterpret_cast<FsInfo_t*>(m_secBuf);
  
  m_relativeSectors = BU32;
  for (m_dataStart = 2*BU32; ; m_dataStart += BU32) {
    nc = (m_sectorCount - m_dataStart)/m_sectorsPerCluster;
    m_fatSize = (nc + 2 + (BYTES_PER_SECTOR/4) - 1)/(BYTES_PER_SECTOR/4);
    r = m_relativeSectors + 9 + 2*m_fatSize;
	DBGPrintf("m_dataStart: %u, m_fatSize: %u, r: %u\n", m_dataStart, m_fatSize, r);
    if (m_dataStart >= r) {
      break;
    }
  }
  
    //nc = (m_sectorCount - m_dataStart)/m_sectorsPerCluster;
    //m_fatSize = (nc + 2 + (BYTES_PER_SECTOR/4) - 1)/(BYTES_PER_SECTOR/4);
	DBGPrintf("    m_part: %d\n", m_part);
    DBGPrintf("    m_sectorCount: %d\n", m_sectorCount);
    DBGPrintf("    m_dataStart: %d\n", m_dataStart);
    DBGPrintf("    m_sectorsPerCluster: %d\n", m_sectorsPerCluster);
    DBGPrintf("    nc: %d\n", nc);
    DBGPrintf("    m_fatSize: %d\n", m_fatSize);

  // error if too few clusters in FAT32 volume
  if (nc < 65525) {
    writeMsg("Bad cluster count\r\n");
    return false;
  }
  m_reservedSectorCount = m_dataStart - m_relativeSectors - 2*m_fatSize;
  m_fatStart = m_relativeSectors + m_reservedSectorCount;
  m_totalSectors = nc*m_sectorsPerCluster + m_dataStart - m_relativeSectors;
  // type depends on address of end sector
  // max CHS has lba = 16450560 = 1024*255*63
  if ((m_relativeSectors + m_totalSectors) <= 16450560) {
    // FAT32 with CHS and LBA
    m_partType = 0X0B;
  } else {
    // FAT32 with only LBA
    m_partType = 0X0C;
  }
  
  //Write MBR
  //Added to keep relative sectors straight
	m_relativeSectors = m_part_relativeSectors;
	m_fatStart = m_relativeSectors + m_reservedSectorCount;
	m_dataStart = m_relativeSectors + m_dataStart;
	m_totalSectors = m_sectorCount;
	
#if defined(DBG_Print)
  Serial.printf("partType: %d, m_relativeSectors: %u, fatStart: %u, fatDatastart: %u, totalSectors: %u\n", m_partType, m_relativeSectors, m_fatStart, m_dataStart, m_totalSectors);
#endif
 
  if (!writeMbr()) {
	return false;
  }

  
  initPbs();  
  setLe32(pbs->bpb.bpb32.sectorsPerFat32, m_fatSize);
  setLe32(pbs->bpb.bpb32.fat32RootCluster, 2);
  setLe16(pbs->bpb.bpb32.fat32FSInfoSector, 1);
  setLe16(pbs->bpb.bpb32.fat32BackBootSector, 6);
  pbs->bpb.bpb32.physicalDriveNumber = 0X80;
  pbs->bpb.bpb32.extSignature = EXTENDED_BOOT_SIGNATURE;
  setLe32(pbs->bpb.bpb32.volumeSerialNumber, 1234567);
  for (size_t i = 0; i < sizeof(pbs->bpb.bpb32.volumeLabel); i++) {
    pbs->bpb.bpb32.volumeLabel[i] = ' ';
  }
  pbs->bpb.bpb32.volumeType[0] = 'F';
  pbs->bpb.bpb32.volumeType[1] = 'A';
  pbs->bpb.bpb32.volumeType[2] = 'T';
  pbs->bpb.bpb32.volumeType[3] = '3';
  pbs->bpb.bpb32.volumeType[4] = '2';

  writeMsg("Writing Partition Boot Sector\n");
  if (!writeSector(m_relativeSectors, m_secBuf)  ||
      !writeSector(m_relativeSectors + 6, m_secBuf)) {
    return false;
  }
  // write extra boot area and backup
  memset(m_secBuf, 0 , BYTES_PER_SECTOR);
  setLe32(fsi->trailSignature, FSINFO_TRAIL_SIGNATURE);
  if (!writeSector(m_relativeSectors + 2, m_secBuf)  ||
      !writeSector(m_relativeSectors + 8, m_secBuf)) {
    return false;
  }
  // write FSINFO sector and backup
  setLe32(fsi->leadSignature, FSINFO_LEAD_SIGNATURE);
  setLe32(fsi->structSignature, FSINFO_STRUCT_SIGNATURE);
  setLe32(fsi->freeCount, 0XFFFFFFFF);
  setLe32(fsi->nextFree, 0XFFFFFFFF);
  writeMsg("Writing FSInfo Sector\n");
  if (!writeSector(m_relativeSectors + 1, m_secBuf)  ||
      !writeSector(m_relativeSectors + 7, m_secBuf)) {
    return false;
  }
  writeMsg("Writing FAT\n");
  return initFatDir(32, 2*m_fatSize + m_sectorsPerCluster);

}

//------------------------------------------------------------------------------
bool PFsFatFormatter::writeMbr() {
  if (m_mbrLBA == 0xFFFFFFFFUL) {
    DBGPrintf("    writeMBR - GPT entry so dont update\n");
    return true;
  }
  memset(m_secBuf, 0, BYTES_PER_SECTOR);
  
  // The relative sectors stuff is setup based off of the logicalMBR...
  uint32_t relativeSectors = m_relativeSectors - m_mbrLBA;

  MbrSector_t* mbr = reinterpret_cast<MbrSector_t*>(m_secBuf);
  MbrPart_t *pt = &mbr->part[m_mbrPart];
  if (!m_dev->readSector(m_mbrLBA, m_secBuf)) {
	writeMsg("Didn't read MBR Sector !!!\n");
	return false;
  }

#if USE_LBA_TO_CHS
  lbaToMbrChs(pt->beginCHS, m_capacityMB, relativeSectors);
  lbaToMbrChs(pt->endCHS, m_capacityMB,
              relativeSectors + m_totalSectors -1);
#else  // USE_LBA_TO_CHS
  pt->beginCHS[0] = 1;
  pt->beginCHS[1] = 1;
  pt->beginCHS[2] = 0;
  pt->endCHS[0] = 0XFE;
  pt->endCHS[1] = 0XFF;
  pt->endCHS[2] = 0XFF;
#endif  // USE_LBA_TO_CHS

  pt->type = m_partType;
  setLe32(pt->relativeSectors, relativeSectors);
  setLe32(pt->totalSectors, m_totalSectors);
  setLe16(mbr->signature, MBR_SIGNATURE);
  return writeSector(m_mbrLBA, m_secBuf);

}

uint8_t PFsFatFormatter::addPartitionToMbr() {
  uint32_t last_start_sector = m_dev->sectorCount();  // get the devices total sector count
  DBGPrintf("\nPFsFatFormatter::addPartitionToMbr: %u %u %u\n", m_part_relativeSectors, m_sectorCount, last_start_sector);

  MbrSector_t* mbr = reinterpret_cast<MbrSector_t*>(m_secBuf);

  if (!m_dev->readSector(0, m_secBuf)) {
    writeMsg("Didn't read MBR Sector !!!\n");
    return 0xff; // did not read the sector.
  }
  dump_hexbytes(&mbr->part[0], 4*sizeof(MbrPart_t));

  MbrPart_t *pt = &mbr->part[0]; 
  // Quick test for GPT disk
  if (pt->type == 0xee) {
    writeMsg("GPT disk - add ExFat Partition not supported yet !!!\n");
    return 0xff;
  }

  int part_index = 3; // zero index;
  pt = &mbr->part[part_index];
  uint32_t part_sector_start = getLe32(pt->relativeSectors);
  uint32_t part_total_sectors = getLe32(pt->totalSectors);

  uint16_t sig = getLe16(mbr->signature);
  if (sig != MBR_SIGNATURE) {
    // not valid we will use the whole thing
    memset(m_secBuf, 0, 512); 
    part_index = 0;
  } else {
    // lets look through to see if we have any empty slots
    DBGPrintf("    p %u: %u %u %u: ", part_index, pt->type, part_sector_start, part_total_sectors);
    if (pt->type && part_sector_start && part_total_sectors) return 0xff; // We don't have any room in the MBR
    // loop through the blank ones
    while (part_index && (pt->type==0) && (part_sector_start== 0) && (part_total_sectors == 0)) {
      DBGPrintf(" - empty slot\n");
      part_index--;
      pt = &mbr->part[part_index];
      part_sector_start = getLe32(pt->relativeSectors);
      part_total_sectors = getLe32(pt->totalSectors);
      DBGPrintf("    p %u: %u %u %u: ", part_index, pt->type, part_sector_start, part_total_sectors);
    }
    // was empty.
    if  ((part_index==0) && (pt->type==0) && (part_sector_start== 0) && (part_total_sectors == 0)) {
      DBGPrintf(" - MBR empty\n");  
      return 0; // empty mbr...
    } 

    // Now see if we found the spot or if we need to move items down.

    while ((part_index >= 0) && (m_part_relativeSectors < part_sector_start)) {
      //move that item down
      memcpy((void*)&mbr->part[part_index+1], (void*)pt, sizeof(MbrPart_t));
      DBGPrintf("- > Move down\n");
      part_index--;
      last_start_sector = part_sector_start; // remember the last start...

      pt = &mbr->part[part_index];
      part_sector_start = getLe32(pt->relativeSectors);
      part_total_sectors = getLe32(pt->totalSectors);
      DBGPrintf("    p %d: %u %u %u: ", part_index, pt->type, part_sector_start, part_total_sectors);
    }
    // Now see if we are at the start or...
    DBGPrintf("- exited copy down\n");
    // We should be able to just increment back up...
    part_index++;

    // Now lets see about does it fit or if we should autofit...
    if (m_sectorCount == 0) {
      m_sectorCount = last_start_sector - m_part_relativeSectors;
      m_capacityMB = (m_sectorCount + SECTORS_PER_MB - 1)/SECTORS_PER_MB;
      DBGPrintf("    Adjust sector count: %u = %u - %u CAP: %u\n", m_sectorCount, last_start_sector, m_part_relativeSectors, m_capacityMB);
      if (m_capacityMB <= 6) {
        writeMsg("Gap not big enough.\r\n");
        return 0xff;
      }  
    }
    // Should we check for overlaps?
    if ((m_part_relativeSectors + m_sectorCount) > last_start_sector) {
      DBGPrintf(" - overlaps existing partition(%u + %u > %u\n", m_part_relativeSectors, m_sectorCount, last_start_sector);
      return 0xff;
    }

    DBGPrintf("    Return partion num: %u\n", part_index);
    // BUGBUG:: should probably test that we don't overlap on the other side...
    // probably don't need this, but:
    pt = &mbr->part[part_index];
    memset(pt, 0, sizeof(MbrPart_t));
  }
  DBGPrintf("After Add Partition\n");
  dump_hexbytes(&mbr->part[0], 4*sizeof(MbrPart_t));
  writeSector(0, m_secBuf);
  return part_index;

}

//-----------------------------------------------------------------------------

#define CSECTORS_PER_WRITE 32
bool PFsFatFormatter::initFatDir(uint8_t fatType, uint32_t sectorCount) {
  DBGPrintf("PFsFatFormatter::initFatDir(%u, %u)\n", fatType, sectorCount);
  size_t n;
  uint32_t fat_sector = 1;
  writeMsg("Writing FAT ");
  if (sectorCount >= CSECTORS_PER_WRITE) {
    uint8_t *large_buffer = (uint8_t *)malloc(BYTES_PER_SECTOR * CSECTORS_PER_WRITE);
    if (large_buffer) {
      memset(large_buffer, 0, BYTES_PER_SECTOR * CSECTORS_PER_WRITE);
      uint32_t sectors_remaining = sectorCount;
      uint32_t loops_per_dot = sectorCount/(32*CSECTORS_PER_WRITE);
      uint32_t loop_count = 0;
      while (sectors_remaining >= CSECTORS_PER_WRITE) {
        if (!m_dev->writeSectors(m_fatStart + fat_sector, large_buffer, CSECTORS_PER_WRITE)) {
           return false;
        }
        fat_sector += CSECTORS_PER_WRITE;
        sectors_remaining -= CSECTORS_PER_WRITE;
        if (++loop_count == loops_per_dot) {
          writeMsg(".");
          loop_count = 0;
        }
      }
      if (sectors_remaining) {
        if (!m_dev->writeSectors(m_fatStart + fat_sector, large_buffer, sectors_remaining)) {
           return false;
        }
        fat_sector += sectors_remaining;
      }
      free(large_buffer);
    }
  }
  if (fat_sector < sectorCount) {
    memset(m_secBuf, 0, BYTES_PER_SECTOR);
    for (; fat_sector < sectorCount; fat_sector++) {
      if (!writeSector(m_fatStart + fat_sector, m_secBuf)) {
         return false;
      }
      if ((fat_sector%(sectorCount/32)) == 0) {
        writeMsg(".");
      }
    }
  }
  writeMsg("\r\n");
  // Allocate reserved clusters and root for FAT32.
  m_secBuf[0] = 0XF8;
  n = fatType == 16 ? 4 : 12;
  for (size_t i = 1; i < n; i++) {
    m_secBuf[i] = 0XFF;
  }
  return writeSector(m_fatStart, m_secBuf) &&
         writeSector(m_fatStart + m_fatSize, m_secBuf);
}

//------------------------------------------------------------------------------
void PFsFatFormatter::initPbs() {
  PbsFat_t* pbs = reinterpret_cast<PbsFat_t*>(m_secBuf);
  memset(m_secBuf, 0, BYTES_PER_SECTOR);
  
  pbs->jmpInstruction[0] = 0XEB;
  pbs->jmpInstruction[1] = 0X76;
  pbs->jmpInstruction[2] = 0X90;
  for (uint8_t i = 0; i < sizeof(pbs->oemName); i++) {
    pbs->oemName[i] = ' ';
  }
  setLe16(pbs->bpb.bpb16.bytesPerSector, BYTES_PER_SECTOR);
  pbs->bpb.bpb16.sectorsPerCluster = m_sectorsPerCluster;
  setLe16(pbs->bpb.bpb16.reservedSectorCount, m_reservedSectorCount);
  pbs->bpb.bpb16.fatCount = 2;
  // skip rootDirEntryCount
  // skip totalSectors16
  pbs->bpb.bpb16.mediaType = 0XF8;
  // skip sectorsPerFat16
  // skip sectorsPerTrack
  // skip headCount
  setLe32(pbs->bpb.bpb16.hidddenSectors, m_relativeSectors);
  setLe32(pbs->bpb.bpb16.totalSectors32, m_totalSectors);
  // skip rest of bpb
  setLe16(pbs->signature, PBR_SIGNATURE);
}
//------------------------------------------------------------------------------


// bgnLba = relSector;
// endLba = relSector + partSize - 1;
void PFsFatFormatter::lbaToMbrChs(uint8_t* chs, uint32_t capacityMB, uint32_t lba) {
  uint32_t c;
  uint8_t h;
  uint8_t s;

  uint8_t numberOfHeads;
  uint8_t sectorsPerTrack = capacityMB <= 256 ? 32 : 63;
  
  if (capacityMB <= 16) {
    numberOfHeads = 2;
  } else if (capacityMB <= 32) {
    numberOfHeads = 4;
  } else if (capacityMB <= 128) {
    numberOfHeads = 8;
  } else if (capacityMB <= 504) {
    numberOfHeads = 16;
  } else if (capacityMB <= 1008) {
    numberOfHeads = 32;
  } else if (capacityMB <= 2016) {
    numberOfHeads = 64;
  } else if (capacityMB <= 4032) {
    numberOfHeads = 128;
  } else {
    numberOfHeads = 255;
  }
  c = lba / (numberOfHeads * sectorsPerTrack);
  if (c <= 1023) {
    h = (lba % (numberOfHeads * sectorsPerTrack)) / sectorsPerTrack;
    s = (lba % sectorsPerTrack) + 1;
  } else {
    c = 1023;
    h = 254;
    s = 63;
  }
  chs[0] = h;
  chs[1] = ((c >> 2) & 0XC0) | s;
  chs[2] = c;
}

void PFsFatFormatter::dump_hexbytes(const void *ptr, int len)
{
  if (ptr == NULL || len <= 0) return;
  const uint8_t *p = (const uint8_t *)ptr;
  while (len) {
    for (uint8_t i = 0; i < 32; i++) {
      if (i > len) break;
      Serial.printf("%02X ", p[i]);
    }
    Serial.print(":");
    for (uint8_t i = 0; i < 32; i++) {
      if (i > len) break;
      Serial.printf("%c", ((p[i] >= ' ') && (p[i] <= '~')) ? p[i] : '.');
    }
    Serial.println();
    p += 32;
    len -= 32;
  }
}
