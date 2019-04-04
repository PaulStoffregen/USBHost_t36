//=============================================================================
// Simple test of USB Host Mouse/Tablet/Joystick viewer on ili9341 display
//
// Currently requires the libraries
//    ili9341_t3n that can be located: https://github.com/KurtE/ILI9341_t3n
//    spin: https://github.com/KurtE/SPIN
//
// Teensy 3.6 Pins
//   8 = RST
//   9 = D/C
//  10 = CS
//
// Teensy 4.0 Beta Pins
//  23 = RST (Marked MCLK on T4 beta breakout)
//  10 = CS (Marked CS)
//  9 = DC  (Marked MEMCS)
//
// This example is in the public domain
//=============================================================================

#include "USBHost_t36.h"
#include <ili9341_t3n_font_Arial.h>

//=============================================================================
// Connection configuration of ILI9341 LCD TFT
//=============================================================================
#if defined(__MK66FX1M0__)
#define TFT_RST 8
#define TFT_DC 9
#define TFT_CS 10
#elif defined(__IMXRT1052__) || defined(__IMXRT1062__)
// On Teensy 4 beta with Paul's breakout out:
// Using pins (MOSI, MISO, SCK which are labeled on Audio board breakout location
// which are not in the Normal processor positions
// Also DC=10(CS), CS=9(BCLK) and RST 23(MCLK)
#define TFT_RST 23
#define TFT_DC 9
#define TFT_CS 10
#else
#error "This example App will only work with Teensy 3.6 or Teensy 4."
#endif
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST);

//=============================================================================
// USB Host Ojbects
//=============================================================================
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);
USBHIDParser hid4(myusb);
USBHIDParser hid5(myusb);
MouseController mouse(myusb);
DigitizerController tablet(myusb);
JoystickController joystick(myusb);
//BluetoothController bluet(myusb, true, "0000");   // Version does pairing to device
BluetoothController bluet(myusb);   // version assumes it already was paired
RawHIDController rawhid2(myusb);

// Lets only include in the lists The most top level type devices we wish to show information for.
USBDriver *drivers[] = {&joystick};
#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"Joystick(device)"};
bool driver_active[CNT_DEVICES] = {false};

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = {&tablet, &joystick, &mouse, &rawhid2};
#define CNT_HIDDEVICES (sizeof(hiddrivers)/sizeof(hiddrivers[0]))
const char * hid_driver_names[CNT_HIDDEVICES] = {"tablet", "joystick", "mouse", "RawHid2"};
bool hid_driver_active[CNT_HIDDEVICES] = {false, false};

BTHIDInput *bthiddrivers[] = {&joystick, &mouse};
#define CNT_BTHIDDEVICES (sizeof(bthiddrivers)/sizeof(bthiddrivers[0]))
const char * bthid_driver_names[CNT_HIDDEVICES] = {"joystick", "mouse"};
bool bthid_driver_active[CNT_HIDDEVICES] = {false, false};

//=============================================================================
// Other state variables.
//=============================================================================

// Save away values for buttons, x, y, wheel, wheelh
int buttons_cur = 0;
int x_cur = 0,
    y_cur = 0,
    z_cur = 0;
int x2_cur = 0,
    y2_cur = 0,
    z2_cur = 0;
int wheel_cur = 0;
int wheelH_cur = 0;
int axis_cur[10];


int user_axis[64];
uint32_t buttons_prev = 0;
uint32_t buttons;

bool show_changed_only = false;
bool new_device_detected = false;
int16_t y_position_after_device_info = 0;

uint8_t joystick_left_trigger_value = 0;
uint8_t joystick_right_trigger_value = 0;
uint64_t joystick_full_notify_mask = (uint64_t) - 1;

//=============================================================================
// Setup
//=============================================================================
void setup()
{
  Serial1.begin(2000000);
  while (!Serial && millis() < 3000) ; // wait for Arduino Serial Monitor
  Serial.println("\n\nUSB Host Testing");
  myusb.begin();
  rawhid2.attachReceive(OnReceiveHidData);


  tft.begin();
  delay(100);
  tft.setRotation(3); // 180
  delay(100);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(2);
  tft.println("Waiting for Mouse or Joystick...");
}


//=============================================================================
// Loop
//=============================================================================
void loop()
{
  myusb.Task();

  // Update the display with
  UpdateActiveDeviceInfo();

  // Now lets try displaying Tablet data
  ProcessTabletData();

  // And joystick data
  ProcessJoystickData();

  // Process Mouse Data
  ProcessMouseData();
}


//=============================================================================
// UpdateActiveDeviceInfo
//=============================================================================
void UpdateActiveDeviceInfo() {
  // First see if any high level devices
  for (uint8_t i = 0; i < CNT_DEVICES; i++) {
    if (*drivers[i] != driver_active[i]) {
      if (driver_active[i]) {
        Serial.printf("*** Device %s - disconnected ***\n", driver_names[i]);
        driver_active[i] = false;
      } else {
        new_device_detected = true;
        Serial.printf("*** Device %s %x:%x - connected ***\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
        driver_active[i] = true;
        tft.fillScreen(ILI9341_BLACK);  // clear the screen.
        tft.setCursor(0, 0);
        tft.setTextColor(ILI9341_YELLOW);
        tft.setFont(Arial_12);
        tft.printf("Device %s %x:%x\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());

        const uint8_t *psz = drivers[i]->manufacturer();
        if (psz && *psz) tft.printf("  manufacturer: %s\n", psz);
        psz = drivers[i]->product();
        if (psz && *psz) tft.printf("  product: %s\n", psz);
        psz = drivers[i]->serialNumber();
        if (psz && *psz) tft.printf("  Serial: %s\n", psz);
      }
    }
  }
  // Then Hid Devices
  for (uint8_t i = 0; i < CNT_HIDDEVICES; i++) {
    if (*hiddrivers[i] != hid_driver_active[i]) {
      if (hid_driver_active[i]) {
        Serial.printf("*** HID Device %s - disconnected ***\n", hid_driver_names[i]);
        hid_driver_active[i] = false;
      } else {
        new_device_detected = true;
        Serial.printf("*** HID Device %s %x:%x - connected ***\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
        hid_driver_active[i] = true;
        tft.fillScreen(ILI9341_BLACK);  // clear the screen.
        tft.setCursor(0, 0);
        tft.setTextColor(ILI9341_YELLOW);
        tft.setFont(Arial_12);
        tft.printf("HID Device %s %x:%x\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());

        const uint8_t *psz = hiddrivers[i]->manufacturer();
        if (psz && *psz) tft.printf("  manufacturer: %s\n", psz);
        psz = hiddrivers[i]->product();
        if (psz && *psz) tft.printf("  product: %s\n", psz);
        psz = hiddrivers[i]->serialNumber();
        if (psz && *psz) tft.printf("  Serial: %s\n", psz);
      }
    }
  }

  // Then Bluetooth devices
  for (uint8_t i = 0; i < CNT_BTHIDDEVICES; i++) {
    if (*bthiddrivers[i] != bthid_driver_active[i]) {
      if (bthid_driver_active[i]) {
        Serial.printf("*** BTHID Device %s - disconnected ***\n", hid_driver_names[i]);
        hid_driver_active[i] = false;
      } else {
        new_device_detected = true;
        Serial.printf("*** BTHID Device %s %x:%x - connected ***\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
        bthid_driver_active[i] = true;
        tft.fillScreen(ILI9341_BLACK);  // clear the screen.
        tft.setCursor(0, 0);
        tft.setTextColor(ILI9341_YELLOW);
        tft.setFont(Arial_12);
        tft.printf("Bluetooth Device %s %x:%x\n", bthid_driver_names[i], bthiddrivers[i]->idVendor(), bthiddrivers[i]->idProduct());

        const uint8_t *psz = bthiddrivers[i]->manufacturer();
        if (psz && *psz) tft.printf("  manufacturer: %s\n", psz);
        psz = bthiddrivers[i]->product();
        if (psz && *psz) tft.printf("  product: %s\n", psz);
        psz = bthiddrivers[i]->serialNumber();
        if (psz && *psz) tft.printf("  Serial: %s\n", psz);
      }
    }
  }
}

//=============================================================================
// ProcessTabletData
//=============================================================================
void ProcessTabletData() {
  if (tablet.available()) {
    if (new_device_detected) {
      // Lets display the titles.
      int16_t x;
      tft.getCursor(&x, &y_position_after_device_info);
      tft.setTextColor(ILI9341_YELLOW);
      tft.printf("Buttons:\nX:\nY:\nWheel:\nWheel H:\nAxis:");
      new_device_detected = false;
    }
    bool something_changed = false;
    if (tablet.getButtons() != buttons_cur) {
      buttons_cur = tablet.getButtons();
      something_changed = true;
    }
    if (tablet.getMouseX() != x_cur) {
      x_cur = tablet.getMouseX();
      something_changed = true;
    }
    if (tablet.getMouseY() != y_cur) {
      y_cur = tablet.getMouseY();
      something_changed = true;
    }
    if (tablet.getWheel() != wheel_cur) {
      wheel_cur = tablet.getWheel();
      something_changed = true;
    }
    if (tablet.getWheelH() != wheelH_cur) {
      wheelH_cur = tablet.getWheelH();
      something_changed = true;
    }
    // BUGBUG:: play with some Axis...
    for (uint8_t i = 0; i < 10; i++) {
      int axis = tablet.getAxis(i);
      if (axis != axis_cur[i]) {
        axis_cur[i] = axis;
        something_changed = true;
      }
    }

    if (something_changed) {
#define TABLET_DATA_X 100
      int16_t x, y2;
      unsigned char line_space = Arial_12.line_space;
      tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
      //tft.setTextDatum(BR_DATUM);
      int16_t y = y_position_after_device_info;
      tft.setCursor(TABLET_DATA_X, y);
      tft.printf("%d(%x)", buttons_cur, buttons_cur);
      tft.getCursor(&x, &y2);
      tft.fillRect(x, y, 320, line_space, ILI9341_BLACK);

      y += line_space; OutputNumberField(TABLET_DATA_X, y, x_cur, 320);
      y += line_space; OutputNumberField(TABLET_DATA_X, y, y_cur, 320);
      y += line_space; OutputNumberField(TABLET_DATA_X, y, wheel_cur, 320);
      y += line_space; OutputNumberField(TABLET_DATA_X, y, wheelH_cur, 320);

      // Output other Axis data
      for (uint8_t i = 0; i < 9; i += 3) {
        y += line_space;
        OutputNumberField(TABLET_DATA_X, y, axis_cur[i], 75);
        OutputNumberField(TABLET_DATA_X + 75, y, axis_cur[i + 1], 75);
        OutputNumberField(TABLET_DATA_X + 150, y, axis_cur[i + 2], 75);
      }

    }
    tablet.digitizerDataClear();
  }
}
//=============================================================================
// OutputNumberField
//=============================================================================
void OutputNumberField(int16_t x, int16_t y, int val, int16_t field_width) {
  int16_t x2, y2;
  tft.setCursor(x, y);
  tft.print(val, DEC); tft.getCursor(&x2, &y2);
  tft.fillRect(x2, y, field_width - (x2-x), Arial_12.line_space, ILI9341_BLACK);
}

//=============================================================================
// ProcessMouseData
//=============================================================================
void ProcessMouseData() {
  if (mouse.available()) {
    if (new_device_detected) {
      // Lets display the titles.
      int16_t x;
      tft.getCursor(&x, &y_position_after_device_info);
      tft.setTextColor(ILI9341_YELLOW);
      tft.printf("Buttons:\nX:\nY:\nWheel:\nWheel H:");
      new_device_detected = false;
    }

    bool something_changed = false;
    if (mouse.getButtons() != buttons_cur) {
      buttons_cur = mouse.getButtons();
      something_changed = true;
    }
    if (mouse.getMouseX() != x_cur) {
      x_cur = mouse.getMouseX();
      something_changed = true;
    }
    if (mouse.getMouseY() != y_cur) {
      y_cur = mouse.getMouseY();
      something_changed = true;
    }
    if (mouse.getWheel() != wheel_cur) {
      wheel_cur = mouse.getWheel();
      something_changed = true;
    }
    if (mouse.getWheelH() != wheelH_cur) {
      wheelH_cur = mouse.getWheelH();
      something_changed = true;
    }
    if (something_changed) {
#define MOUSE_DATA_X 100
      int16_t x, y2;
      unsigned char line_space = Arial_12.line_space;
      tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
      //tft.setTextDatum(BR_DATUM);
      int16_t y = y_position_after_device_info;
      tft.setCursor(TABLET_DATA_X, y);
      tft.printf("%d(%x)", buttons_cur, buttons_cur);
      tft.getCursor(&x, &y2);
      tft.fillRect(x, y, 320, line_space, ILI9341_BLACK);

      y += line_space; OutputNumberField(MOUSE_DATA_X, y, x_cur, 320);
      y += line_space; OutputNumberField(MOUSE_DATA_X, y, y_cur, 320);
      y += line_space; OutputNumberField(MOUSE_DATA_X, y, wheel_cur, 320);
      y += line_space; OutputNumberField(MOUSE_DATA_X, y, wheelH_cur, 320);
    }

    mouse.mouseDataClear();
  }
}

//=============================================================================
// ProcessJoystickData
//=============================================================================
void ProcessJoystickData() {
  if (joystick.available()) {
    uint64_t axis_mask = joystick.axisMask();
    uint64_t axis_changed_mask = joystick.axisChangedMask();
    Serial.print("Joystick: buttons = ");
    buttons = joystick.getButtons();
    Serial.print(buttons, HEX);
    //Serial.printf(" AMasks: %x %x:%x", axis_mask, (uint32_t)(user_axis_mask >> 32), (uint32_t)(user_axis_mask & 0xffffffff));
    //Serial.printf(" M: %lx %lx", axis_mask, joystick.axisChangedMask());
    if (show_changed_only) {
      for (uint8_t i = 0; axis_changed_mask != 0; i++, axis_changed_mask >>= 1) {
        if (axis_changed_mask & 1) {
          Serial.printf(" %d:%d", i, joystick.getAxis(i));
        }
      }
    } else {
      for (uint8_t i = 0; axis_mask != 0; i++, axis_mask >>= 1) {
        if (axis_mask & 1) {
          Serial.printf(" %d:%d", i, joystick.getAxis(i));
        }
      }
    }
    for (uint8_t i = 0; i < 64; i++) {
      user_axis[i] = joystick.getAxis(i);
    }
    uint8_t ltv;
    uint8_t rtv;
    switch (joystick.joystickType()) {
      default:
        break;
      case JoystickController::PS4:
        ltv = joystick.getAxis(3);
        rtv = joystick.getAxis(4);
        if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
          joystick_left_trigger_value = ltv;
          joystick_right_trigger_value = rtv;
          joystick.setRumble(ltv, rtv);
        }
        break;

      case JoystickController::PS3:
        ltv = joystick.getAxis(18);
        rtv = joystick.getAxis(19);
        if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
          joystick_left_trigger_value = ltv;
          joystick_right_trigger_value = rtv;
          joystick.setRumble(ltv, rtv, 50);
        }
        break;

      case JoystickController::XBOXONE:
      case JoystickController::XBOX360:
        ltv = joystick.getAxis(4);
        rtv = joystick.getAxis(5);
        if ((ltv != joystick_left_trigger_value) || (rtv != joystick_right_trigger_value)) {
          joystick_left_trigger_value = ltv;
          joystick_right_trigger_value = rtv;
          joystick.setRumble(ltv, rtv);
          Serial.printf(" Set Rumble %d %d", ltv, rtv);
        }
        break;
    }
    if (buttons != buttons_cur) {
      if (joystick.joystickType() == JoystickController::PS3) {
        joystick.setLEDs((buttons >> 12) & 0xf); //  try to get to TRI/CIR/X/SQuare
      } else {
        uint8_t lr = (buttons & 1) ? 0xff : 0;
        uint8_t lg = (buttons & 2) ? 0xff : 0;
        uint8_t lb = (buttons & 4) ? 0xff : 0;
        joystick.setLEDs(lr, lg, lb);
      }
      buttons_cur = buttons;
    }
    Serial.println();
    TFT_joystick();
    joystick.joystickDataClear();
  }
}


//=============================================================================
// TFT_joystick
//=============================================================================
void TFT_joystick()
{
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.drawString("BUTTONS", 100, 100);
  if (buttons == 0) {
    tft.drawString(".........", 180, 100);
    tft.drawNumber(buttons_prev, 180, 100);
  } else {
    tft.drawNumber(buttons, 180, 100);
  }

  tft.drawString("X: ", 60, 120);
  if (user_axis[0] == x_cur) {
    tft.drawString(".........", 80, 120);
    tft.drawNumber(x_cur, 80, 120);
  } else {
    tft.drawNumber(user_axis[0], 80, 120);
  }
  x_cur = user_axis[0];

  tft.drawString("Y: ", 60, 140);
  if (user_axis[1] == y_cur) {
    tft.drawString(".........", 80, 140);
    tft.drawNumber(y_cur, 80, 140);
  } else {
    tft.drawNumber(user_axis[1], 80, 140);
  }
  y_cur = user_axis[1];

  tft.drawString("Hat: ", 40, 180);
  if (user_axis[9] == wheel_cur) {
    tft.drawString(".........", 80, 180);
    tft.drawNumber(wheel_cur, 80, 180);
  } else {
    tft.drawNumber(user_axis[9], 80, 180);
  }
  wheel_cur = user_axis[9];

  //Gamepads
  tft.drawString("X2: ", 160, 120);
  if (user_axis[2] == x_cur) {
    tft.drawString(".........", 190, 120);
    tft.drawNumber(x_cur, 190, 120);
  } else {
    tft.drawNumber(user_axis[2], 190, 120);
  }
  x2_cur = user_axis[2];


  switch (joystick.joystickType()) {
    default:
      tft.drawString("Z: ", 60, 160);
      if (user_axis[5] == z_cur) {
        tft.drawString(".........", 80, 160);
        tft.drawNumber(z_cur, 80, 140);
      } else {
        tft.drawNumber(user_axis[5], 80, 160);
      }
      z_cur = user_axis[5];
      break;
    case JoystickController::PS4:
      tft.drawString("Y2: ", 160, 140);
      if (user_axis[5] == y2_cur) {
        tft.drawString(".........", 190, 140);
        tft.drawNumber(y2_cur, 190, 120);
      } else {
        tft.drawNumber(user_axis[5], 190, 140);
      }
      y2_cur = user_axis[5];
      break;
    case JoystickController::PS3:
      tft.drawString("Y2: ", 160, 140);
      if (user_axis[5] == y2_cur) {
        tft.drawString(".........", 190, 140);
        tft.drawNumber(y2_cur, 190, 140);
      } else {
        tft.drawNumber(user_axis[5], 190, 140);
      }
      y2_cur = user_axis[5];
      break;
    case JoystickController::XBOXONE:
    case JoystickController::XBOX360:
      tft.drawString("Y2: ", 160, 160);
      if (user_axis[5] == y2_cur) {
        tft.drawString(".........", 190, 160);
        tft.drawNumber(y2_cur, 190, 160);
      } else {
        tft.drawNumber(user_axis[5], 190, 160);
      }
      y2_cur = user_axis[5];
      break;
  }
}

//=============================================================================
// ProcessMouseData
//=============================================================================
bool OnReceiveHidData(uint32_t usage, const uint8_t *data, uint32_t len) {
  // Called for maybe both HIDS for rawhid basic test.  One is for the Teensy
  // to output to Serial. while still having Raw Hid...
  if (usage == 0xffc90004) {
    // Lets trim off trailing null characters.
    while ((len > 0) && (data[len - 1] == 0)) {
      len--;
    }
    if (len) {
      //Serial.print("RawHid Serial: ");
      //Serial.write(data, len);
    }
  } else {
    //Serial.print("RawHIDx data: ");
    //Serial.println(usage, HEX);
    uint8_t len1 = len;

    for (int j = 0; j < len; j++) {
      user_axis[j] = data[j];
    }
    /*
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
    */
  }

  //for (uint8_t i = 0; i<9; i++) {
  //Serial.printf(" %d:%d", i, user_axis[i]);
  //}
  //Serial.println();
  //Serial.print("Mouse: buttons = ");
  //Serial.print(user_axis[3]);
  //Serial.print(",  mouseX = ");
  //Serial.print(user_axis[4]);
  //Serial.print(",  mouseY = ");
  //Serial.print(user_axis[5]);
  //Serial.println();

  bool something_changed = false;
  if (user_axis[3] != buttons_cur) {
    buttons_cur = user_axis[3];
    something_changed = true;
  }
  if (user_axis[4] != x_cur) {
    x_cur = user_axis[4];
    something_changed = true;
  }
  if (user_axis[5] != y_cur) {
    y_cur = user_axis[5];
    something_changed = true;
  }
  if (tablet.getWheel() != wheel_cur) {
    wheel_cur = 0;
    something_changed = true;
  }
  if (tablet.getWheelH() != wheelH_cur) {
    wheelH_cur = 0;
    something_changed = true;
  }
  if (something_changed) {
    tft.fillRect(45, 197, 240, 20, ILI9341_RED);
    tft.drawNumber(buttons_cur, 50, 200);
    tft.drawNumber(x_cur, 100, 200);
    tft.drawNumber(y_cur, 150, 200);
    tft.drawNumber(wheel_cur, 200, 200);
    tft.drawNumber(wheelH_cur, 250, 200);
  }
  return true;
}
