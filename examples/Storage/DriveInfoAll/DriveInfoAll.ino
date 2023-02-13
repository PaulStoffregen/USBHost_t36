#include <USBHost_t36.h>

// Setup USBHost_t36 and as many HUB ports as needed.
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);

// Instances for the number of USB drives you are using.
USBDrive myDrive1(myusb);
USBDrive myDrive2(myusb);
USBDrive *drive_list[] = {&myDrive1, &myDrive2};

// Instances for accessing the files on each drive
USBFilesystem pf1(myusb);
USBFilesystem pf2(myusb);
USBFilesystem pf3(myusb);
USBFilesystem pf4(myusb);
USBFilesystem pf5(myusb);
USBFilesystem pf6(myusb);
USBFilesystem pf7(myusb);
USBFilesystem pf8(myusb);

USBFilesystem *filesystem_list[] = {&pf1, &pf2, &pf3, &pf4, &pf5, &pf6, &pf7, &pf8};


// Show USB drive information for the selected USB drive.
void printDriveInfo(USBDrive &drive) {
  // Print USB drive information.
  Serial.printf(F("       connected: %d\n"), drive.msDriveInfo.connected);
  Serial.printf(F("     initialized: %d\n"), drive.msDriveInfo.initialized);
  Serial.printf(F("   USB Vendor ID: %4.4x\n"), drive.msDriveInfo.idVendor);
  Serial.printf(F("  USB Product ID: %4.4x\n"), drive.msDriveInfo.idProduct);
  Serial.printf(F("      HUB Number: %d\n"), drive.msDriveInfo.hubNumber);
  Serial.printf(F("        HUB Port: %d\n"), drive.msDriveInfo.hubPort);
  Serial.printf(F("  Device Address: %d\n"), drive.msDriveInfo.deviceAddress);
  Serial.printf(F("Removable Device: "));
  if(drive.msDriveInfo.inquiry.Removable == 1) {
    Serial.printf(F("YES\n"));
  } else {
    Serial.printf(F("NO\n"));
  }
  Serial.printf(F("        VendorID: %8.8s\n"), drive.msDriveInfo.inquiry.VendorID);
  Serial.printf(F("       ProductID: %16.16s\n"), drive.msDriveInfo.inquiry.ProductID);
  Serial.printf(F("      RevisionID: %4.4s\n"), drive.msDriveInfo.inquiry.RevisionID);
  Serial.printf(F("         Version: %d\n"), drive.msDriveInfo.inquiry.Version);
  Serial.printf(F("    Sector Count: %ld\n"), drive.msDriveInfo.capacity.Blocks);
  Serial.printf(F("     Sector size: %ld\n"), drive.msDriveInfo.capacity.BlockSize);
  uint64_t drivesize = drive.msDriveInfo.capacity.Blocks;
  drivesize *= drive.msDriveInfo.capacity.BlockSize;
  Serial.print(F("   Disk Capacity: "));
  Serial.print(drivesize);
  Serial.println(" Bytes");
  drive.printPartionTable(Serial);
  Serial.println();
}

// Show USB filesystem information
void printFilesystemInfo(USBFilesystem &fs) {
  // print the type and size of the first FAT-type volume
  char volname[32];
  fs.mscfs.getVolumeLabel(volname, sizeof(volname));
  Serial.print("Volume name: ");
  Serial.println(volname);
  Serial.print("Volume type: FAT");
  Serial.println(fs.mscfs.fatType(), DEC);
  Serial.print("Cluster Size: ");
  Serial.print(fs.mscfs.bytesPerCluster());
  Serial.println(" bytes");
  Serial.print("Volume size: ");
  Serial.print(fs.totalSize());
  Serial.println(" bytes");
  Serial.print(" Space used: ");
  elapsedMillis ms = 0;
  Serial.print(fs.usedSize());
  Serial.print(" bytes  (");
  Serial.print(ms);
  Serial.println(" ms to compute)");
  Serial.println();
  Serial.println("Files:");
  fs.mscfs.ls(LS_R | LS_DATE | LS_SIZE);
  Serial.println();
}


void setup()
{
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for Arduino Serial Monitor to connect.
  }

  myusb.begin();
  delay(500);  // give drives a little time to startup
}


void loop(void) {
   myusb.Task();
  // lets chec each of the drives.
  for (uint16_t drive_index = 0; drive_index < (sizeof(drive_list)/sizeof(drive_list[0])); drive_index++) {
    USBDrive *pdrive = drive_list[drive_index];
    if (*pdrive) {
      Serial.printf("\n === Drive index %d found ===\n", drive_index);
      if (!pdrive->filesystemsStarted()) {
        Serial.println("\t Partition ");
        pdrive->startFilesystems();
      }
      printDriveInfo(*pdrive);

      // Should we print all of the drive infos first or also print out the partitions
      // associated with this drive?  Try the later here...
      bool first_fs = true;
      for (uint16_t fs_index = 0; fs_index < (sizeof(filesystem_list)/sizeof(filesystem_list[0])); fs_index++) {
        USBFilesystem *pfs = filesystem_list[fs_index];
        if (*pfs && pfs->device == pdrive) {
          Serial.printf("=== File system (%u) ===\n", fs_index);
          if (first_fs) {
            const uint8_t *psz;
            if ((psz = pfs->manufacturer()) != nullptr) Serial.printf("\tManufacturer: %s\n", psz);
            if ((psz = pfs->product()) != nullptr) Serial.printf("\tProduct: %s\n", psz);
            if ((psz = pfs->serialNumber()) != nullptr) Serial.printf("\tSerial Number: %s\n", psz);
            first_fs = false;
          }
          printFilesystemInfo(*pfs);          
        }

      }
    }
  }
  while (Serial.read() != -1) ;
  Serial.println("\n *** enter any key to run again ***");
  while (Serial.read() == -1)  myusb.Task();
  while (Serial.read() != -1) ;
  
}
