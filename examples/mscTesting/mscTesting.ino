//Copyright 2020 by Warren Watson
// Version 02-Feb-20
//
#include "Arduino.h"
#include "USBHost_t36.h"

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);

msController msDrive1(myusb);
msController msDrive2(myusb);

// Show USB drive information for the selected USB drive.
int showUSBDriveInfo(msController *drive) {
	if(drive == &msDrive1) {
		Serial.printf(F("msDrive1 is "));
	} else {
		Serial.printf(F("msDrive2 is "));
	}
	if(drive->msDriveInfo.mounted == true) {// check if mounted.
		Serial.printf(F("Mounted\n\n"));
	} else {
		Serial.printf(F("NOT Mounted\n\n"));
	}
	// Now we will print out the information.
	Serial.printf(F("   connected %d\n"),drive->msDriveInfo.connected);
	Serial.printf(F("   initialized %d\n"),drive->msDriveInfo.initialized);
	Serial.printf(F("   USB Vendor ID: %4.4x\n"),drive->msDriveInfo.idVendor);
	Serial.printf(F("  USB Product ID: %4.4x\n"),drive->msDriveInfo.idProduct);
	Serial.printf(F("      HUB Number: %d\n"),drive->msDriveInfo.hubNumber);
	Serial.printf(F("        HUB Port: %d\n"),drive->msDriveInfo.hubPort);
	Serial.printf(F("  Device Address: %d\n"),drive->msDriveInfo.deviceAddress);
	Serial.printf(F("Removable Device: "));
	if(drive->msDriveInfo.inquiry.Removable == 1)
		Serial.printf(F("YES\n"));
	else
		Serial.printf(F("NO\n"));
	Serial.printf(F("        VendorID: %8.8s\n"),drive->msDriveInfo.inquiry.VendorID);
	Serial.printf(F("       ProductID: %16.16s\n"),drive->msDriveInfo.inquiry.ProductID);
	Serial.printf(F("      RevisionID: %4.4s\n"),drive->msDriveInfo.inquiry.RevisionID);
	Serial.printf(F("         Version: %d\n"),drive->msDriveInfo.inquiry.Version);
	Serial.printf(F("    Sector Count: %ld\n"),drive->msDriveInfo.capacity.Blocks);
	Serial.printf(F("     Sector size: %ld\n"),drive->msDriveInfo.capacity.BlockSize);
	Serial.printf(F("   Disk Capacity: %.f Bytes\n\n"),(double_t)drive->msDriveInfo.capacity.Blocks *
										(double_t)drive->msDriveInfo.capacity.BlockSize);
	return 0;
}

static uint8_t mscError = 0;

void setup() {
	while(!Serial);

	Serial.printf(F("\n\nMSC TEST\n\n"));
	Serial.printf(F("Initializing USB Drive(s)\n"));
	
	myusb.begin();

	if(mscError = msDrive1.mscInit())
		Serial.printf(F("msDrive1 not connected: Code: %d\n\n"),  mscError);
	else
		Serial.printf(F("msDrive1  connected\n"));
	
	if(mscError = msDrive2.mscInit())
		Serial.printf(F("msDrive2 not connected: Code: %d\n\n"),  mscError);
	else
		Serial.printf(F("msDrive2  connected\n"));

}

void loop() {
	char op = 0;
	Serial.printf(F("\nPress a key to show USB drive info:\n\n"));

	while(!Serial.available()) yield();
	op = Serial.read();
	if(Serial.available()) Serial.read(); // Get rid of CR or LF if there.

	// Check if msDrive1 is plugged in and initialized
	if((mscError = msDrive1.checkConnectedInitialized()) != MS_INIT_PASS) {
		Serial.printf(F("msDrive1 not connected: Code: %d\n\n"),  mscError);
	} else {
		Serial.printf(F("msDrive1  connected/initilized\n"));
		showUSBDriveInfo(&msDrive1);
	}
	// Check if msDrive2 is plugged in and initialized
	if((mscError = msDrive2.checkConnectedInitialized()) != MS_INIT_PASS) {
		Serial.printf(F("msDrive2 not connected: Code: %d\n\n"),  mscError);
	} else {
		Serial.printf(F("msDrive2  connected/initilized\n"));
		showUSBDriveInfo(&msDrive2);
	}
}
