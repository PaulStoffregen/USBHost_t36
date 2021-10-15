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
PFsVolume* PFsVolume::m_cwv = nullptr;
//------------------------------------------------------------------------------
bool PFsVolume::begin(USBMSCDevice* dev, bool setCwv, uint8_t part) {
  m_usmsci = dev;
  m_blockDev = dev;  
  //Serial.printf("PFsVolume::begin USBmscInterface(%x, %u)\n", (uint32_t)dev, part);  
  return begin((BlockDevice*)dev, setCwv, part);
}

bool PFsVolume::begin(BlockDevice* blockDev, bool setCwv, uint8_t part) {
  //Serial.printf("PFsVolume::begin(%x, %u)\n", (uint32_t)blockDev, part);
  if ((m_blockDev != blockDev) && (m_blockDev != nullptr)) m_usmsci = nullptr; // 
  m_blockDev = blockDev;
  m_part = part;
  m_fVol = nullptr;
  m_xVol = new (m_volMem) ExFatVolume;
  if (m_xVol && m_xVol->begin(m_blockDev, setCwv, part)) {
    goto done;
  }
  m_xVol = nullptr;
  m_fVol = new (m_volMem) FatVolume;
  if (m_fVol && m_fVol->begin(m_blockDev, setCwv, part)) {
    goto done;
  }
  m_cwv = nullptr;
  m_fVol = nullptr;
  return false;

 done:
  m_cwv = this;
  return true;
}
//------------------------------------------------------------------------------
bool PFsVolume::ls(print_t* pr, const char* path, uint8_t flags) {
  PFsBaseFile dir;
  return dir.open(this, path, O_RDONLY) && dir.ls(pr, flags);
}
//------------------------------------------------------------------------------
PFsFile PFsVolume::open(const char *path, oflag_t oflag) {
  PFsFile tmpFile;
  tmpFile.open(this, path, oflag);
  return tmpFile;
}

extern void dump_hexbytes(const void *ptr, int len);

bool PFsVolume::getVolumeLabel(char *volume_label, size_t cb) 
{
  uint8_t buf[512];
  if (!volume_label || (cb < 12)) return false; // don't want to deal with it
  *volume_label = 0; // make sure if we fail later we return empty string as well.
  uint8_t fat_type = fatType();
  uint32_t root_dir;
  // Lets go hard core here.
  if (m_fVol)
  {
    FatFile rootFat;
    if (!rootFat.openRoot(m_fVol)) return false;
    uint32_t root_dir_size = rootFat.dirSize(); // how big is this directory...
    rootFat.close();

    if (fat_type == FAT_TYPE_FAT32) {
      root_dir = m_fVol->dataStartSector();
    } else {
      root_dir = m_fVol->rootDirStart();
    }
    //Serial.printf("\n$$$ PFsVolume::getVolumeLabel(%u): %u %u\n", fat_type, root_dir, root_dir_size);
    uint16_t index_in_sector=0;
    m_blockDev->readSector(root_dir, buf);
    while (root_dir_size) {
      DirFat_t *dir;
      dir = reinterpret_cast<DirFat_t*>(&buf[index_in_sector]);
      if (dir->name[0] == FAT_NAME_FREE) break;  // at end of list...
      if (dir->attributes == 0x08) {
        size_t i;
        for (i = 0; i < 11; i++) {
          volume_label[i]  = dir->name[i];
          }
        while ((i > 0) && (volume_label[i - 1] == ' ')) i--; // trim off trailing blanks
        volume_label[i] = 0;
        return true;
      }
      index_in_sector += 32;  // increment to next entry...
      root_dir_size-=32;
      if (index_in_sector >= 512 && root_dir_size) {
        root_dir++;
        m_blockDev->readSector(root_dir, buf);
        index_in_sector = 0;
      }
    }
  } else if (m_xVol) {

    uint32_t chs = m_xVol->clusterHeapStartSector();
    uint32_t rdc = m_xVol->rootDirectoryCluster();
    uint32_t root_dir_size = m_xVol->rootLength();
    uint32_t spc = m_xVol->sectorsPerCluster();
    //Serial.printf("\n$$$ PFsVolume::getVolumeLabel(Ex): %u %x %x %u\n", root_dir_size, chs, rdc, spc);
    uint32_t root_dir = chs + (rdc-2)*spc;
    //Serial.printf("  $$$ Guess sector: %x\n", root_dir);

    uint16_t index_in_sector=0;
    m_blockDev->readSector(root_dir, buf);
    while (root_dir_size) {
      DirLabel_t *dir;
      dir = reinterpret_cast<DirLabel_t*>(&buf[index_in_sector]);
      //if (dir->name[0] == 0) break;  // at end of list...
      if (dir->type == EXFAT_TYPE_LABEL) {
        size_t i;
        for (i = 0; i < dir->labelLength; i++) {
          volume_label[i] = dir->unicode[2 * i];
        }
        volume_label[i] = 0;
        return true;
      } else if (dir->type == 0) break; // I believe this marks the end...

      index_in_sector += 32;  // increment to next entry...
      root_dir_size-=32;
      if (index_in_sector >= 512 && root_dir_size) {
        root_dir++;
        m_blockDev->readSector(root_dir, buf);
        index_in_sector = 0;
      }
    }
  }
  return false; // no volume label was found

}

bool PFsVolume::setVolumeLabel(char *volume_label) 
{
  uint8_t buf[512];
  uint8_t fat_type = fatType();
  uint32_t root_dir;
  bool label_found = false;

  // Lets go hard core here.
  if (m_fVol)
  {
    FatFile rootFat;
    DirFat_t *dir = nullptr;
    if (!rootFat.openRoot(m_fVol)) return false;
    uint32_t root_dir_size = rootFat.dirSize(); // how big is this directory...
    rootFat.close();

    if (fat_type == FAT_TYPE_FAT32) {
      root_dir = m_fVol->dataStartSector();
    } else {
      root_dir = m_fVol->rootDirStart();
    }
    //Serial.printf("\n$$$ PFsVolume::setVolumeLabel(%u): %u %u\n", fat_type, root_dir, root_dir_size);
    uint16_t index_in_sector=0;
    uint32_t first_deleted_entry_sector = 0;
    uint16_t first_deleted_entry_index = 0;

    m_blockDev->readSector(root_dir, buf);
    //dump_hexbytes(buf, 512);
    while (root_dir_size) {
      dir = reinterpret_cast<DirFat_t*>(&buf[index_in_sector]);
      if (dir->name[0] == FAT_NAME_DELETED) {
        if (!first_deleted_entry_sector) {
          first_deleted_entry_sector = root_dir;
          first_deleted_entry_index = index_in_sector;
        }
      }
      else if (dir->name[0] == FAT_NAME_FREE) break;  // at end of list...
      else if (dir->attributes == 0x08) {
        label_found = true;
        break;
      }
      index_in_sector += 32;  // increment to next entry...
      root_dir_size-=32;
      if (index_in_sector >= 512 && root_dir_size) {
        root_dir++;
        m_blockDev->readSector(root_dir, buf);
        //Serial.printf(">> %x\n", root_dir);
        //dump_hexbytes(buf, 512);
        index_in_sector = 0;
      }
    }
    // Lets see if we found something...
    if (!volume_label || !*volume_label) {
      if (label_found) {
        Serial.printf("Found volume label - deleted\n");
        dir->name[0] = FAT_NAME_DELETED;  // mark item as deleted
        dir->attributes = 0; 
        m_blockDev->writeSector(root_dir, buf);
        m_blockDev->syncDevice();
      }
      return true;
    }
    // Lets see where we should write...
    if (!label_found) {
      if (first_deleted_entry_sector) {
        if (first_deleted_entry_sector != root_dir) {
          root_dir = first_deleted_entry_sector;
          m_blockDev->readSector(root_dir, buf);
        }
        index_in_sector = first_deleted_entry_index;
        dir = reinterpret_cast<DirFat_t*>(&buf[index_in_sector]);
        label_found = true;
      }
      else if (dir->name[0] == FAT_NAME_FREE) label_found = true;
    }
    if (label_found) {  // or found a spot for it.
      memset((void*)dir, 0, 32);  // clear it out.
      if (FsDateTime::callback) {
        uint16_t cur_date;
        uint16_t cur_time;
        uint8_t cur_ms10;
        FsDateTime::callback(&cur_date, &cur_time, &cur_ms10);
        setLe16(dir->modifyTime, cur_time);
        setLe16(dir->modifyDate, cur_date);
      }
      for (size_t i = 0; i < 11; i++) {
        dir->name[i] = *volume_label? *volume_label++ : ' '; // fill in the 11 spots trailing blanks 
      }
      dir->attributes = 8;  // mark as a volume label.
      m_blockDev->writeSector(root_dir, buf);
      m_blockDev->syncDevice();
          return true;
    }

  } else if (m_xVol) {
    DirLabel_t *dir = nullptr;
    uint32_t chs = m_xVol->clusterHeapStartSector();
    uint32_t rdc = m_xVol->rootDirectoryCluster();
    uint32_t root_dir_size = m_xVol->rootLength();
    uint32_t spc = m_xVol->sectorsPerCluster();
    //Serial.printf("\n$$$ PFsVolume::setVolumeLabel(Ex): %u %x %x %u\n", root_dir_size, chs, rdc, spc);
    uint32_t root_dir = chs + (rdc-2)*spc;
    //Serial.printf("  $$$ Guess sector: %x\n", root_dir);

    uint16_t index_in_sector=0;
    m_blockDev->readSector(root_dir, buf);
    //m_xVol->cacheSafeRead(root_dir, buf);
    //dump_hexbytes(buf, 512);
    while (root_dir_size) {
      dir = reinterpret_cast<DirLabel_t*>(&buf[index_in_sector]);
      //if (dir->name[0] == 0) break;  // at end of list...
      if (dir->type == EXFAT_TYPE_LABEL) {
        label_found = true;
        break;
      } else if (dir->type == 0) break;
      index_in_sector += 32;  // increment to next entry...
      root_dir_size-=32;
      if (index_in_sector >= 512 && root_dir_size) {
        root_dir++;
        m_blockDev->readSector(root_dir, buf);
        //m_xVol->cacheSafeRead(root_dir, buf);
        index_in_sector = 0;
        //Serial.println("---");
        //dump_hexbytes(buf, 512);
      }
    }
    // Lets see if we found something...
    if (!volume_label || !*volume_label) {
      if (label_found) {
        Serial.printf("Found volume label - deleted\n");
        dir->type &= 0x7f;  // mark item as deleted
        m_blockDev->writeSector(root_dir, buf);
        //m_xVol->cacheSafeWrite(root_dir, buf);
        m_xVol->cacheClear();
        m_blockDev->syncDevice();
      }
      return true;
    }
    // Lets see where we should write...
    // 
    if (label_found || (dir->type == 0)) {  // or found a spot for it.
      uint8_t cb = strlen(volume_label);
      if (cb > 11) cb = 11; // truncate off. 
      dir->type = EXFAT_TYPE_LABEL; 
      dir->labelLength = cb;
      uint8_t *puni = dir->unicode;
      while (cb--) {
        *puni = *volume_label++;
        puni += 2;
      }
      //m_xVol->cacheSafeWrite(root_dir, buf);
      m_blockDev->writeSector(root_dir, buf);
      m_xVol->cacheClear();
      m_blockDev->syncDevice();
      return true;
    }
  }

  return false; // no volume label was found
}


typedef struct {
  uint32_t free;
  //uint32_t not_free;
  uint32_t todo;
//  uint32_t clusters_per_sector;
//  uint32_t sectors_left_in_call;
} _gfcc_t;


static void _getfreeclustercountCB_fat16(uint32_t token, uint8_t *buffer) 
{
  //digitalWriteFast(1, HIGH);
//  Serial.print("&");
  _gfcc_t *gfcc = (_gfcc_t *)token;
  uint16_t cnt = 512/2;
  if (cnt > gfcc->todo) cnt = gfcc->todo;
  gfcc->todo -= cnt; // update count here...
  //gfcc->sectors_left_in_call--;
  // fat16
  uint16_t *fat16 = (uint16_t *)buffer;
  while (cnt-- ) {
    if (*fat16++ == 0) gfcc->free++;
  }

  //digitalWriteFast(1, LOW);
}

static void _getfreeclustercountCB_fat32(uint32_t token, uint8_t *buffer) 
{
  //digitalWriteFast(1, HIGH);
//  Serial.print("&");
  _gfcc_t *gfcc = (_gfcc_t *)token;
  uint16_t cnt = 512/4;
  if (cnt > gfcc->todo) cnt = gfcc->todo;
  gfcc->todo -= cnt; // update count here...
  uint32_t *fat32 = (uint32_t *)buffer;
  while (cnt-- ) {
    if (*fat32++ == 0) gfcc->free++;
    //else gfcc->not_free++;
  }
}


//-------------------------------------------------------------------------------------------------
uint32_t PFsVolume::freeClusterCount()  {
  // For XVolume lets let the original code do it.
//  Serial.println("PFsVolume::freeClusterCount() called");
  void (*callback)(uint32_t, uint8_t *);
  if (m_xVol) return m_xVol->freeClusterCount();

  if (!m_fVol) return 0;

  if (!m_usmsci) return m_fVol->freeClusterCount();

  // So roll our own here for Fat16/32...
  _gfcc_t gfcc; 
  gfcc.free = 0;
  //gfcc.not_free = 0;

  switch (m_fVol->fatType()) {
    default: return 0;
    case FAT_TYPE_FAT16: callback = &_getfreeclustercountCB_fat16; break;
    case FAT_TYPE_FAT32: callback = &_getfreeclustercountCB_fat32; break;
  }
  gfcc.todo = m_fVol->clusterCount() + 2;

//  digitalWriteFast(0, HIGH);
//  Serial.println("    Using readSectorswithCB");
  #define CNT_FATSECTORS_PER_CALL 1024
  uint32_t first_sector = m_fVol->fatStartSector();
  uint32_t sectors_left = m_fVol->sectorsPerFat();
  bool succeeded = true;

  while (sectors_left) {
    uint32_t sectors_to_read = (sectors_left < CNT_FATSECTORS_PER_CALL)? sectors_left : CNT_FATSECTORS_PER_CALL;

    succeeded = m_usmsci->readSectorsWithCB(first_sector, sectors_to_read, callback, (uint32_t)&gfcc);
    if (!succeeded) break;
    sectors_left -= sectors_to_read;
    first_sector += sectors_to_read;
  }

//  digitalWriteFast(0, LOW);
  if(!succeeded) gfcc.free = (uint32_t)-1;
  //Serial.printf("    status: %u free cluster: %x not free:%x\n", succeeded, gfcc.free, gfcc.not_free);
  return gfcc.free;
}

uint32_t PFsVolume::getFSInfoSectorFreeClusterCount() {
  uint8_t sector_buffer[512];
  if (fatType() != FAT_TYPE_FAT32) return (uint32_t)-1;

  // We could probably avoid this read if our class remembered the starting sector number for the partition...
  if (!m_blockDev->readSector(0, sector_buffer)) return (uint32_t)-1;
  MbrSector_t *mbr = reinterpret_cast<MbrSector_t*>(sector_buffer);
  MbrPart_t *pt = &mbr->part[m_part - 1];
  BpbFat32_t* bpb;
  if ((pt->type != 11) && (pt->type != 12))  return (uint32_t)-1;

  uint32_t volumeStartSector = getLe32(pt->relativeSectors);
  if (!m_blockDev->readSector(volumeStartSector, sector_buffer)) return (uint32_t)-1;
  pbs_t *pbs = reinterpret_cast<pbs_t*> (sector_buffer);
  bpb = reinterpret_cast<BpbFat32_t*>(pbs->bpb);
  
  //Serial.println("\nReadFat32InfoSectorFree BpbFat32_t sector");
  //dump_hexbytes(sector_buffer, 512);
  uint16_t infoSector = getLe16(bpb->fat32FSInfoSector); 

  // I am assuming this sector is based off of the volumeStartSector... So try reading from there.
  //Serial.printf("Try to read Info sector (%u)\n", infoSector); Serial.flush(); 
  if (!m_blockDev->readSector(volumeStartSector+infoSector, sector_buffer)) return (uint32_t)-1;
  //dump_hexbytes(sector_buffer, 512);
  FsInfo_t *pfsi = reinterpret_cast<FsInfo_t*>(sector_buffer);

  // check signatures:
  if (getLe32(pfsi->leadSignature) !=  FSINFO_LEAD_SIGNATURE) Serial.println("Lead Sig wrong");
  if (getLe32(pfsi->structSignature) !=  FSINFO_STRUCT_SIGNATURE) Serial.println("struct Sig wrong");    
  if (getLe32(pfsi->trailSignature) !=  FSINFO_TRAIL_SIGNATURE) Serial.println("Trail Sig wrong");    
  uint32_t free_count = getLe32(pfsi->freeCount);
  return free_count;


}

bool PFsVolume::setUpdateFSInfoSectorFreeClusterCount(uint32_t free_count) {
  uint8_t sector_buffer[512];
  if (fatType() != FAT_TYPE_FAT32) return (uint32_t)false;

  if (free_count == (uint32_t)-1) free_count = freeClusterCount();

  // We could probably avoid this read if our class remembered the starting sector number for the partition...
  if (!m_blockDev->readSector(0, sector_buffer)) return (uint32_t)-1;
  MbrSector_t *mbr = reinterpret_cast<MbrSector_t*>(sector_buffer);
  MbrPart_t *pt = &mbr->part[m_part - 1];
  BpbFat32_t* bpb;
  if ((pt->type != 11) && (pt->type != 12))  return (uint32_t)-1;

  uint32_t volumeStartSector = getLe32(pt->relativeSectors);
  if (!m_blockDev->readSector(volumeStartSector, sector_buffer)) return (uint32_t)-1;
  pbs_t *pbs = reinterpret_cast<pbs_t*> (sector_buffer);
  bpb = reinterpret_cast<BpbFat32_t*>(pbs->bpb);
  
  //Serial.println("\nReadFat32InfoSectorFree BpbFat32_t sector");
  //dump_hexbytes(sector_buffer, 512);
  uint16_t infoSector = getLe16(bpb->fat32FSInfoSector); 

  // OK we now need to fill in the the sector with the appropriate information...
  // Not sure if we should read it first or just blast out new data...
  FsInfo_t *pfsi = reinterpret_cast<FsInfo_t*>(sector_buffer);
  memset(sector_buffer, 0 , 512);

  // write FSINFO sector and backup
  setLe32(pfsi->leadSignature, FSINFO_LEAD_SIGNATURE);
  setLe32(pfsi->structSignature, FSINFO_STRUCT_SIGNATURE);
  setLe32(pfsi->freeCount, free_count);
  setLe32(pfsi->nextFree, 0XFFFFFFFF);
  setLe32(pfsi->trailSignature, FSINFO_TRAIL_SIGNATURE);
  if (!m_blockDev->writeSector(volumeStartSector+infoSector, sector_buffer)) return (uint32_t) false;
  return true;
}



#if ENABLE_ARDUINO_STRING
//------------------------------------------------------------------------------
PFsFile PFsVolume::open(const String &path, oflag_t oflag) {
  return open(path.c_str(), oflag );
}
#endif  // ENABLE_ARDUINO_STRING
