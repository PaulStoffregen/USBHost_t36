//=============================================================================
// Simple test to forward RAWHID and Seremu USB Host device
// It also forwards any Serial Emulation data it receives as well to Serial
// This sketch is very much like the main teensy example: USBtoSerial.ino
// 
// This sketch needs to be built with USB Type of RawHID
//
// The main testing for this is to have a Teensy programmed with the example
// sketch: Examples -> _Teensy -> USBRawHID -> Basic
// plugged into the USBHost port.
//
// Which talks to the host test software you can download from:
//    https://www.pjrc.com/teensy/rawhid.html
//
//
// This example is in the public domain
//=============================================================================

#include "USBHost_t36.h"

//=============================================================================
// Options
//=============================================================================
// uncomment the line below to output debug information
//#define DEBUG_OUTPUT

// Uncomment the line below to print out information about the USB devices that attach.
#define PRINT_DEVICE_INFO

//=============================================================================
// USB Objects
//=============================================================================
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);

RawHIDController rawhid1(myusb);
USBSerialEmu seremu(myusb);

uint8_t buffer[512];  // most of the time will be 64 bytes, but if we support 512...

//=============================================================================
// optional debug stuff
//=============================================================================
#ifdef DEBUG_OUTPUT
#define DBGPrintf Serial.printf
#else
// not debug have it do nothing
inline void DBGPrintf(...) {
}
#endif

#ifdef PRINT_DEVICE_INFO
USBDriver *drivers[] = { &hub1, &hub2, &hid1, &hid2 };
#define CNT_DEVICES (sizeof(drivers) / sizeof(drivers[0]))
const char *driver_names[CNT_DEVICES] = { "Hub1", "Hub2", "HID1", "HID2"};
bool driver_active[CNT_DEVICES] = { false, false, false, false };

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = { &rawhid1, &seremu };
#define CNT_HIDDEVICES (sizeof(hiddrivers) / sizeof(hiddrivers[0]))
const char *hid_driver_names[CNT_DEVICES] = { "RawHid1", "SerEmu" };
bool hid_driver_active[CNT_DEVICES] = { false, false };
#endif


//=============================================================================
// Setup
//=============================================================================
void setup() {
  while (!Serial && millis() < 5000) ; //wait up to 5 seconds
#ifdef __IMXRT1062__
  if (CrashReport) {
    Serial.print(CrashReport);
  }
 #endif

  DBGPrintf("\n\nUSB Host RawHid and Seremu forwarding\n");

  myusb.begin();

  rawhid1.attachReceive(OnReceiveHidData);
}

//=============================================================================
// loop
//=============================================================================
void loop() {
  uint16_t rd, wr, n;
  myusb.Task();
  // See if any RawHID USB packets are waiting for us.
  n = RawHID.recv(buffer, 0); // 0 timeout = do not wait
  if (n > 0) {
    DBGPrintf("RawHID.recv(%u)", n);
    rawhid1.sendPacket(buffer);
  }
  // check if any data has arrived on the USB virtual serial port
  rd = Serial.available();
  if (rd > 0) {
    // check if the USB Host serial port is ready to transmit
    wr = seremu.availableForWrite();
    if (wr > 0) {
      // compute how much data to move, the smallest
      // of rd, wr and the buffer size
      if (rd > wr) rd = wr;
      if (rd > sizeof(buffer)) rd = sizeof(buffer);
      // read data from the USB port
      n = Serial.readBytes((char *)buffer, rd);
      // write it to the USB Host serial port
      DBGPrintf("S-U(%u %u)\n", rd, n);
      seremu.write(buffer, n);
    }
  }

  // check if any data has arrived on the USBHost serial port
  rd = seremu.available();
  if (rd > 0) {
    // check if the USB virtual serial port is ready to transmit
    wr = Serial.availableForWrite();
    if (wr > 0) {
      // compute how much data to move, the smallest
      // of rd, wr and the buffer size
      if (rd > wr) rd = wr;
      if (rd > 80) rd = 80;
      // read data from the USB host serial port
      n = seremu.readBytes((char *)buffer, rd);
      // write it to the USB port
      DBGPrintf("U-S(%u %u):", rd, n);
      Serial.write(buffer, n);
    }
  }

  // Sort of the main Copy of the USBSerial code 
  uint8_t emu_buffer[64];
  uint16_t cb;
  if ((cb = seremu.available())) {
    if (cb > sizeof(emu_buffer)) cb = sizeof(emu_buffer);
    int rd = seremu.readBytes(emu_buffer, cb);
    Serial.write(emu_buffer, rd);
  }
  #ifdef PRINT_DEVICE_INFO
  // Optional. 
  UpdateActiveDeviceInfo();
  #endif
}

bool OnReceiveHidData(uint32_t usage, const uint8_t *data, uint32_t len) {
  DBGPrintf("OnReceiveHidDta(%x %p %u)\n", usage, data, len);
  RawHID.send(data, len);
  return true;
}


#ifdef PRINT_DEVICE_INFO
// check to see if the device list has changed:
void UpdateActiveDeviceInfo() {
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

        // Note: with some keyboards there is an issue that they don't output in boot protocol mode
        // and may not work.  The above code can try to force the keyboard into boot mode, but there
        // are issues with doing this blindly with combo devices like wireless keyboard/mouse, which
        // may cause the mouse to not work.  Note: the above id is in the builtin list of
        // vendor IDs that are already forced
      }
    }
  }

  for (uint8_t i = 0; i < CNT_HIDDEVICES; i++) {
    if (*hiddrivers[i] != hid_driver_active[i]) {
      if (hid_driver_active[i]) {
        Serial.printf("*** HID Device %s - disconnected ***\n", hid_driver_names[i]);
        hid_driver_active[i] = false;
      } else {
        Serial.printf("*** HID Device %s %x:%x - connected ***\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
        hid_driver_active[i] = true;

        const uint8_t *psz = hiddrivers[i]->manufacturer();
        if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = hiddrivers[i]->product();
        if (psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = hiddrivers[i]->serialNumber();
        if (psz && *psz) Serial.printf("  Serial: %s\n", psz);
        //        if (hiddrivers[i] == &seremu) {
        //          Serial.printf("   RX Size:%u TX Size:%u\n", seremu.rxSize(), seremu.txSize());
        //        }
        //        if (hiddrivers[i] == &rawhid1) {
        //          Serial.printf("   RX Size:%u TX Size:%u\n", rawhid1.rxSize(), rawhid1.txSize());
        //        }
      }
    }
  }
}
#endif