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
#include "BTHIDDumper.h"
#include "USBDeviceInfo.h"

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBDeviceInfo dinfo(myusb);  // will never claim anything...
//BluetoothController bluet(myusb, false, "0000", true);   // Version does pairing to device
BluetoothController bluet(myusb);  // version assumes it already was paired

BTHIDDumpController hdc1(myusb, 1);
BTHIDDumpController hdc2(myusb, 2);

USBDriver *drivers[] = { &hub1, &hub2, &bluet };
#define CNT_DEVICES (sizeof(drivers) / sizeof(drivers[0]))
const char *driver_names[CNT_DEVICES] = { "Hub1", "Hub2", "BT" };
bool driver_active[CNT_DEVICES] = { false, false, false };

BTHIDDumpController *bthiddrivers[] = { &hdc1, &hdc2 };
#define CNT_BTHIDDEVICES (sizeof(bthiddrivers) / sizeof(bthiddrivers[0]))
const char *bthid_driver_names[CNT_BTHIDDEVICES] = { "HDC1", "HDC2" };
bool bthid_driver_active[CNT_BTHIDDEVICES] = { false, false };



bool show_changed_only = false;
void setup() {
  Serial1.begin(2000000);
  while (!Serial)
    ;  // wait for Arduino Serial Monitor
  if (CrashReport) Serial.print(CrashReport);
  Serial.println("\n\nUSB HID Device Info Program");
  Serial.println("\nThis Sketch shows information about plugged in HID devices");
  Serial.println("\n*** You can control the output by simple character input to Serial ***");
  Serial.println("R - Turns on or off showing the raw data");
  Serial.println("C - Toggles showing changed data only on or off");
  Serial.println("A - Try connecting and printing SDP data again");
  Serial.println("P - Start a normal Pairing operation with 0000");
  Serial.println("S - Start an SDP pairing operation");
  Serial.println("E - Erase Key pairing information");


  Serial.println("<anything else> - toggles showing the Hid formatted breakdown of the data\n");

  myusb.begin();
}


void loop() {
  myusb.Task();

  if (Serial.available()) {
    int ch = Serial.read();  // get the first char.
    while (Serial.read() != -1)
      ;
    if (ch == 'r' || (ch == 'R')) {
      if (BTHIDDumpController::show_raw_data) {
        BTHIDDumpController::show_raw_data = false;
        BTHIDDumpController::show_formated_data = true;
        Serial.println("\n*** Turn off RAW output formatted data is on ***\n");
      } else {
        BTHIDDumpController::show_raw_data = true;
        Serial.println("\n*** Turn on RAW output ***\n");
      }
    } else if (ch == 'a' || (ch == 'A')) {
      Serial.println("\n>>>>>>>>>> Try to decode SDP Data <<<<<<<<<<");
      if (hdc1) hdc1.decode_SDP_Data(true);
      if (hdc2) hdc2.decode_SDP_Data(true);

    } else if (ch == 'C' || (ch == 'c')) {
      if (BTHIDDumpController::changed_data_only) {
        BTHIDDumpController::changed_data_only = false;
        Serial.println("***\n Now Showing all data ***\n");
      } else {
        BTHIDDumpController::changed_data_only = true;
        Serial.println("***\n Now Showing changed data only ***\n");
      }
    } else if (ch == 'P') {
      if (bluet.startDevicePairing("0000", false)) {
        Serial.println("Pairing operation started with SSP false");

      } else {
        Serial.println("Staring of Pairing operation failed");
      }

    } else if (ch == 'S') {
      if (bluet.startDevicePairing("0000", true)) {
        Serial.println("Pairing operation started with SSP true");

      } else {
        Serial.println("Staring of Pairing operation failed");
      }
    } else if (ch == 'E') {
      Serial.println("Erase Pairing Link Keys");
      bluet.writeLinkKey(nullptr, nullptr);  // which should clear all of them.  W
    } else {
      if (BTHIDDumpController::show_formated_data) {
        BTHIDDumpController::show_formated_data = false;
        BTHIDDumpController::show_raw_data = true;  // At least make sure raw raw data is output
        Serial.println("\n*** Turn off formatted output formated HID data is on ***\n");
      } else {
        BTHIDDumpController::show_formated_data = true;
        Serial.println("\n*** Turn on formated output ***\n");
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

  // Then Bluetooth devices
  for (uint8_t i = 0; i < CNT_BTHIDDEVICES; i++) {
    if (*bthiddrivers[i] != bthid_driver_active[i]) {
      if (bthid_driver_active[i]) {
        Serial.printf("*** BTHID Device %s - disconnected ***\n", bthid_driver_names[i]);
        bthid_driver_active[i] = false;
      } else {
        Serial.printf("*** BTHID Device %s %x:%x - connected ***\n", bthid_driver_names[i], bthiddrivers[i]->idVendor(), bthiddrivers[i]->idProduct());

        bthid_driver_active[i] = true;
        const uint8_t *psz = bthiddrivers[i]->manufacturer();
        if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = bthiddrivers[i]->product();
        if (psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = bthiddrivers[i]->serialNumber();
        if (psz && *psz) Serial.printf("  Serial: %s\n", psz);

        // lets dump the SDP data
        bthiddrivers[i]->decode_SDP_Data(false);
      }
    }
  }
}