// HIDDeviceInfo - Simple HID device example
// 
// This Simple test sketch is setup to print out HID information about a device
// The other two tabs are a simple C++ subclass of the USBHIDInput class that is part 
// of the USBHost_t36 library.  
//
// This subclass simply tries to connect to each different HID object and
// the only thing it does is to try to print out all of the data it receives
// in a reasonable way. 
//
// The idea is that with the output from this sketch we can hopefully add support 
// for some additional devices that are not currently supported or allows you to 
// develop your own. 
// 
// You can use Serial Input to control how much data is displayed per each HID packet
// received by the sketch.
//
// By Default it displays both the RAW (Hex dump) of the data received, as well
// as the data as the HID interpreter walks through the data into the individual
// fields, which we then print out.  
//
// There are options to turn off some of this output, also an option that you can
// toggle on or off (C) to only try to show the changed fields.   
//
// This example is in the public domain

#include <USBHost_t36.h>
#include "HIDDumper.h"
#include "USBDeviceInfo.h"

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBDeviceInfo dinfo(myusb); // will never claim anything... 
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);
USBHIDParser hid4(myusb);
USBHIDParser hid5(myusb);

HIDDumpController hdc1(myusb, 1);
HIDDumpController hdc2(myusb, 2);
HIDDumpController hdc3(myusb, 3);
HIDDumpController hdc4(myusb, 4);
HIDDumpController hdc5(myusb, 5);

USBDriver *drivers[] = {&hub1, &hub2, &hid1, &hid2, &hid3, &hid4, &hid5};
#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"Hub1", "Hub2", "HID1" , "HID2", "HID3", "HID4", "HID5"};
bool driver_active[CNT_DEVICES] = {false, false, false, false};

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = {&hdc1, &hdc2,  &hdc3, &hdc4, &hdc5};
#define CNT_HIDDEVICES (sizeof(hiddrivers)/sizeof(hiddrivers[0]))
const char * hid_driver_names[CNT_DEVICES] = {"hdc1", "hdc2", "hdc3", "hdc4", "hdc5"};
bool hid_driver_active[CNT_DEVICES] = {false, false};
bool show_changed_only = false;
void setup()
{
  Serial1.begin(2000000);
  while (!Serial) ; // wait for Arduino Serial Monitor
  Serial.println("\n\nUSB HID Device Info Program");
  Serial.println("\nThis Sketch shows information about plugged in HID devices");
  Serial.println("\n*** You can control the output by simple character input to Serial ***");
  Serial.println("R - Turns on or off showing the raw data");
  Serial.println("C - Toggles showing changed data only on or off");
  Serial.println("<anything else> - toggles showing the Hid formatted breakdown of the data\n");

  myusb.begin();
}


void loop()
{
  myusb.Task();

  if (Serial.available()) {
    int ch = Serial.read(); // get the first char.
    while (Serial.read() != -1) ;
    if (ch == 'r' || (ch == 'R')) {
      if (HIDDumpController::show_raw_data) {
        HIDDumpController::show_raw_data = false;
        HIDDumpController::show_formated_data = true;
        Serial.println("\n*** Turn off RAW output formatted data is on ***\n");
      } else {
        HIDDumpController::show_raw_data = true;
        Serial.println("\n*** Turn on RAW output ***\n");
      }
    } else if (ch == 'C' || (ch == 'c')) {
      if (HIDDumpController::changed_data_only) {
        HIDDumpController::changed_data_only = false;
        Serial.println("***\n Now Showing all data ***\n");
      } else {
        HIDDumpController::changed_data_only = true;
        Serial.println("***\n Now Showing changed data only ***\n");

      }
    } else {
      if (HIDDumpController::show_formated_data) {
        HIDDumpController::show_formated_data = false;
        HIDDumpController::show_raw_data = true;  // At least make sure raw raw data is output
        Serial.println("\n*** Turn off formatted output formatted HID data is on ***\n");
      } else {
        HIDDumpController::show_formated_data = true;
        Serial.println("\n*** Turn on formatted output ***\n");
      }
    }
  }

  for (uint8_t i = 0; i < CNT_DEVICES; i++) {
    if (*drivers[i] != driver_active[i]) {
      if (driver_active[i]) {
        Serial.printf("*** Device %s - disconnected ***\n", driver_names[i]);
        driver_active[i] = false;
      } else {
        Serial.printf("*** Device %s %x:%x - connected ***\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
        driver_active[i] = true;

        const uint8_t *psz = drivers[i]->manufacturer();
        if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = drivers[i]->product();
        if (psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = drivers[i]->serialNumber();
        if (psz && *psz) Serial.printf("  Serial: %s\n", psz);

      }
    }
  }

  for (uint8_t i = 0; i < CNT_HIDDEVICES; i++) {
    if (*hiddrivers[i] != hid_driver_active[i]) {
      if (hid_driver_active[i]) {
        Serial.printf("*** HID Device %s - disconnected ***\n", hid_driver_names[i]);
        hid_driver_active[i] = false;
      } else {
        Serial.printf("*** HID Device %s %x: %x - connected ***\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
        hid_driver_active[i] = true;

        const uint8_t *psz = hiddrivers[i]->manufacturer();
        if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = hiddrivers[i]->product();
        if (psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = hiddrivers[i]->serialNumber();
        if (psz && *psz) Serial.printf("  Serial: %s\n", psz);
      }
    }
  }
}