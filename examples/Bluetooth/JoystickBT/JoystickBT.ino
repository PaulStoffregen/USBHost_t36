
// Simple test of USB Host Mouse/Keyboard
//
// This example is in the public domain

#include "USBHost_t36.h"
//#include "debug_tt.h"

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);
USBHIDParser hid4(myusb);
USBHIDParser hid5(myusb);
JoystickController joystick1(myusb);
//BluetoothController bluet(myusb, true, "0000");   // Version does pairing to device
BluetoothController bluet(myusb);   // version assumes it already was paired
int user_axis[64];
uint32_t buttons_prev = 0;
uint32_t buttons;
RawHIDController rawhid1(myusb);
RawHIDController rawhid2(myusb, 0xffc90004);

USBDriver *drivers[] = {&hub1, &hub2, &joystick1, &bluet, &hid1, &hid2, &hid3, &hid4, &hid5};

#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"Hub1", "Hub2", "JOY1D", "Bluet", "HID1" , "HID2", "HID3", "HID4", "HID5"};

bool driver_active[CNT_DEVICES] = {false, false, false, false};

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = {&joystick1, &rawhid1, &rawhid2};

#define CNT_HIDDEVICES (sizeof(hiddrivers)/sizeof(hiddrivers[0]))
const char * hid_driver_names[CNT_DEVICES] = {"Joystick1", "RawHid1", "RawHid2"};

bool hid_driver_active[CNT_DEVICES] = {false, false, false};
bool show_changed_only = false;
bool show_raw_data = false;
bool show_changed_data = false;

uint8_t joystick_left_trigger_value = 0;
uint8_t joystick_right_trigger_value = 0;
uint64_t joystick_full_notify_mask = (uint64_t) - 1;

int psAxis[64];
bool first_joystick_message = true;
uint8_t last_bdaddr[6] = {0, 0, 0, 0, 0, 0};

void setup()
{

  Serial1.begin(2000000);
  while (!Serial) ; // wait for Arduino Serial Monitor

  Serial.println("\n\nUSB Host Testing");
  Serial.println(sizeof(USBHub), DEC);
  myusb.begin();

  delay(2000);

  rawhid1.attachReceive(OnReceiveHidData);
  rawhid2.attachReceive(OnReceiveHidData);
}


void loop()
{
  myusb.Task();

  if (Serial.available()) {
    int ch = Serial.read(); // get the first char.
    while (Serial.read() != -1) ;
    if ((ch == 'b') || (ch == 'B')) {
      Serial.println("Only notify on Basic Axis changes");
      joystick1.axisChangeNotifyMask(0x3ff);
    } else if ((ch == 'f') || (ch == 'F')) {
      Serial.println("Only notify on Full Axis changes");
      joystick1.axisChangeNotifyMask(joystick_full_notify_mask);

    } else {
      if (show_changed_only) {
        show_changed_only = false;
        Serial.println("\n*** Show All fields mode ***");
      } else {
        show_changed_only = true;
        Serial.println("\n*** Show only changed fields mode ***");
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

        if (drivers[i] == &bluet) {
          const uint8_t *bdaddr = bluet.myBDAddr();
          // remember it...
          Serial.printf("  BDADDR: %x:%x:%x:%x:%x:%x\n", bdaddr[0], bdaddr[1], bdaddr[2], bdaddr[3], bdaddr[4], bdaddr[5]);
          for (uint8_t i = 0; i < 6; i++) last_bdaddr[i] = bdaddr[i];
        }
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
      }
    }
  }

  if (joystick1.available()) {
    if (first_joystick_message) {
      Serial.printf("*** First Joystick message %x:%x ***\n",
                    joystick1.idVendor(), joystick1.idProduct());
      first_joystick_message = false;

      const uint8_t *psz = joystick1.manufacturer();
      if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
      psz = joystick1.product();
      if (psz && *psz) Serial.printf("  product: %s\n", psz);
      psz = joystick1.serialNumber();
      if (psz && *psz) Serial.printf("  Serial: %s\n", psz);

      // lets try to reduce number of fields that update
      joystick1.axisChangeNotifyMask(0xFFFFFl);
    }

    for (uint8_t i = 0; i < 64; i++) {
      psAxis[i] = joystick1.getAxis(i);
    }
    switch (joystick1.joystickType()) {
      case JoystickController::UNKNOWN:
      case JoystickController::PS4:
        displayPS4Data();
        break;
      case JoystickController::PS3:
        displayPS3Data();
        break;
      case JoystickController::XBOXONE:
      case JoystickController::XBOX360:
        displayXBoxData();
        break;
      default:
        displayRawData();
        break;
    }
    //for (uint8_t i = 0; i < 24; i++) {
    //  Serial.printf(" %d:%d", i, psAxis[i]);
    //}
    //Serial.println();

    delay(100);
    joystick1.joystickDataClear();
  }

  // See if we have some RAW data
  if (rawhid1) {
    int ch;
    uint8_t buffer[64];
    uint8_t count_chars = 0;
    memset(buffer, 0, sizeof(buffer));
    if (Serial.available()) {
      while (((ch = Serial.read()) != -1) && (count_chars < sizeof(buffer))) {
        buffer[count_chars++] = ch;
      }
      rawhid1.sendPacket(buffer);
    }
  }
}

void displayPS4Data()
{
  buttons = joystick1.getButtons();
  Serial.printf("LX: %d, LY: %d, RX: %d, RY: %d \r\n", psAxis[0], psAxis[1], psAxis[2], psAxis[5]);
  Serial.printf("L-Trig: %d, R-Trig: %d\r\n", psAxis[3], psAxis[4]);
  Serial.printf("Buttons: %x\r\n", buttons);
  Serial.printf("Battery Status: %d\n", ((psAxis[30] & (1 << 4) - 1)*10));
  printAngles();
  Serial.println();

  uint8_t ltv;
  uint8_t rtv;

  ltv = psAxis[3];
  rtv = psAxis[4];

  if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
    joystick_left_trigger_value = ltv;
    joystick_right_trigger_value = rtv;
    Serial.printf("Rumbling: %d, %d\r\n", ltv, rtv);
    joystick1.setRumble(ltv, rtv);
  }
   

  if (buttons != buttons_prev) {
      uint8_t lr = 0;
      uint8_t lg = 0;
      uint8_t lb = 0;
      if(buttons == 0x10008){//Srq
        lr = 0xff;
      }
      if(buttons == 0x40008){//Circ
        lg = 0xff;
      }
      if(buttons == 0x80008){//Tri
        lb = 0xff;
      }
      
      Serial.print(buttons,HEX); Serial.print(", ");
      Serial.print(lr); Serial.print(", "); 
      Serial.print(lg); Serial.print(", "); 
      Serial.println(lb); 
      joystick1.setLEDs(lr, lg, lb);
      buttons_prev = buttons;  
  }

}

void displayPS3Data()

{
  buttons = joystick1.getButtons();
  //buttons = psAxis[2] | ((uint16_t)psAxis[3] << 8);

  // Use L3 (Left joystick button) to toggle Show Raw or not...
  if ((buttons & 0x02) && !(buttons_prev & 0x02)) show_raw_data = !show_raw_data;
  if ((buttons & 0x04) && !(buttons_prev & 0x04)) show_changed_data = !show_changed_data;

  // See about maybe pair...
  if ((buttons & 0x10000) && !(buttons_prev & 0x10000) && (buttons & 0x0C01)) {
    // PS button just pressed and select button pressed act like PS4 share like...
    // Note: you can use either R1 or L1 with the PS button, to work with Sony Move Navigation...
    Serial.print("\nPS3 Pairing Request");
    if (!last_bdaddr[0] && !last_bdaddr[1] && !last_bdaddr[2] && !last_bdaddr[3] && !last_bdaddr[4] && !last_bdaddr[5]) {
      Serial.println(" - failed - no Bluetooth adapter has been plugged in");
    } else if (!hiddrivers[0]) {  // Kludge see if we are connected as HID?
      Serial.println(" - failed - PS3 device not plugged into USB");
    } else {
      Serial.printf(" - Attempt pair to: %x:%x:%x:%x:%x:%x\n", last_bdaddr[0], last_bdaddr[1], last_bdaddr[2], last_bdaddr[3], last_bdaddr[4], last_bdaddr[5]);

      if (! joystick1.PS3Pair(last_bdaddr)) {
        Serial.println("  Pairing call Failed");
      } else {
        Serial.println("  Pairing complete (I hope), make sure Bluetooth adapter is plugged in and try PS3 without USB");
      }
    }
  }


  if (show_raw_data)  {
    displayRawData();
  } else {
    Serial.printf("LX: %d, LY: %d, RX: %d, RY: %d \r\n", psAxis[0], psAxis[1], psAxis[2], psAxis[5]);
    Serial.printf("L-Trig: %d, R-Trig: %d\r\n", psAxis[3], psAxis[4]);
    Serial.printf("Buttons: %x\r\n", buttons);
  }
  uint8_t ltv;
  uint8_t rtv;

  ltv = psAxis[3];
  rtv = psAxis[4];

  if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
    joystick_left_trigger_value = ltv;
    joystick_right_trigger_value = rtv;
    Serial.printf("Rumbling: %d, %d\r\n", ltv, rtv);
    joystick1.setRumble(ltv, rtv);
  }

  if (buttons != buttons_prev) {
    uint8_t leds = 0;
    if (buttons & 0x8000) leds = 1;   //Srq
    if (buttons & 0x2000) leds = 2;   //Cir
    if (buttons & 0x1000) leds = 3;   //Tri
    //Cross = 2
    joystick1.setLEDs(leds);
    buttons_prev = buttons;
  }
}

void displayXBoxData()
{
  buttons = joystick1.getButtons();

  // Use L3 (Left joystick button) to toggle Show Raw or not...
  if ((buttons & 0x4000) && !(buttons_prev & 0x4000)) show_raw_data = !show_raw_data;
  if ((buttons & 0x8000) && !(buttons_prev & 0x8000)) show_changed_data = !show_changed_data;

  if (show_raw_data)  {
    displayRawData();
  } else {
    Serial.printf("LX: %d, LY: %d, RX: %d, RY: %d \r\n", psAxis[0], psAxis[1], psAxis[2], psAxis[5]);
    Serial.printf("L-Trig: %d, R-Trig: %d\r\n", psAxis[3], psAxis[4]);
    Serial.printf("Buttons: %x\r\n", buttons);
  }
  uint8_t ltv;
  uint8_t rtv;

  ltv = psAxis[3];
  rtv = psAxis[4];

  if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
    joystick_left_trigger_value = ltv;
    joystick_right_trigger_value = rtv;
    Serial.printf("Rumbling: %d, %d\r\n", ltv, rtv);
    joystick1.setRumble(ltv, rtv);
  }

  if (buttons != buttons_prev) {
    uint8_t leds = 0;
    if (buttons & 0x8000) leds = 1;   //Srq
    if (buttons & 0x2000) leds = 2;   //Cir
    if (buttons & 0x1000) leds = 3;   //Tri
    //Cross = 2
    joystick1.setLEDs(leds);
    buttons_prev = buttons;
  }
}

void displayRawData() {
  uint64_t axis_mask = joystick1.axisMask();
  uint64_t changed_mask = joystick1.axisChangedMask();

  buttons = joystick1.getButtons();

  if (!changed_mask && (buttons == buttons_prev)) return;

  if (show_changed_data) {
    if (!changed_mask) return;
    changed_mask &= 0xfffffffffL; // try reducing which ones show...
    Serial.printf("%0x - ", joystick1.getButtons());

    for (uint16_t index = 0; changed_mask; index++) {
      if (changed_mask & 1) {
        Serial.printf("%d:%02x ", index, psAxis[index]);
      }
      changed_mask >>= 1;
    }

  } else {
    axis_mask &= 0xffffff;
    Serial.printf("%06x%06x: %06x - ", (uint32_t)(changed_mask >> 32), (uint32_t)(changed_mask & 0xffffffff), joystick1.getButtons());

    for (uint16_t index = 0; axis_mask; index++) {
      Serial.printf("%02x ", psAxis[index]);
      axis_mask >>= 1;
    }
  }
  Serial.println();
  buttons_prev = buttons;

}


bool OnReceiveHidData(uint32_t usage, const uint8_t *data, uint32_t len) {
  // Called for maybe both HIDS for rawhid basic test.  One is for the Teensy
  // to output to Serial. while still having Raw Hid...
  if (usage == 0xffc90004) {
    // Lets trim off trailing null characters.
    while ((len > 0) && (data[len - 1] == 0)) {
      len--;
    }
    if (len) {
      Serial.print("RawHid Serial: ");
      Serial.write(data, len);
    }
  } else {
    Serial.print("RawHID data: ");
    Serial.println(usage, HEX);
    while (len) {
      uint8_t cb = (len > 16) ? 16 : len;
      const uint8_t *p = data;
      uint8_t i;
      for (i = 0; i < cb; i++) {
        Serial.printf("%02x ", *p++);
      }
      Serial.print(": ");
      for (i = 0; i < cb; i++) {
        Serial.write(((*data >= ' ') && (*data <= '~')) ? *data : '.');
        data++;
      }
      len -= cb;
      Serial.println();
    }
  }

  return true;
}