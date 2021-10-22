#include "PFsLib.h"

//Set to 0 for debug info
#define DBG_Print	0
#if defined(DBG_Print)
#define DBGPrintf Serial.printf
#else
void inline DBGPrintf(...) {};
#endif

//------------------------------------------------------------------------------
#define PRINT_FORMAT_PROGRESS 1
#if !PRINT_FORMAT_PROGRESS
#define writeMsg(str)
#elif defined(__AVR__)
#define writeMsg(str) if (m_pr) m_pr->print(F(str))
#else  // PRINT_FORMAT_PROGRESS
#define writeMsg(str) if (m_pr) m_pr->write((const char*)str)
#endif  // PRINT_FORMAT_PROGRESS

//----------------------------------------------------------------
#define SECTORS_2GB 4194304   // (2^30 * 2) / 512
#define SECTORS_32GB 67108864 // (2^30 * 32) / 512
#define SECTORS_127GB 266338304 // (2^30 * 32) / 512

//uint8_t partVols_drive_index[10];

//=============================================================================
bool PFsLib::deletePartition(BlockDeviceInterface *blockDev, uint8_t part, print_t* pr, Stream &Serialx) 
{
  uint8_t  sectorBuffer[512];
  
  m_pr = pr;

  MbrSector_t* mbr = reinterpret_cast<MbrSector_t*>(sectorBuffer);
  if (!blockDev->readSector(0, sectorBuffer)) {
    writeMsg(F("\nERROR: read MBR failed.\n"));
    return false;
  }

  if ((part < 1) || (part > 4)) {
    m_pr->printf(F("ERROR: Invalid Partition: %u, only 1-4 are valid\n"), part);
    return false;
  }

  writeMsg(F("Warning this will delete the partition are you sure, continue: Y? "));
  int ch;
  
  //..... TODO CIN for READ ......
  while ((ch = Serialx.read()) == -1) ;
  if (ch != 'Y') {
    writeMsg(F("Canceled"));
    return false;
  }
  DBGPrintf(F("MBR Before"));
#if(DBG_Print)
	dump_hexbytes(&mbr->part[0], 4*sizeof(MbrPart_t));
#endif
  // Copy in the higher numer partitions; 
  for (--part; part < 3; part++)  memcpy(&mbr->part[part], &mbr->part[part+1], sizeof(MbrPart_t));
  // clear out the last one
  memset(&mbr->part[part], 0, sizeof(MbrPart_t));

  DBGPrintf(F("MBR After"));
#if(DBG_Print)
  dump_hexbytes(&mbr->part[0], 4*sizeof(MbrPart_t));
#endif
  return blockDev->writeSector(0, sectorBuffer);
}

//===========================================================================
//----------------------------------------------------------------
#define SECTORS_2GB 4194304   // (2^30 * 2) / 512
#define SECTORS_32GB 67108864 // (2^30 * 32) / 512
#define SECTORS_127GB 266338304 // (2^30 * 32) / 512

//uint8_t partVols_drive_index[10];

//----------------------------------------------------------------
// Function to handle one MS Drive...
//msc[drive_index].usbDrive()
void PFsLib::InitializeDrive(BlockDeviceInterface *dev, uint8_t fat_type, print_t* pr)
{
  uint8_t  sectorBuffer[512];

  m_dev = dev;
  m_pr = pr;
  
  //TODO: have to see if this is still valid
  PFsVolume partVol;
/*
  for (int ii = 0; ii < count_partVols; ii++) {
    if (partVols_drive_index[ii] == drive_index) {
      while (Serial.read() != -1) ;
      writeMsg(F("Warning it appears like this drive has valid partitions, continue: Y? "));
      int ch;
      while ((ch = Serial.read()) == -1) ;
      if (ch != 'Y') {
        writeMsg(F("Canceled"));
        return;
      }
      break;
    }
  }

  if (drive_index == LOGICAL_DRIVE_SDIO) {
    dev = sd.card();
  } else if (drive_index == LOGICAL_DRIVE_SDSPI) {
    dev = sdSPI.card();
  } else {
    if (!msDrives[drive_index]) {
      writeMsg(F("Not a valid USB drive"));
      return;
    }
    dev = (USBMSCDevice*)msc[drive_index].usbDrive();
  }
*/
  uint32_t sectorCount = dev->sectorCount();
  
  m_pr->printf(F("sectorCount = %u, FatType: %x\n"), sectorCount, fat_type);
  
  // Serial.printf(F("Blocks: %u Size: %u\n"), msDrives[drive_index].msCapacity.Blocks, msDrives[drive_index].msCapacity.BlockSize);
  if ((fat_type == FAT_TYPE_EXFAT) && (sectorCount < 0X100000 )) fat_type = 0; // hack to handle later
  if ((fat_type == FAT_TYPE_FAT16) && (sectorCount >= SECTORS_2GB )) fat_type = 0; // hack to handle later
  if ((fat_type == FAT_TYPE_FAT32) && (sectorCount >= SECTORS_127GB )) fat_type = 0; // hack to handle later
  if (fat_type == 0)  {
    // assume 512 byte blocks here.. 
    if (sectorCount < SECTORS_2GB) fat_type = FAT_TYPE_FAT16;
    else if (sectorCount < SECTORS_32GB) fat_type = FAT_TYPE_FAT32;
    else fat_type = FAT_TYPE_EXFAT;
  }

  // lets generate a MBR for this type...
  memset(sectorBuffer, 0, 512); // lets clear out the area.
  MbrSector_t* mbr = reinterpret_cast<MbrSector_t*>(sectorBuffer);
  setLe16(mbr->signature, MBR_SIGNATURE);

  // Temporary until exfat is setup...
  if (fat_type == FAT_TYPE_EXFAT) {
    m_pr->println(F("TODO createPartition on ExFat"));
    m_dev->writeSector(0, sectorBuffer);
    createExFatPartition(m_dev, 2048, sectorCount, sectorBuffer, &Serial);
    return;
  } else {
    // Fat16/32
    m_dev->writeSector(0, sectorBuffer);
    createFatPartition(m_dev, fat_type, 2048, sectorCount, sectorBuffer, &Serial);
  }
  
	m_dev->syncDevice();
    writeMsg(F("Format Done\r\n"));
 
}


bool PFsLib::formatter(PFsVolume &partVol, uint8_t fat_type, bool dump_drive, bool g_exfat_dump_changed_sectors, Print &Serialx)
{
  uint8_t  sectorBuffer[512];

  m_pr = &Serialx; // I believe we need this as dump_hexbytes prints to this...

  if (fat_type == 0) fat_type = partVol.fatType();

  if (fat_type != FAT_TYPE_FAT12) {
    // 
    uint8_t buffer[512];
    MbrSector_t *mbr = (MbrSector_t *)buffer;
    if (!partVol.blockDevice()->readSector(0, buffer)) return false;
    MbrPart_t *pt = &mbr->part[partVol.part() - 1];

    uint32_t sector = getLe32(pt->relativeSectors);

    // I am going to read in 24 sectors for EXFat.. 
    uint8_t *bpb_area = (uint8_t*)malloc(512*24); 
    if (!bpb_area) {
      writeMsg(F("Unable to allocate dump memory"));
      return false;
    }
    // Lets just read in the top 24 sectors;
    uint8_t *sector_buffer = bpb_area;
    for (uint32_t i = 0; i < 24; i++) {
      partVol.blockDevice()->readSector(sector+i, sector_buffer);
      sector_buffer += 512;
    }

    if (dump_drive) {
      sector_buffer = bpb_area;
      
      for (uint32_t i = 0; i < 12; i++) {
        DBGPrintf(F("\nSector %u(%u)\n"), i, sector);
        dump_hexbytes(sector_buffer, 512);
        sector++;
        sector_buffer += 512;
      }
      for (uint32_t i = 12; i < 24; i++) {
        DBGPrintf(F("\nSector %u(%u)\n"), i, sector);
        compare_dump_hexbytes(sector_buffer, sector_buffer - (512*12), 512);
        sector++;
        sector_buffer += 512;
      }

    } else {  
      if (fat_type != FAT_TYPE_EXFAT) {
        PFsFatFormatter::format(partVol, fat_type, sectorBuffer, &Serialx);
      } else {
        //DBGPrintf(F("ExFatFormatter - WIP\n"));
        PFsExFatFormatter::format(partVol, sectorBuffer, &Serial);
        if (g_exfat_dump_changed_sectors) {
          // Now lets see what changed
          uint8_t *sector_buffer = bpb_area;
          for (uint32_t i = 0; i < 24; i++) {
            partVol.blockDevice()->readSector(sector, buffer);
            DBGPrintf(F("Sector %u(%u)\n"), i, sector);
            if (memcmp(buffer, sector_buffer, 512)) {
              compare_dump_hexbytes(buffer, sector_buffer, 512);
              DBGPrintf("\n");
            }
            sector++;
            sector_buffer += 512;
          }
        }
      }
    }
    free(bpb_area); 
  }
  else {
    writeMsg(F("Formatting of Fat12 partition not supported"));
    return false;
  }
  return true;
}




//================================================================================================
void PFsLib::print_partion_info(PFsVolume &partVol, Stream &Serialx) 
{
  uint8_t buffer[512];
  MbrSector_t *mbr = (MbrSector_t *)buffer;
  if (!partVol.blockDevice()->readSector(0, buffer)) return;
  MbrPart_t *pt = &mbr->part[partVol.part() - 1];

  uint32_t starting_sector = getLe32(pt->relativeSectors);
  uint32_t sector_count = getLe32(pt->totalSectors);
  Serialx.printf(F("Starting Sector: %u, Sector Count: %u\n"), starting_sector, sector_count);    

  FatPartition *pfp = partVol.getFatVol();
  if (pfp) {
    Serialx.printf(F("fatType:%u\n"), pfp->fatType());
    Serialx.printf(F("bytesPerClusterShift:%u\n"), pfp->bytesPerClusterShift());
    Serialx.printf(F("bytesPerCluster:%u\n"), pfp->bytesPerCluster());
    Serialx.printf(F("bytesPerSector:%u\n"), pfp->bytesPerSector());
    Serialx.printf(F("bytesPerSectorShift:%u\n"), pfp->bytesPerSectorShift());
    Serialx.printf(F("sectorMask:%u\n"), pfp->sectorMask());
    Serialx.printf(F("sectorsPerCluster:%u\n"), pfp->sectorsPerCluster());
    Serialx.printf(F("sectorsPerFat:%u\n"), pfp->sectorsPerFat());
    Serialx.printf(F("clusterCount:%u\n"), pfp->clusterCount());
    Serialx.printf(F("dataStartSector:%u\n"), pfp->dataStartSector());
    Serialx.printf(F("fatStartSector:%u\n"), pfp->fatStartSector());
    Serialx.printf(F("rootDirEntryCount:%u\n"), pfp->rootDirEntryCount());
    Serialx.printf(F("rootDirStart:%u\n"), pfp->rootDirStart());
  }
} 


uint32_t PFsLib::mbrDmp(BlockDeviceInterface *blockDev, uint32_t device_sector_count, Stream &Serialx) {
  MbrSector_t mbr;
  m_pr = &Serialx;
  uint32_t next_free_sector = 8192;  // Some inital value this is default for Win32 on SD...
  // bool valid = true;
  if (!blockDev->readSector(0, (uint8_t*)&mbr)) {
    Serialx.print(F("\nread MBR failed.\n"));
    //errorPrint();
    return (uint32_t)-1;
  }
  Serialx.print(F("\nmsc # Partition Table\n"));
  Serialx.print(F("\tpart,boot,bgnCHS[3],type,endCHS[3],start,length\n"));
  for (uint8_t ip = 1; ip < 5; ip++) {
    MbrPart_t *pt = &mbr.part[ip - 1];
    uint32_t starting_sector = getLe32(pt->relativeSectors);
    uint32_t total_sector = getLe32(pt->totalSectors);
    if (starting_sector > next_free_sector) {
      Serialx.printf(F("\t < unused area starting at: %u length %u >\n"), next_free_sector, starting_sector-next_free_sector);
    }
    switch (pt->type) {
    case 4:
    case 6:
    case 0xe:
      Serialx.print(F("FAT16:\t"));
      break;
    case 11:
    case 12:
      Serialx.print(F("FAT32:\t"));
      break;
    case 7:
      Serialx.print(F("exFAT:\t"));
      break;
    case 0xf:
      Serial.print(F("Extend:\t"));
      break;
    case 0x83: Serialx.print(F("ext2/3/4:\t")); break; 
    case 0xee: Serialx.print(F("*** GPT Disk not supported ***\nGPT guard:\t")); break;
    default:
      Serialx.print(F("pt_#"));
      Serialx.print(pt->type);
      Serialx.print(":\t");
      break;
    }
    Serialx.print( int(ip)); Serial.print( ',');
    Serialx.print(int(pt->boot), HEX); Serial.print( ',');
    for (int i = 0; i < 3; i++ ) {
      Serialx.print("0x"); Serial.print(int(pt->beginCHS[i]), HEX); Serial.print( ',');
    }
    Serialx.print("0x"); Serial.print(int(pt->type), HEX); Serial.print( ',');
    for (int i = 0; i < 3; i++ ) {
      Serialx.print("0x"); Serial.print(int(pt->endCHS[i]), HEX); Serial.print( ',');
    }
    Serialx.print(starting_sector, DEC); Serial.print(',');
    Serialx.println(total_sector);

    // Lets get the max of start+total
    if (starting_sector && total_sector)  next_free_sector = starting_sector + total_sector;
  }
  if ((device_sector_count != (uint32_t)-1) && (next_free_sector < device_sector_count)) {
    Serialx.printf(F("\t < unused area starting at: %u length %u >\n"), next_free_sector, device_sector_count-next_free_sector);
  } 
  return next_free_sector;
}

//----------------------------------------------------------------

void PFsLib::dump_hexbytes(const void *ptr, int len)
{
  if (ptr == NULL || len <= 0) return;
  if (m_pr == nullptr) return;
  const uint8_t *p = (const uint8_t *)ptr;
  while (len) {
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

void PFsLib::compare_dump_hexbytes(const void *ptr, const uint8_t *compare_buf, int len)
{
  if (ptr == NULL || len <= 0) return;
  const uint8_t *p = (const uint8_t *)ptr;
  while (len) {
    for (uint8_t i = 0; i < 32; i++) {
      if (i > len) break;
      m_pr->printf("%c%02X", (p[i]==compare_buf[i])? ' ' : '*',p[i]);
    }
    m_pr->print(":");
    for (uint8_t i = 0; i < 32; i++) {
      if (i > len) break;
      m_pr->printf("%c", ((p[i] >= ' ') && (p[i] <= '~')) ? p[i] : '.');
    }
    m_pr->println();
    p += 32;
    compare_buf += 32;
    len -= 32;
  }
}
