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
#define DBG_FILE "PFsExFatFormatter.cpp"
//#include "../common/DebugMacros.h"
#include "PFsLib.h"
#include "PFsExFatFormatter.h"

// Compatibility to SDFAT 2.1.1
#ifndef DBG_FAIL_MACRO
#define DBG_FAIL_MACRO
#endif
uint16_t toUpcase(uint16_t chr);

//Set to 0 for debug info
#define DBG_PRINT  // for more printing. 
#define DBG_Print	0
#if defined(DBG_Print)
#define DBGPrintf Serial.printf
#else
void inline DBGPrintf(...) {};
#endif

//------------------------------------------------------------------------------
// Formatter assumes 512 byte sectors.
const uint32_t BOOT_BACKUP_OFFSET = 12;
const uint16_t BYTES_PER_SECTOR = 512;
const uint16_t SECTOR_MASK = BYTES_PER_SECTOR - 1;
const uint8_t  BYTES_PER_SECTOR_SHIFT = 9;
const uint16_t MINIMUM_UPCASE_SKIP = 512;
const uint32_t BITMAP_CLUSTER = 2;
const uint32_t UPCASE_CLUSTER = 3;
const uint32_t ROOT_CLUSTER = 4;
const uint16_t SECTORS_PER_MB = 0X100000/BYTES_PER_SECTOR;

//------------------------------------------------------------------------------
#define PRINT_FORMAT_PROGRESS 1
#if !PRINT_FORMAT_PROGRESS
#define writeMsg(pr, str)
#elif defined(__AVR__)
#define writeMsg(pr, str) if (pr) pr->print(F(str))
#else  // PRINT_FORMAT_PROGRESS
#define writeMsg(pr, str) if (pr) pr->write(str)
#endif  // PRINT_FORMAT_PROGRESS

static PFsLib pfslib;  // 
//------------------------------------------------------------------------------

bool PFsExFatFormatter::format(PFsVolume &partVol, uint8_t* secBuf, print_t* pr) {
#if !PRINT_FORMAT_PROGRESS
(void)pr;
#endif  //  !PRINT_FORMAT_PROGRESS
  DBGPrintf("\n### PFsExFatFormatter::format called\n");
  checksum = 0;

  ExFatPbs_t* pbs;
  DirUpcase_t* dup;
  DirBitmap_t* dbm;
  DirLabel_t* label;

  uint32_t firstLBA;
  uint32_t sectorCount;
  uint32_t mbrLBA; 
  uint8_t mbrPart;
  uint8_t vs;

  m_secBuf = secBuf;
  m_pr = pr;
  m_dev = partVol.blockDevice();
  m_part = partVol.part()-1;  // convert to 0 biased. 

  PFsLib::voltype_t vt = pfslib.getPartitionInfo(m_dev, partVol.part(), pr, secBuf, firstLBA, sectorCount, mbrLBA, mbrPart);

  DBGPrintf("Part:%u vt:%u first:%u, count:%u MBR:%u MBR Part:%u\n", partVol.part(), (uint8_t)vt, firstLBA, sectorCount, mbrLBA, mbrPart);

  if (vt == PFsLib::INVALID_VOL) return false;

  // yes could have used some of the directly...
  m_relativeSectors = firstLBA;
  m_sectorCount = sectorCount;
  m_mbrLBA = mbrLBA;
  m_mbrPart = mbrPart;
  
  bool has_volume_label = partVol.getVolumeLabel(volName, sizeof(volName));

  #if defined(DBG_PRINT)
	DBGPrintf("    m_sectorsPerCluster:%u\n", partVol.sectorsPerCluster());
	DBGPrintf("    m_relativeSectors:%u\n", m_relativeSectors);
	DBGPrintf("    m_fatStartSector: %u\n", partVol.fatStartSector());
	DBGPrintf("    m_fatType: %d\n", partVol.fatType());
	DBGPrintf("    m_clusterCount: %u\n", partVol.clusterCount());
	DBGPrintf("    m_totalSectors: %u\n", sectorCount);
	DBGPrintf("\n");
  #endif
  
  // Min size is 512 MB
  if (sectorCount < 0X100000) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  
  //BYTES_PER_SECTOR = partVol.getExFatVol()->bytesPerSector();
  //SECTOR_MASK = BYTES_PER_SECTOR - 1;
  //ROOT_CLUSTER = partVol.getExFatVol()->rootDirectoryCluster();
  
  // Determine partition layout.
  for (m = 1, vs = 0; m && sectorCount > m; m <<= 1, vs++) {}
  sectorsPerClusterShift = vs < 29 ? 8 : (vs - 11)/2;
  //sectorsPerClusterShift = partVol.getExFatVol()->sectorsPerClusterShift();
  //DBGPrintf("Calculate sectorsPerClusterShift = %u\n", vs < 29 ? 8 : (vs - 11)/2);
  
  //1 << n is the same as raising 2 to the power n
  sectorsPerCluster = 1UL << sectorsPerClusterShift;
  //sectorsPerCluster = partVol.getExFatVol()->sectorsPerCluster();
  //DBGPrintf("Calculated sectorsPerCluster = %u\n",  1UL << sectorsPerClusterShift);
  
  //The ClusterCount field shall describe the number of clusters the Cluster Heap contains
  //   (VolumeLength - ClusterHeapOffset) / 2^SectorsPerClusterShift rounded down to the nearest integer, which is exactly the number of clusters which can fit between the beginning of the Cluster Heap and the end of the volume
  //   232- 11, which is the maximum number of clusters a FAT can describe
  //clusterCount = (sectorCount - 4*fatLength) >> sectorsPerClusterShift;
  clusterCount = partVol.clusterCount();
  //DBGPrintf("Calculated clusterCount = %u\n", (sectorCount - 4*fatLength) >> sectorsPerClusterShift);
  
  //The ClusterHeapOffset field shall describe the volume-relative sector offset of the Cluster Heap
  //   At least FatOffset + FatLength * NumberOfFats, to account for the sectors all the preceding regions consume
  //   At most 2^32- 1 or VolumeLength - (ClusterCount * 2^SectorsPerClusterShift), whichever calculation is less
  //clusterHeapOffset = 2*fatLength;
  clusterHeapOffset = partVol.getExFatVol()->clusterHeapStartSector() - m_relativeSectors;
    
  //The FatLength field shall describe the length, in sectors, of each FAT table
  //   At least (ClusterCount + 2) * 2^2/ 2^BytesPerSectorShift rounded up to the nearest integer
  //   At most (ClusterHeapOffset - FatOffset) / NumberOfFats rounded down to the nearest integer
  //fatLength = 1UL << (vs < 27 ? 13 : (vs + 1)/2);
  //fatLength = partVol.getExFatVol()->fatLength();
  //DBGPrintf("Calculated fatLength1 = %u\n",1UL << (vs < 27 ? 13 : (vs + 1)/2));
  //DBGPrintf("Calculated fatLength2 = %u\n", (clusterCount + 2) * (1UL<<2)/(1UL<<BYTES_PER_SECTOR_SHIFT));
  fatLength = ((clusterCount + 2) * (1UL<<2)/(1UL<<BYTES_PER_SECTOR_SHIFT)) + 1;
  
  //The FatOffset field shall describe the volume-relative sector offset of the First FAT
  //   At least 24, which accounts for the sectors the Main Boot and Backup Boot regions consume
  //   At most ClusterHeapOffset - (FatLength * NumberOfFats), which accounts for the sectors the Cluster Heap consumes
  //fatOffset = fatLength;
  //fatOffset = partVol.fatStartSector() - m_relativeSectors;
  //DBGPrintf("Calculated fatOffset = %u\n", clusterHeapOffset - fatLength);
  fatOffset = clusterHeapOffset - fatLength;
  
  //The PartitionOffset field shall describe the media-relative sector offset of the partition which hosts the given exFAT volume
  //partitionOffset = 2*fatLength;
  partitionOffset = m_relativeSectors;
  
  
  //The VolumeLength field shall describe the size of the given exFAT volume in sectors
  //   At least 2^20/ 2^BytesPerSectorShift, which ensures the smallest volume is no less than 1MB
  //   At most 264- 1, the largest value this field can describe
  //volumeLength = clusterHeapOffset + (clusterCount << sectorsPerClusterShift);
  volumeLength = sectorCount;
  
  #if defined(DBG_PRINT)
    DBGPrintf("VS: %d\n", vs);
    DBGPrintf("sectorsPerClusterShift: %u,\n sectorsPerCluster: %u,\n fatLength %u\n", sectorsPerClusterShift, sectorsPerCluster, fatLength);
    DBGPrintf("fatOffset: %u,\n partitionOffset: %u\n", fatOffset, partitionOffset);
  	DBGPrintf("clusterHeapOffset: %u,\n clusterCount: %u,\n volumeLength %u\n", clusterHeapOffset, clusterCount, volumeLength);

  	DBGPrintf("cluster 2 bitmap: %u\n",partitionOffset + clusterHeapOffset);
  	DBGPrintf("Up case table: %u\n",partitionOffset + clusterHeapOffset + sectorsPerCluster);

// =============================== DEBUG
  pbs = reinterpret_cast<ExFatPbs_t*>(secBuf);
  if (m_dev->readSector(firstLBA, m_secBuf)) {
    DBGPrintf("\n *** PBS data ***\n");
    DBGPrintf("\tFirstLBA: %u\n", firstLBA);
    DBGPrintf("\tpartitionOffset: %llu %u\n",getLe64(pbs->bpb.partitionOffset), partitionOffset);
    DBGPrintf("\tvolumeLength: %llu %u\n",getLe64(pbs->bpb.volumeLength), volumeLength);
    DBGPrintf("\tfatOffset: %u %u\n",getLe32(pbs->bpb.fatOffset), fatOffset);
    DBGPrintf("\tfatLength: %u %u\n",getLe32(pbs->bpb.fatLength), fatLength);
    DBGPrintf("\tclusterHeapOffset: %u %u\n",getLe32(pbs->bpb.clusterHeapOffset), clusterHeapOffset);
    DBGPrintf("\tclusterCount: %u %u\n",getLe32(pbs->bpb.clusterCount), clusterCount);
    DBGPrintf("\trootDirectoryCluster: %u %u\n",getLe32(pbs->bpb.rootDirectoryCluster), ROOT_CLUSTER);
    DBGPrintf("\tvolumeSerialNumber: %u %u\n",getLe32(pbs->bpb.volumeSerialNumber), sectorCount);
    DBGPrintf("\tfileSystemRevision: %u %u\n",getLe16(pbs->bpb.fileSystemRevision), 0X100);
    DBGPrintf("\tvolumeFlags: %u %u\n",getLe16(pbs->bpb.volumeFlags), 0);

    //Serial.println("*** Hit any key to continue $ to abort");
    //int ch;
    //while((ch=Serial.read()) == -1);
    //while(Serial.read() != -1);
    //if (ch == '$') { DBGPrintf("*** Aborted ***"); return false; }
  }
  #endif

//--------------------  WRITE MBR ----------

  if (!writeMbr()) {
    DBG_FAIL_MACRO;
    goto fail;
  }  

  // Debug Set the sand box 
  setWriteSandBox(firstLBA, firstLBA + sectorCount - 1);

  writeMsg(pr, "Writing Partition Boot Sector\n");
  // Partition Boot sector.
  memset(secBuf, 0, BYTES_PER_SECTOR);
  pbs = reinterpret_cast<ExFatPbs_t*>(secBuf);

  pbs->jmpInstruction[0] = 0XEB;
  pbs->jmpInstruction[1] = 0X76;
  pbs->jmpInstruction[2] = 0X90;
  pbs->oemName[0] = 'E';
  pbs->oemName[1] = 'X';
  pbs->oemName[2] = 'F';
  pbs->oemName[3] = 'A';
  pbs->oemName[4] = 'T';
  pbs->oemName[5] = ' ';
  pbs->oemName[6] = ' ';
  pbs->oemName[7] = ' ';
  setLe64(pbs->bpb.partitionOffset, partitionOffset);
  setLe64(pbs->bpb.volumeLength, volumeLength);
  setLe32(pbs->bpb.fatOffset, fatOffset);
  setLe32(pbs->bpb.fatLength, fatLength);
  setLe32(pbs->bpb.clusterHeapOffset, clusterHeapOffset);
  setLe32(pbs->bpb.clusterCount, clusterCount);
  setLe32(pbs->bpb.rootDirectoryCluster, ROOT_CLUSTER);
  setLe32(pbs->bpb.volumeSerialNumber, sectorCount);
    
  setLe16(pbs->bpb.fileSystemRevision, 0X100);
  setLe16(pbs->bpb.volumeFlags, 0);
  pbs->bpb.bytesPerSectorShift = BYTES_PER_SECTOR_SHIFT;
  pbs->bpb.sectorsPerClusterShift = sectorsPerClusterShift;
  pbs->bpb.numberOfFats = 1;
  pbs->bpb.driveSelect = 0X80;
  pbs->bpb.percentInUse = 0;

  // Fill boot code like official SDFormatter.
  for (size_t i = 0; i < sizeof(pbs->bootCode); i++) {
    pbs->bootCode[i] = 0XF4;
  }
  setLe16(pbs->signature, PBR_SIGNATURE);
  for (size_t i = 0; i < BYTES_PER_SECTOR; i++) {
    if (i == offsetof(ExFatPbs_t, bpb.volumeFlags[0]) ||
        i == offsetof(ExFatPbs_t, bpb.volumeFlags[1]) ||
        i == offsetof(ExFatPbs_t, bpb.percentInUse)) {
      continue;
    }
    checksum = exFatChecksum(checksum, secBuf[i]);
  }
    
  sector = partitionOffset;
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
#endif
  if (!writeSector(sector, secBuf)  ||
      !writeSector(sector + BOOT_BACKUP_OFFSET , secBuf)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
    
    
  writeMsg(pr, "Write eight Extended Boot Sectors\n");
  sector++;
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
#endif
  // Write eight Extended Boot Sectors.
  memset(secBuf, 0, BYTES_PER_SECTOR);
  setLe16(pbs->signature, PBR_SIGNATURE);
  for (int j = 0; j < 8; j++) {
    for (size_t i = 0; i < BYTES_PER_SECTOR; i++) {
      checksum = exFatChecksum(checksum, secBuf[i]);
    }
    if (!writeSector(sector, secBuf)  ||
        !writeSector(sector + BOOT_BACKUP_OFFSET , secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    sector++;
  }
    
  writeMsg(pr, "Write OEM Parameter Sector and reserved sector\n");
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
#endif
  // Write OEM Parameter Sector and reserved sector.
  memset(secBuf, 0, BYTES_PER_SECTOR);
  for (int j = 0; j < 2; j++) {
    for (size_t i = 0; i < BYTES_PER_SECTOR; i++) {
      checksum = exFatChecksum(checksum, secBuf[i]);
    }
    if (!writeSector(sector, secBuf)  ||
        !writeSector(sector + BOOT_BACKUP_OFFSET , secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    sector++;
  }
  
  writeMsg(pr, "Write Boot CheckSum Sector\n");
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
#endif
  // Write Boot CheckSum Sector.
  for (size_t i = 0; i < BYTES_PER_SECTOR; i += 4) {
    setLe32(secBuf + i, checksum);
  }
  if (!writeSector(sector, secBuf)  ||
      !writeSector(sector + BOOT_BACKUP_OFFSET , secBuf)) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  // Initialize FAT.
  writeMsg(pr, "Writing exFAT ");
  sector = partitionOffset + fatOffset;
  
  //The + 2 is because the first two entries in a FAT do not represent clusters.
  //      The FatEntry[0] field shall describe the media type in the first byte (the lowest order byte) and shall contain FFh 
  //      in the remaining three bytes. The media type (the first byte) should be F8h
  //Media type is generally ignored so my bug of not setting it correctly was not caught earlier.
  //      The FatEntry[1] field only exists due to historical precedence and does not describe anything of interest.
  //The )*4 is because entries are four bytes. The expression rounds up to a whole number of sectors.
  ns = ((clusterCount + 2)*4 + BYTES_PER_SECTOR - 1)/BYTES_PER_SECTOR;
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d, ns: %u\n", sector-partitionOffset, ns);
#endif
  memset(secBuf, 0, BYTES_PER_SECTOR);
  // Allocate two reserved clusters, bitmap, upcase, and root clusters.
	  secBuf[0] = 0XF8;
	  for (size_t i = 1; i < 20; i++) {
		secBuf[i] = 0XFF;
	  }
  for (uint32_t i = 0; i < ns; i++) {
    if (i%(ns/32) == 0) {
      writeMsg(pr, ".");
    }
    if (!writeSector(sector + i, secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (i == 0) {
      memset(secBuf, 0, BYTES_PER_SECTOR);
    }
  }
  writeMsg(pr, "\r\n");
  
  //==================================================================
  writeMsg(pr, "Write cluster two, bitmap\n");

  // Write cluster two, bitmap.
  sector = partitionOffset + clusterHeapOffset;
  // The 7)/8 converts clusterCount to bytes rounded up to whole bytes.
  bitmapSize = (clusterCount + 7)/8;  
  ns = (bitmapSize + BYTES_PER_SECTOR - 1)/BYTES_PER_SECTOR;
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
  DBGPrintf("sectorsPerCluster: %d, bitmapSize: %d, ns: %d\n", sectorsPerCluster, bitmapSize, ns);
#endif
  if (ns > sectorsPerCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  memset(secBuf, 0, BYTES_PER_SECTOR);
  // Allocate clusters for bitmap, upcase, and root.
  secBuf[0] = 0X7;
  for (uint32_t i = 1; i < ns; i++) {
    if (!writeSector(sector + i, secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (i == 0) {
      secBuf[0] = 0;
    }
  }
  
 
  // Write cluster three, upcase table.
  writeMsg(pr, "Writing upcase table\r\n");
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d\n", clusterHeapOffset + sectorsPerCluster);
#endif
  if (!writeUpcase(partitionOffset + clusterHeapOffset + sectorsPerCluster)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (m_upcaseSize > BYTES_PER_SECTOR*sectorsPerCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  
  // Initialize first sector of root.
  writeMsg(pr, "Writing root\r\n");
  ns = sectorsPerCluster;
  sector = partitionOffset + clusterHeapOffset + 2*sectorsPerCluster;
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting 1st Sector of root: %d\n", clusterHeapOffset + 2*sectorsPerCluster);
#endif
  memset(secBuf, 0, BYTES_PER_SECTOR);

  // Unused Label entry.
  label = reinterpret_cast<DirLabel_t*>(secBuf);
  label->type = EXFAT_TYPE_LABEL & 0X7F;

  // bitmap directory  entry.
  dbm = reinterpret_cast<DirBitmap_t*>(secBuf + 32);
  dbm->type = EXFAT_TYPE_BITMAP;
  setLe32(dbm->firstCluster, BITMAP_CLUSTER);
  setLe64(dbm->size, bitmapSize);

  // upcase directory entry.
  dup = reinterpret_cast<DirUpcase_t*>(secBuf +64);
  dup->type = EXFAT_TYPE_UPCASE;
  setLe32(dup->checksum, m_upcaseChecksum);
  setLe32(dup->firstCluster, UPCASE_CLUSTER);
  setLe64(dup->size, m_upcaseSize);

  // Write root, cluster four.
  for (uint32_t i = 0; i < ns; i++) {
    if (!writeSector(sector + i, secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (i == 0) {
      memset(secBuf, 0, BYTES_PER_SECTOR);
    }
  }
  writeMsg(pr, "Format done\r\n");
  // what happens if I tell the partion to begin again?
  partVol.begin(m_dev, true, m_part+1);  // need to 1 bias again...
  DBGPrintf("free clusters after begin on partVol: %u\n", partVol.freeClusterCount());
  if (has_volume_label) {
    writeMsg(pr, "Set Volume Label\n");
    partVol.setVolumeLabel(volName);
  }
  m_dev->syncDevice();
  setWriteSandBox(0, 0xffffffff);
  return true;

 fail:
  writeMsg(pr, "Format failed\r\n");
  setWriteSandBox(0, 0xffffffff);
  return false;
}

//----------------------------------------------------------------------------
bool PFsExFatFormatter::writeSector(uint32_t sector, const uint8_t* src) {
  // sandbox support
  if ((sector < m_minSector) || (sector > m_maxSector)) {
    DBGPrintf("!!! Sandbox Error: %u <= %u <= %u - Press any key to continue\n", 
      m_minSector, sector, m_maxSector);
    while (Serial.read() == -1);
    while (Serial.read() != -1) ;
  }

  return m_dev->writeSector(sector, src);
}

//----------------------------------------------------------------------------
bool PFsExFatFormatter::createExFatPartition(BlockDevice* dev, uint32_t startSector, uint32_t sectorCount, uint8_t* secBuf, print_t* pr) {
#if !PRINT_FORMAT_PROGRESS
(void)pr;
#endif  //  !PRINT_FORMAT_PROGRESS
  checksum = 0;

  //MbrSector_t mbr;
  ExFatPbs_t* pbs;
  DirUpcase_t* dup;
  DirBitmap_t* dbm;
  DirLabel_t* label;

  uint8_t vs;

  m_secBuf = secBuf;
  m_pr = pr;
  m_dev = dev;
  m_part_relativeSectors = startSector;

  m_part = addExFatPartitionToMbr();  
  
  if(m_part == 0xff) {
	writeMsg(m_pr, "failed to create partition\n");
	return false;
  }
  
  // Determine partition layout.  
  m_relativeSectors = startSector;
  //sectorCount = getLe32(pt->totalSectors);
  //#if defined(DBG_PRINT)
	//DBGPrintf("    m_relativeSectors: %u\n", m_part_relativeSectors);
	//DBGPrintf("    m_totalSectors: %u\n", sectorCount);
	//DBGPrintf("\n");
  //#endif
  
  // Min size is 512 MB
  if (sectorCount < 0X100000) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  
  //BYTES_PER_SECTOR = partVol.getExFatVol()->bytesPerSector();
  //SECTOR_MASK = BYTES_PER_SECTOR - 1;
  //ROOT_CLUSTER = partVol.getExFatVol()->rootDirectoryCluster();
  
  // Determine partition layout.
  for (m = 1, vs = 0; m && sectorCount > m; m <<= 1, vs++) {}
  sectorsPerClusterShift = vs < 29 ? 8 : (vs - 11)/2;
  //sectorsPerClusterShift = partVol.getExFatVol()->sectorsPerClusterShift();
  
  //1 << n is the same as raising 2 to the power n
  sectorsPerCluster = 1UL << sectorsPerClusterShift;
  //sectorsPerCluster = partVol.getExFatVol()->sectorsPerCluster();
  
  //The FatLength field shall describe the length, in sectors, of each FAT table
  //   At least (ClusterCount + 2) * 2^2/ 2^BytesPerSectorShiftrounded up to the nearest integer
  //   At most (ClusterHeapOffset - FatOffset) / NumberOfFats rounded down to the nearest integer
  fatLength = 1UL << (vs < 27 ? 13 : (vs + 1)/2);
  //fatLength = partVol.getExFatVol()->fatLength();
  
  //The FatOffset field shall describe the volume-relative sector offset of the First FAT
  //   At least 24, which accounts for the sectors the Main Boot and Backup Boot regions consume
  //   At most ClusterHeapOffset - (FatLength * NumberOfFats), which accounts for the sectors the Cluster Heap consumes
  fatOffset = fatLength;
  //fatOffset = partVol.fatStartSector() - m_relativeSectors;
  
  //The PartitionOffset field shall describe the media-relative sector offset of the partition which hosts the given exFAT volume
  //partitionOffset = 2*fatLength;
  partitionOffset = m_relativeSectors;
  
  //The ClusterHeapOffset field shall describe the volume-relative sector offset of the Cluster Heap
  //   At least FatOffset + FatLength * NumberOfFats, to account for the sectors all the preceding regions consume
  //   At most 2^32- 1 or VolumeLength - (ClusterCount * 2^SectorsPerClusterShift), whichever calculation is less
  clusterHeapOffset = 2*fatLength;
  //clusterHeapOffset = partVol.getExFatVol()->clusterHeapStartSector() - m_relativeSectors;
  
  //The ClusterCount field shall describe the number of clusters the Cluster Heap contains
  //   (VolumeLength - ClusterHeapOffset) / 2^SectorsPerClusterShift rounded down to the nearest integer, which is exactly the number of clusters which can fit between the beginning of the Cluster Heap and the end of the volume
  //   232- 11, which is the maximum number of clusters a FAT can describe
  clusterCount = (sectorCount - 4*fatLength) >> sectorsPerClusterShift;
  //clusterCount = partVol.clusterCount();
  
  //The VolumeLength field shall describe the size of the given exFAT volume in sectors
  //   At least 2^20/ 2^BytesPerSectorShift, which ensures the smallest volume is no less than 1MB
  //   At most 264- 1, the largest value this field can describe
  //volumeLength = clusterHeapOffset + (clusterCount << sectorsPerClusterShift);
  volumeLength = sectorCount;
  
    DBGPrintf("VS: %d\n", vs);
    Serial.printf("sectorsPerClusterShift: %u,\n sectorsPerCluster: %u,\n fatLength %u\n", sectorsPerClusterShift, sectorsPerCluster, fatLength);
    DBGPrintf("fatOffset: %u,\n partitionOffset: %u\n", fatOffset, partitionOffset);
  	DBGPrintf("clusterHeapOffset: %u,\n clusterCount: %u,\n volumeLength %u\n", clusterHeapOffset, clusterCount, volumeLength);
  	DBGPrintf("cluster 2 bitmap: %u\n",partitionOffset + clusterHeapOffset);
  	DBGPrintf("Up case table: %u\n",partitionOffset + clusterHeapOffset + sectorsPerCluster);
  
//--------------------  WRITE MBR ----------

  if (!writeMbr()) {
    DBG_FAIL_MACRO;
    goto fail;
  }  

  writeMsg(pr, "Writing Partition Boot Sector\n");
  // Partition Boot sector.
  memset(secBuf, 0, BYTES_PER_SECTOR);
  pbs = reinterpret_cast<ExFatPbs_t*>(secBuf);

  pbs->jmpInstruction[0] = 0XEB;
  pbs->jmpInstruction[1] = 0X76;
  pbs->jmpInstruction[2] = 0X90;
  pbs->oemName[0] = 'E';
  pbs->oemName[1] = 'X';
  pbs->oemName[2] = 'F';
  pbs->oemName[3] = 'A';
  pbs->oemName[4] = 'T';
  pbs->oemName[5] = ' ';
  pbs->oemName[6] = ' ';
  pbs->oemName[7] = ' ';
  setLe64(pbs->bpb.partitionOffset, partitionOffset);
  setLe64(pbs->bpb.volumeLength, volumeLength);
  setLe32(pbs->bpb.fatOffset, fatOffset);
  setLe32(pbs->bpb.fatLength, fatLength);
  setLe32(pbs->bpb.clusterHeapOffset, clusterHeapOffset);
  setLe32(pbs->bpb.clusterCount, clusterCount);
  setLe32(pbs->bpb.rootDirectoryCluster, ROOT_CLUSTER);
  setLe32(pbs->bpb.volumeSerialNumber, sectorCount);
    
  setLe16(pbs->bpb.fileSystemRevision, 0X100);
  setLe16(pbs->bpb.volumeFlags, 0);
  pbs->bpb.bytesPerSectorShift = BYTES_PER_SECTOR_SHIFT;
  pbs->bpb.sectorsPerClusterShift = sectorsPerClusterShift;
  pbs->bpb.numberOfFats = 1;
  pbs->bpb.driveSelect = 0X80;
  pbs->bpb.percentInUse = 0;

  // Fill boot code like official SDFormatter.
  for (size_t i = 0; i < sizeof(pbs->bootCode); i++) {
    pbs->bootCode[i] = 0XF4;
  }
  setLe16(pbs->signature, PBR_SIGNATURE);
  for (size_t i = 0; i < BYTES_PER_SECTOR; i++) {
    if (i == offsetof(ExFatPbs_t, bpb.volumeFlags[0]) ||
        i == offsetof(ExFatPbs_t, bpb.volumeFlags[1]) ||
        i == offsetof(ExFatPbs_t, bpb.percentInUse)) {
      continue;
    }
    checksum = exFatChecksum(checksum, secBuf[i]);
  }
  
  sector = partitionOffset;
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
  if (!writeSector(sector, secBuf)  ||
      !writeSector(sector + BOOT_BACKUP_OFFSET , secBuf)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  
  
  writeMsg(pr, "Write eight Extended Boot Sectors\n");
  sector++;
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
  // Write eight Extended Boot Sectors.
  memset(secBuf, 0, BYTES_PER_SECTOR);
  setLe16(pbs->signature, PBR_SIGNATURE);
  for (int j = 0; j < 8; j++) {
    for (size_t i = 0; i < BYTES_PER_SECTOR; i++) {
      checksum = exFatChecksum(checksum, secBuf[i]);
    }
    if (!writeSector(sector, secBuf)  ||
        !writeSector(sector + BOOT_BACKUP_OFFSET , secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    sector++;
  }
  
  writeMsg(pr, "Write OEM Parameter Sector and reserved sector\n");
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
  // Write OEM Parameter Sector and reserved sector.
  memset(secBuf, 0, BYTES_PER_SECTOR);
  for (int j = 0; j < 2; j++) {
    for (size_t i = 0; i < BYTES_PER_SECTOR; i++) {
      checksum = exFatChecksum(checksum, secBuf[i]);
    }
    if (!writeSector(sector, secBuf)  ||
        !writeSector(sector + BOOT_BACKUP_OFFSET , secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    sector++;
  }
  
  writeMsg(pr, "Write Boot CheckSum Sector\n");
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
  // Write Boot CheckSum Sector.
  for (size_t i = 0; i < BYTES_PER_SECTOR; i += 4) {
    setLe32(secBuf + i, checksum);
  }
  if (!writeSector(sector, secBuf)  ||
      !writeSector(sector + BOOT_BACKUP_OFFSET , secBuf)) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  // Initialize FAT.
  writeMsg(pr, "Writing exFAT ");
  sector = partitionOffset + fatOffset;
  
  //The + 2 is because the first two entries in a FAT do not represent clusters.
  //      The FatEntry[0] field shall describe the media type in the first byte (the lowest order byte) and shall contain FFh 
  //      in the remaining three bytes. The media type (the first byte) should be F8h
  //Media type is generally ignored so my bug of not setting it correctly was not caught earlier.
  //      The FatEntry[1] field only exists due to historical precedence and does not describe anything of interest.
  //The )*4 is because entries are four bytes. The expression rounds up to a whole number of sectors.
  ns = ((clusterCount + 2)*4 + BYTES_PER_SECTOR - 1)/BYTES_PER_SECTOR;
  DBGPrintf("\tWriting Sector: %d, ns: %u\n", sector-partitionOffset, ns);
#
  memset(secBuf, 0, BYTES_PER_SECTOR);
  // Allocate two reserved clusters, bitmap, upcase, and root clusters.
	  secBuf[0] = 0XF8;
	  for (size_t i = 1; i < 20; i++) {
		secBuf[i] = 0XFF;
	  }
  for (uint32_t i = 0; i < ns; i++) {
    if (i%(ns/32) == 0) {
      writeMsg(pr, ".");
    }
    if (!writeSector(sector + i, secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (i == 0) {
      memset(secBuf, 0, BYTES_PER_SECTOR);
    }
  }
  writeMsg(pr, "\r\n");
  
  //==================================================================
  writeMsg(pr, "Write cluster two, bitmap\n");

  // Write cluster two, bitmap.
  sector = partitionOffset + clusterHeapOffset;
  // The 7)/8 converts clusterCount to bytes rounded up to whole bytes.
  bitmapSize = (clusterCount + 7)/8;  
  ns = (bitmapSize + BYTES_PER_SECTOR - 1)/BYTES_PER_SECTOR;
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
  DBGPrintf("sectorsPerCluster: %d, bitmapSize: %d, ns: %d\n", sectorsPerCluster, bitmapSize, ns);
  if (ns > sectorsPerCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  memset(secBuf, 0, BYTES_PER_SECTOR);
  // Allocate clusters for bitmap, upcase, and root.
  secBuf[0] = 0X7;
  for (uint32_t i = 1; i < ns; i++) {
    if (!writeSector(sector + i, secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (i == 0) {
      secBuf[0] = 0;
    }
  }
   
  // Write cluster three, upcase table.
  writeMsg(pr, "Writing upcase table\r\n");
  DBGPrintf("\tWriting Sector: %d\n", clusterHeapOffset + sectorsPerCluster);
  if (!writeUpcase(partitionOffset + clusterHeapOffset + sectorsPerCluster)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (m_upcaseSize > BYTES_PER_SECTOR*sectorsPerCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  
  // Initialize first sector of root.
  writeMsg(pr, "Writing root\r\n");
  ns = sectorsPerCluster;
  sector = partitionOffset + clusterHeapOffset + 2*sectorsPerCluster;
  DBGPrintf("\tWriting 1st Sector of root: %d\n", clusterHeapOffset + 2*sectorsPerCluster);
  memset(secBuf, 0, BYTES_PER_SECTOR);

  // Unused Label entry.
  label = reinterpret_cast<DirLabel_t*>(secBuf);
  label->type = EXFAT_TYPE_LABEL & 0X7F;

  // bitmap directory  entry.
  dbm = reinterpret_cast<DirBitmap_t*>(secBuf + 32);
  dbm->type = EXFAT_TYPE_BITMAP;
  setLe32(dbm->firstCluster, BITMAP_CLUSTER);
  setLe64(dbm->size, bitmapSize);

  // upcase directory entry.
  dup = reinterpret_cast<DirUpcase_t*>(secBuf +64);
  dup->type = EXFAT_TYPE_UPCASE;
  setLe32(dup->checksum, m_upcaseChecksum);
  setLe32(dup->firstCluster, UPCASE_CLUSTER);
  setLe64(dup->size, m_upcaseSize);

  // Write root, cluster four.
  for (uint32_t i = 0; i < ns; i++) {
    if (!writeSector(sector + i, secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (i == 0) {
      memset(secBuf, 0, BYTES_PER_SECTOR);
    }
  }
  writeMsg(pr, "Format done\r\n");

  m_dev->syncDevice();
  return true;
  
 fail:
  writeMsg(pr, "Format failed\r\n");
  return false;

}




//----------------------------------------------------------------------------
uint8_t PFsExFatFormatter::addExFatPartitionToMbr() {
  uint32_t last_start_sector = m_dev->sectorCount();  // get the devices total sector count
  DBGPrintf("\nPFsFatFormatter::addPartitionToMbr: %u %u %u\n", m_part_relativeSectors, m_sectorCount, last_start_sector);

  MbrSector_t* mbr = reinterpret_cast<MbrSector_t*>(m_secBuf);

  if (!m_dev->readSector(0, m_secBuf)) {
    writeMsg(m_pr, "Didn't read MBR Sector !!!\n");
    return 0xff; // did not read the sector.
  }
  dump_hexbytes(&mbr->part[0], 4*sizeof(MbrPart_t));

  MbrPart_t *pt = &mbr->part[0]; 
  // Quick test for GPT disk
  if (pt->type == 0xee) {
    writeMsg(m_pr, "GPT disk - add ExFat Partition not supported yet !!!\n");
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
        writeMsg(m_pr, "Gap not big enough.\r\n");
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


//------------------------------------------------------------------------------
bool PFsExFatFormatter::writeMbr() {
  // make Master Boot Record.  Use fake CHS.
  if (m_mbrLBA == 0xFFFFFFFFUL) {
    DBGPrintf("    writeMBR - GPT entry so dont update\n");
    return true;
  }
  memset(m_secBuf, 0, BYTES_PER_SECTOR);

  // Need to handle EXT wmere MBR may be EXtended Boot record
  MbrSector_t* mbr = reinterpret_cast<MbrSector_t*>(m_secBuf);
  MbrPart_t *pt = &mbr->part[m_mbrPart];
  
  if (!m_dev->readSector(m_mbrLBA, m_secBuf)) writeMsg(m_pr, "DIDN't GET SECTOR BUFFER");

  
  pt->beginCHS[0] = 0x20;
  pt->beginCHS[1] = 0x21;
  pt->beginCHS[2] = 0;
  pt->type = 7;
  pt->endCHS[0] = 0XFE;
  pt->endCHS[1] = 0XFF;
  pt->endCHS[2] = 0XFF;
  setLe32(pt->relativeSectors, partitionOffset - m_mbrLBA);  // should be relative to the start...
  setLe32(pt->totalSectors, volumeLength);
  setLe16(mbr->signature, MBR_SIGNATURE);
  //DBGPrintf("    m_relativeSectors:%u\n", getLe32(pt->relativeSectors));
  //DBGPrintf("    m_totalSectors:%u\n", getLe32(pt->totalSectors));
  
  return writeSector(m_mbrLBA, m_secBuf);

}


//------------------------------------------------------------------------------
bool PFsExFatFormatter::syncUpcase() {
  uint16_t index = m_upcaseSize & SECTOR_MASK;
  if (!index) {
    return true;
  }
  for (size_t i = index; i < BYTES_PER_SECTOR; i++) {
    m_secBuf[i] = 0;
  }
  return writeSector(m_upcaseSector, m_secBuf);
}
//------------------------------------------------------------------------------
bool PFsExFatFormatter::writeUpcaseByte(uint8_t b) {
  uint16_t index = m_upcaseSize & SECTOR_MASK;
  m_secBuf[index] = b;
  m_upcaseChecksum = exFatChecksum(m_upcaseChecksum, b);
  m_upcaseSize++;
  if (index == SECTOR_MASK) {
    return writeSector(m_upcaseSector++, m_secBuf);
  }
  return true;
}
//------------------------------------------------------------------------------
bool PFsExFatFormatter::writeUpcaseUnicode(uint16_t unicode) {
  return writeUpcaseByte(unicode) && writeUpcaseByte(unicode >> 8);
}
//------------------------------------------------------------------------------
bool PFsExFatFormatter::writeUpcase(uint32_t sector) {
  uint32_t n;
  uint32_t ns;
  uint32_t ch = 0;
  uint16_t uc;

  m_upcaseSize = 0;
  m_upcaseChecksum = 0;
  m_upcaseSector = sector;

  while (ch < 0X10000) {
    uc = toUpcase(ch);
    if (uc != ch) {
      if (!writeUpcaseUnicode(uc)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      ch++;
    } else {
      for (n = ch + 1; n < 0X10000 && n == toUpcase(n); n++) {}
      ns = n - ch;
      if (ns >= MINIMUM_UPCASE_SKIP) {
        if (!writeUpcaseUnicode(0XFFFF) || !writeUpcaseUnicode(ns)) {
          DBG_FAIL_MACRO;
          goto fail;
        }
        ch = n;
      } else {
        while (ch < n) {
          if (!writeUpcaseUnicode(ch++)) {
            DBG_FAIL_MACRO;
            goto fail;
          }
        }
      }
    }
  }
  if (!syncUpcase()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

 fail:
  return false;
}

void PFsExFatFormatter::dump_hexbytes(const void *ptr, int len)
{
  if (ptr == NULL || len <= 0) return;
  const uint8_t *p = (const uint8_t *)ptr;
  while (len > 0) {
    for (uint8_t i = 0; i < 32; i++) {
      if (i > len) break;
      m_pr->printf("%02X ", p[i]);
    }
    m_pr->print(":");
    for (uint8_t i = 0; i < 32; i++) {
      if (i > len) break;
      m_pr->printf("%c", ((p[i] >= ' ') && (p[i] <= '~')) ? p[i] : '.');
    }
    m_pr->println();
    p += 32;
    len -= 32;
  }
}
