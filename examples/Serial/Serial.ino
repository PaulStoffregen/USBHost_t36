// Simple test of USB Host Serial test
//
// This example is in the public domain

#include "USBHost_t36.h"
#define USBBAUD 115200
uint32_t baud = USBBAUD;
uint32_t format = USBHOST_SERIAL_8N1;
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);

// There is now two versions of the USBSerial class, that are both derived from a common Base class
// The difference is on how large of transfers that it can handle.  This is controlled by
// the device descriptor, where up to now we handled those up to 64 byte USB transfers.
// But there are now new devices that support larger transfer like 512 bytes.  This for example
// includes the Teensy 4.x boards.  For these we need the big buffer version. 
// uncomment one of the following defines for userial
USBSerial userial(myusb);  // works only for those Serial devices who transfer <=64 bytes (like T3.x, FTDI...)
//USBSerial_BigBuffer userial(myusb, 1); // Handles anything up to 512 bytes
//USBSerial_BigBuffer userial(myusb); // Handles up to 512 but by default only for those > 64 bytes


USBDriver *drivers[] = {&hub1, &hub2, &hid1, &hid2, &hid3, &userial};
#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"Hub1", "Hub2",  "HID1", "HID2", "HID3", "USERIAL1" };
bool driver_active[CNT_DEVICES] = {false, false, false, false};

void setup()
{
  pinMode(13, OUTPUT);
  while (!Serial && (millis() < 5000)) ; // wait for Arduino Serial Monitor
  Serial.println("\n\nUSB Host Testing - Serial");
  myusb.begin();
  Serial1.begin(115200);  // We will echo stuff Through Serial1...
}


void loop()
{
  digitalToggleFast(13);
  myusb.Task();
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

        // If this is a new Serial device.
        if (drivers[i] == &userial) {
          // Lets try first outputting something to our USerial to see if it will go out...
          userial.begin(baud);

        }
      }
    }
  }

  if (Serial.available()) {
    Serial.println("Serial Available");
    while (Serial.available()) {
      int ch = Serial.read();
      if (ch == '#') {
        // Lets see if we have a baud rate specified here... 
        uint32_t new_baud = 0;
        for(;;) {
          ch = Serial.read(); 
          if ((ch < '0') || (ch > '9')) 
            break;
          new_baud = new_baud*10 + ch - '0'; 
        }
        // See if the user is specifying a format: 8n1, 7e1, 7e2, 8n2 
        // Note this is Quick and very dirty code... 
        // 
        if (ch == ',') {
          char command_line[10];
          ch = Serial.read();
          while (ch == ' ') Serial.read();  // ignore any spaces.
          uint8_t cb = 0;
          while ((ch > ' ') && (cb < sizeof(command_line))) {
            command_line[cb++] = ch;
            ch = Serial.read();
          }
          command_line[cb] = '\0'; 
          if (CompareStrings(command_line, "8N1")) format = USBHOST_SERIAL_8N1;
          else if (CompareStrings(command_line, "8N2")) format = USBHOST_SERIAL_8N2;
          else if (CompareStrings(command_line, "7E1")) format = USBHOST_SERIAL_7E1;
          else if (CompareStrings(command_line, "7O1")) format = USBHOST_SERIAL_7O1;
        }
        Serial.println("\n*** Set new Baud command ***\n  do userial.end()");
        userial.end();  // Do the end statement;
        if (new_baud) {
          baud = new_baud;
          Serial.print("  New Baud: ");
          Serial.println(baud);
          Serial.print("  Format: ");
          Serial.println(format, HEX);
          userial.begin(baud, format);
          Serial.println("  Completed ");
        } else {
          Serial.println("  New Baud 0 - leave disabled");
        }

        while (Serial.read() != -1);
      } else { 
        userial.write(ch);
      }
    }
  }

  while (Serial1.available()) {
//    Serial.println("Serial1 Available");
    Serial1.write(Serial1.read());
  }

  while (userial.available()) {
//    Serial.println("USerial Available");
    Serial.write(userial.read());
  }
}

bool CompareStrings(const char *sz1, const char *sz2) {
  while (*sz2 != 0) {
    if (toupper(*sz1) != toupper(*sz2)) 
      return false;
    sz1++;
    sz2++;
  }
  return true; // end of string so show as match
}
