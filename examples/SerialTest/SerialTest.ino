// Simple test of USB Host Mouse/Keyboard
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
USBSerial userial(myusb);

USBDriver *drivers[] = {&hub1, &hub2, &hid1, &hid2, &hid3, &userial};
#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"Hub1", "Hub2",  "HID1", "HID2", "HID3", "USERIAL1" };
bool driver_active[CNT_DEVICES] = {false, false, false, false};

void setup()
{
  pinMode(13, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  for (int i = 0; i < 5; i++) {
    digitalWrite(2, HIGH);
    delayMicroseconds(50);
    digitalWrite(2, LOW);
    delayMicroseconds(50);
  }
  while (!Serial && (millis() < 5000)) ; // wait for Arduino Serial Monitor
  Serial.println("\n\nUSB Host Testing - Serial");
  myusb.begin();
  Serial1.begin(115200);  // We will echo stuff Through Serial1...

}


void loop()
{
  digitalWrite(13, !digitalRead(13));
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
      if (ch == '$') {
        BioloidTest();
        while (Serial.read() != -1);
      } else if (ch == '#') {
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
        digitalWriteFast(2, HIGH);
        userial.end();  // Do the end statement;
        digitalWriteFast(2, LOW);
        if (new_baud) {
          baud = new_baud;
          Serial.print("  New Baud: ");
          Serial.println(baud);
          Serial.print("  Format: ");
          Serial.println(format, HEX);
          digitalWriteFast(3, HIGH);
          userial.begin(baud, format);
          digitalWriteFast(3, LOW);
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

//#define ID_MASTER 200 
#define ID_MASTER 0xfd
// Extract stuff from Bioloid library..
#define AX12_BUFFER_SIZE 128
#define COUNTER_TIMEOUT 12000

/** Instruction Set **/ #define AX_PING                     1
#define AX_READ_DATA                2
#define AX_WRITE_DATA               3
#define AX_REG_WRITE                4
#define AX_ACTION                   5
#define AX_RESET                    6
#define AX_SYNC_WRITE               131

#define AX_TORQUE_ENABLE            24
#define AX_LED                      25
#define AX_CW_COMPLIANCE_MARGIN     26
#define AX_CCW_COMPLIANCE_MARGIN    27
#define AX_CW_COMPLIANCE_SLOPE      28
#define AX_CCW_COMPLIANCE_SLOPE     29
#define AX_GOAL_POSITION_L          30
#define AX_GOAL_POSITION_H          31
#define AX_GOAL_SPEED_L             32
#define AX_GOAL_SPEED_H             33
#define AX_TORQUE_LIMIT_L           34
#define AX_TORQUE_LIMIT_H           35
#define AX_PRESENT_POSITION_L       36
#define AX_PRESENT_POSITION_H       37


void BioloidTest() {
  uint8_t master_id = 200;
  Serial.println("\n*** Bioloid Test ***");
  if (ax12GetRegister(master_id, 0, 1) != -1) {
    Serial.println("Controller found at 200");
  } else {
    Serial.println("Controller not at 200 try 0xfd");
    master_id = 0xfd;
    if (ax12GetRegister(master_id, 0, 1) != -1) {
      Serial.println("Controller found at 0xfd");
    } else {
      Serial.println("Controller not found");
    }
  }
  for (uint8_t reg = 0; reg < 10; reg++) {
    myusb.Task();

    Serial.print(ax12GetRegister(master_id, reg, 1), HEX);
    Serial.print(" ");
  }
  Serial.println();
  // Now assuming we found controller... 
  // May need to turn on power on controller
  ax12SetRegister(master_id, AX_TORQUE_ENABLE, 1);
  delay(2);

  // Lets see if we can get the current position for any servo
  for (int i = 0; i < 254; i++) {
    int servo_pos = ax12GetRegister(i, AX_PRESENT_POSITION_L, 2); 
    if (servo_pos != -1) {
      Serial.printf("Servo: %d Pos: %d\n", i, servo_pos);
    }
  }

}



unsigned char ax_rx_buffer[AX12_BUFFER_SIZE];
int ax12GetRegister(int id, int regstart, int length) {
  // 0xFF 0xFF ID LENGTH INSTRUCTION PARAM... CHECKSUM
  int return_value;
  digitalWriteFast(2, HIGH);
  int checksum = ~((id + 6 + regstart + length) % 256);
  userial.write(0xFF);
  userial.write(0xFF);
  userial.write(id);
  userial.write(4);    // length
  userial.write(AX_READ_DATA);
  userial.write(regstart);
  userial.write(length);
  userial.write(checksum);
  userial.flush();  // make sure the data goes out.

  if (ax12ReadPacket(length + 6) > 0) {
    //    ax12Error = ax_rx_buffer[4];
    if (length == 1)
      return_value = ax_rx_buffer[5];
    else
      return_value = ax_rx_buffer[5] + (ax_rx_buffer[6] << 8);
  } else {
    digitalWriteFast(3, !digitalReadFast(3));
    return_value = -1;
  }
  digitalWriteFast(2, LOW);
  return return_value;

}

void ax12SetRegister(int id, int regstart, int data){
    int checksum = ~((id + 4 + AX_WRITE_DATA + regstart + (data&0xff)) % 256);
    userial.write(0xFF);
    userial.write(0xFF);
    userial.write(id);
    userial.write(4);    // length
    userial.write(AX_WRITE_DATA);
    userial.write(regstart);
    userial.write(data&0xff);
    // checksum =
    userial.write(checksum);
    userial.flush();
    //ax12ReadPacket();
}



int ax12ReadPacket(int length) {
  unsigned long ulCounter;
  unsigned char offset, checksum;
  unsigned char *psz;
  unsigned char *pszEnd;
  int ch;


  offset = 0;

  psz = ax_rx_buffer;
  pszEnd = &ax_rx_buffer[length];

  while (userial.read() != -1) ;
  uint32_t ulStart = millis();
  // Need to wait for a character or a timeout...
  do {
    ulCounter = COUNTER_TIMEOUT;
    while ((ch = userial.read()) == -1) {
      if ((millis() - ulStart) > 10) {
        //if (!--ulCounter) {
        Serial.println("Timeout");
        return 0;   // Timeout
      }
    }
  } while (ch != 0xff) ;
  *psz++ = 0xff;
  while (psz != pszEnd) {
    ulCounter = COUNTER_TIMEOUT;
    while ((ch = userial.read()) == -1) {
      //Serial.printf("Read ch: %x\n", ch);
      if (!--ulCounter)  {
        return 0;   // Timeout
      }
    }
    *psz++ = (unsigned char)ch;
  }
  checksum = 0;
  for (offset = 2; offset < length; offset++)
    checksum += ax_rx_buffer[offset];
  if (checksum != 255) {
    return 0;
  } else {
    return 1;
  }
}

