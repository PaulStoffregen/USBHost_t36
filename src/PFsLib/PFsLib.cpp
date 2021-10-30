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
  uint8_t  buffer[512];

  m_pr = &Serialx; // I believe we need this as dump_hexbytes prints to this...
  uint8_t *bpb_area = nullptr;
  uint8_t *sector_buffer;
  uint32_t sector_index = 0; 

  if (fat_type == 0) fat_type = partVol.fatType();

  if (fat_type != FAT_TYPE_FAT12) {
    // only do any of this stuff if we are dumping something!
    if (dump_drive || g_exfat_dump_changed_sectors) {
      MbrSector_t *mbr = (MbrSector_t *)buffer;
      if (!partVol.blockDevice()->readSector(0, buffer)) return false;
      MbrPart_t *pt = &mbr->part[partVol.part() - 1];

      sector_index = getLe32(pt->relativeSectors);

      // I am going to read in 24 sectors for EXFat.. 
      bpb_area = (uint8_t*)malloc(512*24); 
      if (!bpb_area) {
        writeMsg(F("Unable to allocate dump memory"));
        return false;
      }
      // Lets just read in the top 24 sectors;
      sector_buffer = bpb_area;
      for (uint32_t i = 0; i < 24; i++) {
        partVol.blockDevice()->readSector(sector_index+i, sector_buffer);
        sector_buffer += 512;
      }
    }
    if (dump_drive) {
      sector_buffer = bpb_area;
      
      for (uint32_t i = 0; i < 12; i++) {
        DBGPrintf(F("\nSector %u(%u)\n"), i, sector_index);
        dump_hexbytes(sector_buffer, 512);
        sector_index++;
        sector_buffer += 512;
      }
      for (uint32_t i = 12; i < 24; i++) {
        DBGPrintf(F("\nSector %u(%u)\n"), i, sector_index);
        compare_dump_hexbytes(sector_buffer, sector_buffer - (512*12), 512);
        sector_index++;
        sector_buffer += 512;
      }

    } else {  
      if (fat_type != FAT_TYPE_EXFAT) {
        PFsFatFormatter::format(partVol, fat_type, buffer, &Serialx);
      } else {
        //DBGPrintf(F("ExFatFormatter - WIP\n"));
        PFsExFatFormatter::format(partVol, buffer, &Serial);
        if (g_exfat_dump_changed_sectors) {
          // Now lets see what changed
          uint8_t *sector_buffer = bpb_area;
          for (uint32_t i = 0; i < 24; i++) {
            partVol.blockDevice()->readSector(sector_index, buffer);
            DBGPrintf(F("Sector %u(%u)\n"), i, sector_index);
            if (memcmp(buffer, sector_buffer, 512)) {
              compare_dump_hexbytes(buffer, sector_buffer, 512);
              DBGPrintf("\n");
            }
            sector_index++;
            sector_buffer += 512;
          }
        }
      }
    }
    if (bpb_area) free(bpb_area); 
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
  bool gpt_disk = false;
  bool ext_partition;
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
    ext_partition = false;
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
      ext_partition = true;
      break;
    case 0x83: Serialx.print(F("ext2/3/4:\t")); break; 
    case 0xee: 
      Serialx.print(F("*** GPT Disk WIP ***\nGPT guard:\t")); 
      gpt_disk = true;
      break;
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
    if (ext_partition) {
      extgptDmp(blockDev, &mbr, ip, Serialx);
      blockDev->readSector(0, (uint8_t*)&mbr); // maybe need to restore
    }
    // Lets get the max of start+total
    if (starting_sector && total_sector)  next_free_sector = starting_sector + total_sector;
  }
  if ((device_sector_count != (uint32_t)-1) && (next_free_sector < device_sector_count)) {
    Serialx.printf(F("\t < unused area starting at: %u length %u >\n"), next_free_sector, device_sector_count-next_free_sector);
  } 
  if (gpt_disk) gptDmp(blockDev, Serialx);
  return next_free_sector;
}

void PFsLib::extgptDmp(BlockDeviceInterface *blockDev, MbrSector_t *mbr, uint8_t ipExt, Stream &Serialx) {

  // Extract the data from EX partition block...
  MbrPart_t *pt = &mbr->part[ipExt - 1];
  uint32_t ext_starting_sector = getLe32(pt->relativeSectors);
  //uint32_t ext_total_sector = getLe32(pt->totalSectors);
  uint32_t next_mbr = ext_starting_sector;
  uint8_t ext_index = 0;

  while (next_mbr) {
    ext_index++;
    if (!blockDev->readSector(next_mbr, (uint8_t*)mbr)) break;
    pt = &mbr->part[0];
    dump_hexbytes((uint8_t*)pt, sizeof(MbrPart_t)*2);
    uint32_t starting_sector = getLe32(pt->relativeSectors);
    uint32_t total_sector = getLe32(pt->totalSectors);
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
    default:
      Serialx.print(F("pt_#"));
      Serialx.print(pt->type);
      Serialx.print(":\t");
      break;
    }
    Serialx.print( int(ipExt)); Serialx.print(":"); Serialx.print(ext_index); Serialx.print( ',');
    Serialx.print(int(pt->boot), HEX); Serialx.print( ',');
    for (int i = 0; i < 3; i++ ) {
      Serialx.print("0x"); Serialx.print(int(pt->beginCHS[i]), HEX); Serialx.print( ',');
    }
    Serialx.print("0x"); Serialx.print(int(pt->type), HEX); Serialx.print( ',');
    for (int i = 0; i < 3; i++ ) {
      Serialx.print("0x"); Serialx.print(int(pt->endCHS[i]), HEX); Serialx.print( ',');
    }
    Serialx.printf("%u(%u),", next_mbr + starting_sector, starting_sector);
    //Serialx.print(ext_starting_sector + starting_sector, DEC); Serialx.print(',');
    Serialx.print(total_sector);

    // Now lets see what is in the 2nd one...
    pt = &mbr->part[1];
    Serialx.printf(" (%x)\n", pt->type);
    starting_sector = getLe32(pt->relativeSectors);
    if (pt->type && starting_sector) next_mbr = /*starting_sector*/ next_mbr + ext_starting_sector;
    else next_mbr = 0;
  }
}

#if 0
typedef struct {
  uint8_t  signature[8];
  uint8_t  revision[4];
  uint8_t  headerSize[4];
  uint8_t  crc32[4];
  uint8_t  reserved[4];
  uint8_t  currentLBA[8];
  uint8_t  backupLBA[8];
  uint8_t  firstLBA[8];
  uint8_t  lastLBA[8];
  uint8_t  diskGUID[16];
  uint8_t  startLBAArray[8];
  uint8_t  numberPartitions[4];
  uint8_t  sizePartitionEntry[4];
  uint8_t  crc32PartitionEntries[4];
  uint8_t  unused[420]; // should be 0;
} GPTPartitionHeader_t;

typedef struct {
  uint8_t  partitionTypeGUID[16];
  uint8_t  uniqueGUID[16];
  uint8_t  firstLBA[8];
  uint8_t  lastLBA[8];
  uint8_t  attributeFlags[8];
  uint16_t name[36];
} GPTPartitionEntryItem_t;

typedef struct {
  GPTPartitionEntryItem_t items[4];
} GPTPartitionEntrySector_t;

#endif

typedef struct {
  uint32_t  q1;
  uint16_t  w2; 
  uint16_t  w3;
  uint8_t   b[8];
} guid_t;


void printGUID(uint8_t* pbguid, Print *pserial) {
  // Windows basic partion guid is: EBD0A0A2-B9E5-4433-87C0-68B6B72699C7
  // raw dump of it: A2 A0 D0 EB E5 B9 33 44 87 C0 68 B6 B7 26 99 C7
  guid_t *pg = (guid_t*)pbguid;
  pserial->printf("%08X-%04X-%04X-%02X%02X-", pg->q1, pg->w2, pg->w3, pg->b[0], pg->b[1]);
  for (uint8_t i=2;i<8; i++) pserial->printf("%02X", pg->b[i]);
}

static const uint8_t mbdpGuid[16] PROGMEM = {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};

//----------------------------------------------------------------
uint32_t PFsLib::gptDmp(BlockDeviceInterface *blockDev, Stream &Serialx) {
  union {
    MbrSector_t mbr;
    partitionBootSector pbs;
    GPTPartitionHeader_t gpthdr;
    GPTPartitionEntrySector_t gptes;
    uint8_t buffer[512];
  } sector; 
  m_pr = &Serialx;

  // Lets verify that we are an GPT...
  if (!blockDev->readSector(0, (uint8_t*)&sector.mbr)) {
    Serialx.print(F("\nread MBR failed.\n"));
    //errorPrint();
    return (uint32_t)-1;
  }
  // verify that the first partition is the guard...
  MbrPart_t *pt = &sector.mbr.part[0];
  if (pt->type != 0xee) {
    Serialx.print(F("\nMBR is not an gpt guard\n"));
    return (uint32_t)-1;
  }

  if (!blockDev->readSector(1, (uint8_t*)&sector.buffer)) {
    Serialx.print(F("\nread Partition Table Header failed.\n"));
    return (uint32_t)-1;
  }
  // Do quick test for signature:
  if (memcmp(sector.gpthdr.signature, "EFI PART", 8)!= 0) { 
    Serialx.println("GPT partition header signature did not match");
    dump_hexbytes(&sector.buffer, 512);
  }
  Serialx.printf("\nGPT partition header revision: %x\n", getLe32(sector.gpthdr.revision));
  Serialx.printf("LBAs current:%llu backup:%llu first:%llu last:%llu\nDisk GUID:", 
    getLe64(sector.gpthdr.currentLBA), getLe64(sector.gpthdr.backupLBA), 
    getLe64(sector.gpthdr.firstLBA), getLe64(sector.gpthdr.lastLBA));
  printGUID(sector.gpthdr.diskGUID, &Serialx);

  //dump_hexbytes(&sector.gpthdr.diskGUID, 16);
  uint32_t cParts = getLe32(sector.gpthdr.numberPartitions);
  Serialx.printf("Start LBA Array: %llu Count: %u size:%u\n", 
      getLe64(sector.gpthdr.startLBAArray), cParts, getLe32(sector.gpthdr.sizePartitionEntry));
  uint32_t sector_number = 2;
  Serialx.println("Part\t Type Guid, Unique Guid, First, last, attr, name");
  for (uint8_t part = 0; part < cParts ; part +=4) {
    if (blockDev->readSector(sector_number, (uint8_t*)&sector.buffer)) {
      //dump_hexbytes(&sector.buffer, 512);
      for (uint8_t ipei = 0; ipei < 4; ipei++) {
        GPTPartitionEntryItem_t *pei = &sector.gptes.items[ipei];
        // see if the entry has any data in it...
        uint32_t end_addr = (uint32_t)pei + sizeof(GPTPartitionEntryItem_t);
        uint32_t *p = (uint32_t*)pei;
        for (; (uint32_t)p < end_addr; p++) {
          if (*p) break; // found none-zero. 
        }
        if ((uint32_t)p < end_addr) {
          // So entry has data:
          Serialx.printf("%u\t", part + ipei);
          printGUID(pei->partitionTypeGUID, &Serialx);
          Serialx.print(", ");
          printGUID(pei->uniqueGUID, &Serialx);
          Serialx.printf(", %llu, %llu, %llX, ", getLe64(pei->firstLBA), getLe64(pei->lastLBA), 
              getLe64(pei->attributeFlags));
          for (uint8_t i = 0; i < 36; i++) {
            if ((pei->name[i]) == 0) break;
            Serialx.write((uint8_t)pei->name[i]);
          }
          Serialx.println();
          if (memcmp((uint8_t *)pei->partitionTypeGUID, mbdpGuid, 16) == 0) {
            Serialx.print(">>> Microsoft Basic Data Partition\n");
            // See if we can read in the first sector
            if (blockDev->readSector(getLe64(pei->firstLBA), (uint8_t*)&sector.buffer)) {
              //dump_hexbytes(sector.buffer, 512);

              // First see if this is exFat... 
              // which starts with: 
              static const uint8_t exfatPBS[] PROGMEM = {0xEB, 0x76, 0x90, //Jmp instruction
                   'E', 'X', 'F', 'A', 'T', ' ', ' ', ' '};
              if (memcmp(sector.buffer, exfatPBS, 11) == 0) {
                Serial.println("    EXFAT:");
              }

            }
            // Bugbug reread that sector...
            blockDev->readSector(sector_number, (uint8_t*)&sector.buffer);
          }
        }
      }
    }
    sector_number++;
  }
  return 0;
}
//----------------------------------------------------------------

void PFsLib::dump_hexbytes(const void *ptr, int len)
{
  if (ptr == NULL || len <= 0) return;
  if (m_pr == nullptr) return;
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

//================================================================================================
//typedef enum {INVALID_VOL=0, MBR_VOL, EXT_VOL, GPT_VOL} voltype_t; // what type of volume did the mapping return
PFsLib::voltype_t PFsLib::getPartitionInfo(BlockDeviceInterface *blockDev, uint8_t part, Print* pserial, uint8_t *secBuf,
    uint32_t &firstLBA, uint32_t &sectorCount, uint32_t &mbrLBA, uint8_t &mbrPart) {

  //Serial.printf("PFsLib::getPartitionInfo(%x, %u)\n", (uint32_t)blockDev, part);
  MbrSector_t *mbr;
  MbrPart_t *mp;

  if (!part) return INVALID_VOL; // won't handle this here.
  part--; // zero base it.

  if (!blockDev->readSector(0, secBuf)) return INVALID_VOL;
  mbr = reinterpret_cast<MbrSector_t*>(secBuf);

  // First check for GPT vs MBR
  mp = &mbr->part[0];
  if (mp->type == 0xee) {
    // This is a GPT initialized Disk assume validation done earlier.
    //if (!m_dev->readSector(1, secBuf)) return INVALID_VOL; 
    //GPTPartitionHeader_t* gptph = reinterpret_cast<GPTPartitionHeader_t*>(secBuf);
    // We will overload the mbr part to give clue where GPT data is stored for this volume
    mbrLBA = 2 + (part >> 2);
    mbrPart = part & 0x3;
    if (!blockDev->readSector(mbrLBA, secBuf)) return INVALID_VOL; 
    GPTPartitionEntrySector_t *gptes = reinterpret_cast<GPTPartitionEntrySector_t*>(secBuf);
    GPTPartitionEntryItem_t *gptei = &gptes->items[mbrPart];

    // Mow extract the data...
    firstLBA = getLe64(gptei->firstLBA);
    sectorCount = 1 + getLe64(gptei->lastLBA) - getLe64(gptei->firstLBA);
    if ((firstLBA == 0) && (sectorCount == 1)) return INVALID_VOL;
    
    if (memcmp((uint8_t *)gptei->partitionTypeGUID, mbdpGuid, 16) != 0) return OTHER_VOL;

    return GPT_VOL; 
  }
  // So we are now looking a MBR type setups. 
  // Extended support we need to walk through the partitions to see if there is an extended partition
  // that we need to walk into. 
  // short cut:
  if (part < 4) {
    // try quick way through
    mp = &mbr->part[part];
    if (((mp->boot == 0) || (mp->boot == 0X80)) && (mp->type != 0) && (mp->type != 0xf)) {
      firstLBA = getLe32(mp->relativeSectors);
      sectorCount = getLe32(mp->totalSectors);
      mbrLBA = 0;
      mbrPart = part; // zero based. 
      return MBR_VOL;
    }
  }  

  // So must be extended or invalid.
  uint8_t index_part;
  for (index_part = 0; index_part < 4; index_part++) {
    mp = &mbr->part[index_part];
    if ((mp->boot != 0 && mp->boot != 0X80) || mp->type == 0 || index_part > part) return INVALID_VOL;
    if (mp->type == 0xf) break;
  }

  if (index_part == 4) return INVALID_VOL; // no extended partition found. 

  // Our partition if it exists is in extended partition. 
  uint32_t next_mbr = getLe32(mp->relativeSectors);
  for(;;) {
    if (!blockDev->readSector(next_mbr, secBuf)) return INVALID_VOL;
    mbr = reinterpret_cast<MbrSector_t*>(secBuf);

    if (index_part == part) break; // should be at that entry
    // else we need to see if it points to others...
    mp = &mbr->part[1];
    uint32_t  relSec = getLe32(mp->relativeSectors);
    //Serial.printf("    Check for next: type: %u start:%u\n ", mp->type, volumeStartSector);
    if ((mp->type == 5) && relSec) {
      next_mbr = next_mbr + relSec;
      index_part++; 
    } else return INVALID_VOL;
  }
 
  // If we are here than we should hopefully be at start of segment...
  mp = &mbr->part[0];
  firstLBA = getLe32(mp->relativeSectors) + next_mbr;
  sectorCount = getLe32(mp->totalSectors);
  mbrLBA = next_mbr;
  mbrPart = 0; // zero based
  return EXT_VOL;
}

void PFsLib::listPartitions(BlockDeviceInterface *blockDev, Print &Serialx) {
  // simply enumerate through partitions until one fails.
  PFsLib::voltype_t vt;
  uint32_t firstLBA;
  uint32_t sectorCount;
  uint32_t mbrLBA;
  uint8_t  mbrPart;
  uint8_t secBuf[512];

  Serialx.println("\nPART\tType\tStart\tCount\t(MBR\tPart)\tVolume Type");
  uint32_t part = 1;
  while ((vt = getPartitionInfo(blockDev, part, &Serialx, secBuf, firstLBA, sectorCount, mbrLBA, mbrPart)) != PFsLib::INVALID_VOL) {
    Serial.printf("%u\t", part);
    switch(vt) {
      case PFsLib::MBR_VOL: Serialx.write('M'); break;
      case PFsLib::EXT_VOL: Serialx.write('E'); break;
      case PFsLib::GPT_VOL: Serialx.write('G'); break;
      case PFsLib::OTHER_VOL: Serialx.write('O'); break;
      default: Serialx.write('?'); break;
    }
    Serialx.printf("\t%u\t%u\t%u\t%u", firstLBA, sectorCount, mbrLBA, mbrPart);

    // Lets see if we can guess what FS this might be:
    if (vt != PFsLib::OTHER_VOL) {
      if (blockDev->readSector(firstLBA, secBuf)) {
        //Serialx.println();
        //dump_hexbytes(secBuf, 512);
        static const uint8_t exfatPBS[] PROGMEM = 
            {0xEB, 0x76, 0x90, //Jmp instruction
             'E', 'X', 'F', 'A', 'T', ' ', ' ', ' '};
        if (memcmp(secBuf, exfatPBS, 11) == 0) {
          Serialx.print("\texFAT");
        } else {
          pbs_t* pbs = reinterpret_cast<pbs_t*> (secBuf);
          BpbFat32_t* bpb = reinterpret_cast<BpbFat32_t*>(pbs->bpb);
          // hacks for now probably should have more validation
          if (getLe16(bpb->bytesPerSector) == 512) {
            if (getLe16(bpb->sectorsPerFat16)) Serialx.print("\tFat16:");
            else if (getLe32(bpb->sectorsPerFat32)) Serialx.print("\tFat32:");
          }
        }
      }
    } else {
      if ((mbrPart < 4) && blockDev->readSector(mbrLBA, secBuf)) {
        GPTPartitionEntrySector_t* pgpes = reinterpret_cast<GPTPartitionEntrySector_t*> (secBuf);
        // try to print out guid...
        Serialx.write('\t');
        printGUID(pgpes->items[mbrPart].partitionTypeGUID, &Serialx);
      }
    }
    Serialx.println();
    part++;
  }
}

