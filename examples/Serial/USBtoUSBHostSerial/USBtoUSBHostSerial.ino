//=============================================================================
// Simple USBHost USBSerial test
// This sketch is very much like the main teensy example: USBtoSerial.ino
// but instead of going to hardware serial port it forwards the data
// between the USB Serial and a USB Serial device connected to the USB Host
// port.
//
// This example is in the public domain
//=============================================================================

#include <USBHost_t36.h>

//=============================================================================
// Options
//=============================================================================
// uncomment the line below to output debug information
//#define DEBUG_OUTPUT

// Uncomment the line below to print out information about the USB devices that attach.
#define PRINT_DEVICE_INFO
#define USBBAUD 1000000 //115200

uint32_t baud = USBBAUD;
uint32_t format = USBHOST_SERIAL_8N1;
USBHost myusb;

//=============================================================================
// USB Objects
//=============================================================================

// Optional if you use are possibly going to plug your USB Serial device
// into a USB Hub, you should include one or more USB Hub objects. 
// How many? depends as some HUB chips support lets say 4 ports, so if the HUB
// actuall has more than this, than internally it may be made up using multiple
// HUBs and you may need one of the hub objects for each one of these.
USBHub hub1(myusb);
//USBHub hub2(myusb);
//USBHub hub3(myusb);

// There is now two versions of the USBSerial class, that are both derived from a common Base class
// The difference is on how large of transfers that it can handle.  This is controlled by
// the device descriptor, where up to now we handled those up to 64 byte USB transfers.
// But there are now new devices that support larger transfer like 512 bytes.  This for example
// includes the Teensy 4.x boards.  For these we need the big buffer version. 
// uncomment one of the following defines for userial
//USBSerial userial(myusb);  // works only for those Serial devices who transfer <=64 bytes (like T3.x, FTDI...)
USBSerial_BigBuffer userial(myusb, 1); // Handles anything up to 512 bytes
//USBSerial_BigBuffer userial(myusb); // Handles up to 512 but by default only for those > 64 bytesUSBHost myusb;
// We also now have an optional set of parameters for the constructor that allows you to pass in a Vendor ID,
// product ID, what it maps to and the like, to handle USB objects which have underlying USB to serial converters, 
// that are known, but not in our list. 
//USBSerial_BigBuffer userial(myusb, 1, 0x10c4, 0xea60, USBSerialBase::CP210X, 0); // Handles anything up to 512 bytes
//
// Although not the Serial class, this sketch can also handle forwarding of the Teensy Serial Emulation object (SEREMU)
// may need multpile HID Parser objects depending on what other USB types the object supports.
//#define USERIAL_IS_SEREMU   // SEREMU is not a top level device, so we need to update device tables for this
#ifdef USERIAL_IS_SEREMU
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBSerialEmu userial(myusb);
#endif


// Define the buffer to use to copy between the two devices
// I am using 512 bytes as that is the largest one that can happen between two T4.x
// if other type devices could easily reduce to something like 64 bytes
char buffer[512];
uint32_t led_on_time=0;

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

// This sketch can optionally print out when some of these devices are inserted and removed. 
#ifdef PRINT_DEVICE_INFO
// If you add devices you may want to extend these structures to include them as well.
#ifndef USERIAL_IS_SEREMU
USBDriver *drivers[] = {&userial, &hub1};
#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"USERIAL", "Hub1"};
bool driver_active[CNT_DEVICES] = {false, false};

#else
// For SEREMU
USBDriver *drivers[] = {&hid1, &hid2, &hub1};
#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"HID1", "HID2", "Hub1"};
bool driver_active[CNT_DEVICES] = {false, false, false};
// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = {&userial };
#define CNT_HIDDEVICES (sizeof(hiddrivers) / sizeof(hiddrivers[0]))
const char *hid_driver_names[CNT_DEVICES] = { "USERIAL" };
bool hid_driver_active[CNT_DEVICES] = { false };
#endif  // USERIAL_IS_SEREMU

#endif



//=============================================================================
// Setup - only runs once
//=============================================================================
void setup() {
  myusb.begin();
  // pre-Configure the USB Host Serial adapter at startup time
  // Note: versions up till now, this needed to be done after the device connects
  // and at that time the connecton defaulted to 115200
  // In both of these begins below, the actual baud rate specified does not impact the USB communications.
  // it is simply a hint to the devices that allow them to configure other parts of their device.  Example
  // USB to serial devices use this to figure their hardware USART or UART. 
  userial.begin(USBBAUD); 
  Serial.begin(USBBAUD);

  pinMode(LED_BUILTIN, OUTPUT);

}

//=============================================================================
// loop: continuously called.
//=============================================================================
void loop() {
  myusb.Task(); 

  uint16_t rd, wr, n;

  // check if any data has arrived on the USB virtual serial port
  rd = Serial.available();
  if (rd > 0) {
    // check if the USB Host serial port is ready to transmit
    wr = userial.availableForWrite();
    if (wr > 0) {
      // compute how much data to move, the smallest
      // of rd, wr and the buffer size
      if (rd > wr) rd = wr;
      if (rd > sizeof(buffer)) rd = sizeof(buffer);
      // read data from the USB port
      n = Serial.readBytes((char *)buffer, rd);
      // write it to the USB Host serial port
      DBGPrintf("S-U(%u %u)\n", rd, n);
      userial.write(buffer, n);
      // turn on the LED to indicate activity
      digitalWrite(LED_BUILTIN, HIGH);
      led_on_time = millis();
    }
  }

  // check if any data has arrived on the USBHost serial port
  rd = userial.available();
  if (rd > 0) {
    // check if the USB virtual serial port is ready to transmit
    wr = Serial.availableForWrite();
    if (wr > 0) {
      // compute how much data to move, the smallest
      // of rd, wr and the buffer size
      if (rd > wr) rd = wr;
      if (rd > 80) rd = 80;
      // read data from the USB host serial port
      n = userial.readBytes((char *)buffer, rd);
      // write it to the USB port
      DBGPrintf("U-S(%u %u):", rd, n);
      Serial.write(buffer, n);
      // turn on the LED to indicate activity
      digitalWrite(LED_BUILTIN, HIGH);
      led_on_time = millis();
    }
  }

  // if the LED has been left on without more activity, turn it off
  if (led_on_time && (millis() - led_on_time > 3)) {
    digitalWrite(LED_BUILTIN, LOW);
    led_on_time = 0; 
  }

  // check if the USB virtual serial wants a new baud rate
  // ignore if 0 as current Serial monitor of Arduino sets to 0..
  uint32_t cur_usb_baud = Serial.baud();
  if (cur_usb_baud && (cur_usb_baud != baud)) {
    baud = cur_usb_baud;
    DBGPrintf("DEBUG: baud change: %u\n", baud);
    if (baud == 57600) {
      // This ugly hack is necessary for talking
      // to the arduino bootloader, which actually
      // communicates at 58824 baud (+2.1% error).
      // Teensyduino will configure the UART for
      // the closest baud rate, which is 57143
      // baud (-0.8% error).  Serial communication
      // can tolerate about 2.5% error, so the
      // combined error is too large.  Simply
      // setting the baud rate to the same as
      // arduino's actual baud rate works.
      userial.begin(58824);
    } else {
      userial.begin(baud);
    }
  }

  // Optional check for defice changes
  #ifdef PRINT_DEVICE_INFO
  check_for_usbhost_device_changes();
  #endif
}

// Optional check for defice changes
#ifdef PRINT_DEVICE_INFO
void check_for_usbhost_device_changes() {
  // Print out information about different devices.
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
        #ifndef USERIAL_IS_SEREMU
        if (drivers[i] == &userial) {
          userial.begin(baud);
        }
        #endif
      }
    }
  }
#ifdef USERIAL_IS_SEREMU

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
#endif

}
#endif
