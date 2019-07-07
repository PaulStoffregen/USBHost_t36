//=============================================================================
// Simple test viewer app for several of the USB devices on ili9341 display
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
#define TEENSY64

//=============================================================================
// Connection configuration of ILI9341 LCD TFT
//=============================================================================
DMAMEM uint16_t frame_buffer[ILI9341_TFTWIDTH * ILI9341_TFTHEIGHT];
#if defined(__MK66FX1M0__) && !defined(TEENSY64)
#define TFT_RST 255
#define TFT_DC 20
#define TFT_CS 21
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST);
#elif defined(__IMXRT1052__) || defined(__IMXRT1062__)
// On Teensy 4 beta with Paul's breakout out:
// Using pins (MOSI, MISO, SCK which are labeled on Audio board breakout location
// which are not in the Normal processor positions
// Also DC=10(CS), CS=9(BCLK) and RST 23(MCLK)
#define TFT_RST 23
#define TFT_DC 9
#define TFT_CS 10
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST);
#elif defined(TEENSY64)
#define TFT_RST 255
#define TFT_DC 20
#define TFT_CS 21
#define TFT_SCK 14
#define TFT_MISO 39
#define TFT_MOSI 28
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCK, TFT_MISO);
#else
#error "This example App will only work with Teensy 3.6 or Teensy 4."
#endif

//=============================================================================
// USB Host Ojbects
//=============================================================================
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
KeyboardController keyboard1(myusb);
KeyboardController keyboard2(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);
USBHIDParser hid4(myusb);
USBHIDParser hid5(myusb);
MouseController mouse(myusb);
DigitizerController tablet(myusb);
JoystickController joystick(myusb);
BluetoothController bluet(myusb, true, "0000");   // Version does pairing to device
//BluetoothController bluet(myusb);   // version assumes it already was paired
RawHIDController rawhid2(myusb);

// Lets only include in the lists The most top level type devices we wish to show information for.
//USBDriver *drivers[] = {&keyboard1, &keyboard2, &joystick};
USBDriver *drivers[] = {&keyboard1, &keyboard2, &joystick, &bluet, &hid1, &hid2};

#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
//const char * driver_names[CNT_DEVICES] = {"KB1", "KB2", "Joystick(device)"};
const char * driver_names[CNT_DEVICES] = {"KB1", "KB2", "Joystick(device)", "Bluet", "HID1" , "HID2"};
//bool driver_active[CNT_DEVICES] = {false, false, false};
bool driver_active[CNT_DEVICES] = {false, false, false, false, false};

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = {&tablet, &joystick, &mouse, &rawhid2};
#define CNT_HIDDEVICES (sizeof(hiddrivers)/sizeof(hiddrivers[0]))
const char * hid_driver_names[CNT_HIDDEVICES] = {"tablet", "joystick", "mouse", "RawHid2"};

bool hid_driver_active[CNT_HIDDEVICES] = {false, false, false, false};

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
    z2_cur = 0,
    L1_cur = 0,
    R1_cur = 0;
int wheel_cur = 0;
int wheelH_cur = 0;
int axis_cur[10];

String p = "KB1";
bool BT = 0;

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
  keyboard1.attachPress(OnPress);
  keyboard2.attachPress(OnPress);
  keyboard1.attachExtrasPress(OnHIDExtrasPress);
  keyboard1.attachExtrasRelease(OnHIDExtrasRelease);
  keyboard2.attachExtrasPress(OnHIDExtrasPress);
  keyboard2.attachExtrasRelease(OnHIDExtrasRelease);

  // The below forceBootProtocol will force which ever
  // next keyboard that attaches to this device to be in boot protocol
  // Only try this if you run into keyboard with issues.  If this is a combined
  // device like wireless mouse and keyboard this can cause mouse problems.
  //keyboard1.forceBootProtocol();

  tft.begin();
  // explicitly set the frame buffer
  tft.setFrameBuffer(frame_buffer);
  delay(100);
  tft.setRotation(3); // 180
  delay(100);

  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(2);
  tft.println("Waiting for Device...");
  tft.useFrameBuffer(true);
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
        tft.updateScreen(); // update the screen now
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
        tft.updateScreen(); // update the screen now
      }
    }
  }

  // Then Bluetooth devices
  for (uint8_t i = 0; i < CNT_BTHIDDEVICES; i++) {
    if (*bthiddrivers[i] != bthid_driver_active[i]) {
      if (bthid_driver_active[i]) {
        Serial.printf("*** BTHID Device %s - disconnected ***\n", hid_driver_names[i]);
        bthid_driver_active[i] = false;
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
        tft.updateScreen(); // update the screen now
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
      tft.updateScreen(); // update the screen now

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
  tft.fillRect(x2, y, field_width - (x2 - x), Arial_12.line_space, ILI9341_BLACK);
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
      tft.updateScreen(); // update the screen now
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
    tft_JoystickData();
    joystick.joystickDataClear();
  }
}


//=============================================================================
// TFT_joystick
//=============================================================================
void tft_JoystickData() {
    if (new_device_detected) {
      // Lets display the titles.
      int16_t x;
      tft.getCursor(&x, &y_position_after_device_info);
      tft.setTextColor(ILI9341_YELLOW);
      tft.printf("Buttons:\nX:\nY:\nX2\nY2(Z):\nL1:\nR1:\nHAT:");
      new_device_detected = false;
    }

    bool something_changed = false;
    if (buttons != buttons_prev) {  //buttons
      something_changed = true;
    }
    if (user_axis[0] != x_cur) {  //xL
      x_cur = user_axis[0];
      something_changed = true;
    }
    if (user_axis[1] != y_cur) {  //yL
      y_cur = user_axis[1];
      something_changed = true;
    }
    if (user_axis[9] != wheel_cur) {  //Hat
      wheel_cur = user_axis[9];
      something_changed = true;
    }
  //Second Axis
    if (user_axis[2] != x2_cur) {  //xR
      x2_cur = user_axis[2];
      something_changed = true;
    }
    if (user_axis[5] != y2_cur) {  //yR or z-axis
      y2_cur = user_axis[5];
      something_changed = true;
    } 
    //Rumble Axis
     switch (joystick.joystickType()) {
        case JoystickController::XBOXONE:
        case JoystickController::XBOX360:
        case JoystickController::PS4:
          if (user_axis[3] != L1_cur) {  //xR
            L1_cur = user_axis[3];
            something_changed = true;
          }
          if (user_axis[4] != R1_cur) {  //yR or z-axis
            R1_cur = user_axis[4];
            something_changed = true;
          }
          break;
      case JoystickController::PS3:
          if (user_axis[18] != L1_cur) {  //xR
            L1_cur = user_axis[18];
            something_changed = true;
          }
          if (user_axis[19] != R1_cur) {  //yR or z-axis
            R1_cur = user_axis[19];
            something_changed = true;
          }
          break;
     }
    if (something_changed) {
#define MOUSE_DATA_X 100
      int16_t x, y2;
      unsigned char line_space = Arial_12.line_space;
      tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
      //tft.setTextDatum(BR_DATUM);
      int16_t y = y_position_after_device_info;
      tft.setCursor(TABLET_DATA_X, y);
      tft.printf("%d(%x)", buttons, buttons);
      tft.getCursor(&x, &y2);
      tft.fillRect(x, y, 320, line_space, ILI9341_BLACK);

      y += line_space; OutputNumberField(MOUSE_DATA_X, y, x_cur, 320);  //x
      y += line_space; OutputNumberField(MOUSE_DATA_X, y, y_cur, 320);  //y
      y += line_space; OutputNumberField(MOUSE_DATA_X, y, x2_cur, 320); //x2(z)
      y += line_space; OutputNumberField(MOUSE_DATA_X, y, y2_cur, 320); //y2
      switch (joystick.joystickType()) {
        case JoystickController::PS4:
        case JoystickController::PS3:
        case JoystickController::XBOXONE:
        case JoystickController::XBOX360:
          y += line_space; OutputNumberField(MOUSE_DATA_X, y, L1_cur, 320); 
          y += line_space; OutputNumberField(MOUSE_DATA_X, y, R1_cur, 320);
          break;
        default:
          y += line_space; OutputNumberField(MOUSE_DATA_X, y, 0, 320); 
          y += line_space; OutputNumberField(MOUSE_DATA_X, y, 0, 320);
          y += line_space; OutputNumberField(MOUSE_DATA_X, y, wheel_cur, 320); //hat
          break;
      }
      tft.updateScreen(); // update the screen now
    }
}

//=============================================================================
// ProcessMouseData
//=============================================================================
bool OnReceiveHidData(uint32_t usage, const uint8_t *data, uint32_t len) {
  // Called for maybe both HIDS for rawhid basic test.  One is for the Teensy
  // to output to Serial. while still having Raw Hid...
  if (usage == 0xFF000080
     ) {
    //Serial.print("RawHIDx data: ");
    //Serial.println(usage, HEX);
    for (uint32_t j = 0; j < len; j++) {
      user_axis[j] = data[j];
    }

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
      tft.updateScreen(); // update the screen now
    }
  } else {
    // Lets trim off trailing null characters.
    while ((len > 0) && (data[len - 1] == 0)) {
      len--;
    }
    if (len) {
      //Serial.print("RawHid Serial: ");
      //Serial.write(data, len);
    }
  }
  return true;
}

void MaybeSetupTextScrollArea()
{
  if (new_device_detected) {
    BT = 0;
    new_device_detected = false;
  }
  if (BT == 0) {
    tft.enableScroll();
    tft.setScrollTextArea(20, 70, 280, 140);
    tft.setScrollBackgroundColor(ILI9341_GREEN);
    tft.setFont(Arial_11);
    tft.setTextColor(ILI9341_BLACK);
    tft.setCursor(20, 70);
    BT = 1;
  }
}

void OnPress(int key)
{
  MaybeSetupTextScrollArea();
  tft.print("key: ");
  switch (key) {
    case KEYD_UP       : tft.print("UP"); break;
    case KEYD_DOWN    : tft.print("DN"); break;
    case KEYD_LEFT     : tft.print("LEFT"); break;
    case KEYD_RIGHT   : tft.print("RIGHT"); break;
    case KEYD_INSERT   : tft.print("Ins"); break;
    case KEYD_DELETE   : tft.print("Del"); break;
    case KEYD_PAGE_UP  : tft.print("PUP"); break;
    case KEYD_PAGE_DOWN: tft.print("PDN"); break;
    case KEYD_HOME     : tft.print("HOME"); break;
    case KEYD_END      : tft.print("END"); break;
    case KEYD_F1       : tft.print("F1"); break;
    case KEYD_F2       : tft.print("F2"); break;
    case KEYD_F3       : tft.print("F3"); break;
    case KEYD_F4       : tft.print("F4"); break;
    case KEYD_F5       : tft.print("F5"); break;
    case KEYD_F6       : tft.print("F6"); break;
    case KEYD_F7       : tft.print("F7"); break;
    case KEYD_F8       : tft.print("F8"); break;
    case KEYD_F9       : tft.print("F9"); break;
    case KEYD_F10      : tft.print("F10"); break;
    case KEYD_F11      : tft.print("F11"); break;
    case KEYD_F12      : tft.print("F12"); break;
    default: tft.print((char)key); break;
  }
  tft.print("'  ");
  tft.print(key);
  tft.print(" MOD: ");
  if (keyboard1) {
    tft.print(keyboard1.getModifiers(), HEX);
    tft.print(" OEM: ");
    tft.print(keyboard1.getOemKey(), HEX);
    tft.print(" LEDS: ");
    tft.println(keyboard1.LEDS(), HEX);
  } else {
    tft.print(keyboard2.getModifiers(), HEX);
    tft.print(" OEM: ");
    tft.print(keyboard2.getOemKey(), HEX);
    tft.print(" LEDS: ");
    tft.println(keyboard2.LEDS(), HEX);
  }

  //Serial.print("key ");
  //Serial.print((char)keyboard1.getKey());
  //Serial.print("  ");
  //Serial.print((char)keyboard2.getKey());
  //Serial.println();
  tft.updateScreen(); // update the screen now
}

void OnHIDExtrasPress(uint32_t top, uint16_t key)
{
  MaybeSetupTextScrollArea();
  tft.print("HID (");
  tft.print(top, HEX);
  tft.print(") key press:");
  tft.print(key, HEX);
  if (top == 0xc0000) {
    switch (key) {
      case  0x20 : tft.print(" - +10"); break;
      case  0x21 : tft.print(" - +100"); break;
      case  0x22 : tft.print(" - AM/PM"); break;
      case  0x30 : tft.print(" - Power"); break;
      case  0x31 : tft.print(" - Reset"); break;
      case  0x32 : tft.print(" - Sleep"); break;
      case  0x33 : tft.print(" - Sleep After"); break;
      case  0x34 : tft.print(" - Sleep Mode"); break;
      case  0x35 : tft.print(" - Illumination"); break;
      case  0x36 : tft.print(" - Function Buttons"); break;
      case  0x40 : tft.print(" - Menu"); break;
      case  0x41 : tft.print(" - Menu  Pick"); break;
      case  0x42 : tft.print(" - Menu Up"); break;
      case  0x43 : tft.print(" - Menu Down"); break;
      case  0x44 : tft.print(" - Menu Left"); break;
      case  0x45 : tft.print(" - Menu Right"); break;
      case  0x46 : tft.print(" - Menu Escape"); break;
      case  0x47 : tft.print(" - Menu Value Increase"); break;
      case  0x48 : tft.print(" - Menu Value Decrease"); break;
      case  0x60 : tft.print(" - Data On Screen"); break;
      case  0x61 : tft.print(" - Closed Caption"); break;
      case  0x62 : tft.print(" - Closed Caption Select"); break;
      case  0x63 : tft.print(" - VCR/TV"); break;
      case  0x64 : tft.print(" - Broadcast Mode"); break;
      case  0x65 : tft.print(" - Snapshot"); break;
      case  0x66 : tft.print(" - Still"); break;
      case  0x80 : tft.print(" - Selection"); break;
      case  0x81 : tft.print(" - Assign Selection"); break;
      case  0x82 : tft.print(" - Mode Step"); break;
      case  0x83 : tft.print(" - Recall Last"); break;
      case  0x84 : tft.print(" - Enter Channel"); break;
      case  0x85 : tft.print(" - Order Movie"); break;
      case  0x86 : tft.print(" - Channel"); break;
      case  0x87 : tft.print(" - Media Selection"); break;
      case  0x88 : tft.print(" - Media Select Computer"); break;
      case  0x89 : tft.print(" - Media Select TV"); break;
      case  0x8A : tft.print(" - Media Select WWW"); break;
      case  0x8B : tft.print(" - Media Select DVD"); break;
      case  0x8C : tft.print(" - Media Select Telephone"); break;
      case  0x8D : tft.print(" - Media Select Program Guide"); break;
      case  0x8E : tft.print(" - Media Select Video Phone"); break;
      case  0x8F : tft.print(" - Media Select Games"); break;
      case  0x90 : tft.print(" - Media Select Messages"); break;
      case  0x91 : tft.print(" - Media Select CD"); break;
      case  0x92 : tft.print(" - Media Select VCR"); break;
      case  0x93 : tft.print(" - Media Select Tuner"); break;
      case  0x94 : tft.print(" - Quit"); break;
      case  0x95 : tft.print(" - Help"); break;
      case  0x96 : tft.print(" - Media Select Tape"); break;
      case  0x97 : tft.print(" - Media Select Cable"); break;
      case  0x98 : tft.print(" - Media Select Satellite"); break;
      case  0x99 : tft.print(" - Media Select Security"); break;
      case  0x9A : tft.print(" - Media Select Home"); break;
      case  0x9B : tft.print(" - Media Select Call"); break;
      case  0x9C : tft.print(" - Channel Increment"); break;
      case  0x9D : tft.print(" - Channel Decrement"); break;
      case  0x9E : tft.print(" - Media Select SAP"); break;
      case  0xA0 : tft.print(" - VCR Plus"); break;
      case  0xA1 : tft.print(" - Once"); break;
      case  0xA2 : tft.print(" - Daily"); break;
      case  0xA3 : tft.print(" - Weekly"); break;
      case  0xA4 : tft.print(" - Monthly"); break;
      case  0xB0 : tft.print(" - Play"); break;
      case  0xB1 : tft.print(" - Pause"); break;
      case  0xB2 : tft.print(" - Record"); break;
      case  0xB3 : tft.print(" - Fast Forward"); break;
      case  0xB4 : tft.print(" - Rewind"); break;
      case  0xB5 : tft.print(" - Scan Next Track"); break;
      case  0xB6 : tft.print(" - Scan Previous Track"); break;
      case  0xB7 : tft.print(" - Stop"); break;
      case  0xB8 : tft.print(" - Eject"); break;
      case  0xB9 : tft.print(" - Random Play"); break;
      case  0xBA : tft.print(" - Select DisC"); break;
      case  0xBB : tft.print(" - Enter Disc"); break;
      case  0xBC : tft.print(" - Repeat"); break;
      case  0xBD : tft.print(" - Tracking"); break;
      case  0xBE : tft.print(" - Track Normal"); break;
      case  0xBF : tft.print(" - Slow Tracking"); break;
      case  0xC0 : tft.print(" - Frame Forward"); break;
      case  0xC1 : tft.print(" - Frame Back"); break;
      case  0xC2 : tft.print(" - Mark"); break;
      case  0xC3 : tft.print(" - Clear Mark"); break;
      case  0xC4 : tft.print(" - Repeat From Mark"); break;
      case  0xC5 : tft.print(" - Return To Mark"); break;
      case  0xC6 : tft.print(" - Search Mark Forward"); break;
      case  0xC7 : tft.print(" - Search Mark Backwards"); break;
      case  0xC8 : tft.print(" - Counter Reset"); break;
      case  0xC9 : tft.print(" - Show Counter"); break;
      case  0xCA : tft.print(" - Tracking Increment"); break;
      case  0xCB : tft.print(" - Tracking Decrement"); break;
      case  0xCD : tft.print(" - Pause/Continue"); break;
      case  0xE0 : tft.print(" - Volume"); break;
      case  0xE1 : tft.print(" - Balance"); break;
      case  0xE2 : tft.print(" - Mute"); break;
      case  0xE3 : tft.print(" - Bass"); break;
      case  0xE4 : tft.print(" - Treble"); break;
      case  0xE5 : tft.print(" - Bass Boost"); break;
      case  0xE6 : tft.print(" - Surround Mode"); break;
      case  0xE7 : tft.print(" - Loudness"); break;
      case  0xE8 : tft.print(" - MPX"); break;
      case  0xE9 : tft.print(" - Volume Up"); break;
      case  0xEA : tft.print(" - Volume Down"); break;
      case  0xF0 : tft.print(" - Speed Select"); break;
      case  0xF1 : tft.print(" - Playback Speed"); break;
      case  0xF2 : tft.print(" - Standard Play"); break;
      case  0xF3 : tft.print(" - Long Play"); break;
      case  0xF4 : tft.print(" - Extended Play"); break;
      case  0xF5 : tft.print(" - Slow"); break;
      case  0x100: tft.print(" - Fan Enable"); break;
      case  0x101: tft.print(" - Fan Speed"); break;
      case  0x102: tft.print(" - Light"); break;
      case  0x103: tft.print(" - Light Illumination Level"); break;
      case  0x104: tft.print(" - Climate Control Enable"); break;
      case  0x105: tft.print(" - Room Temperature"); break;
      case  0x106: tft.print(" - Security Enable"); break;
      case  0x107: tft.print(" - Fire Alarm"); break;
      case  0x108: tft.print(" - Police Alarm"); break;
      case  0x150: tft.print(" - Balance Right"); break;
      case  0x151: tft.print(" - Balance Left"); break;
      case  0x152: tft.print(" - Bass Increment"); break;
      case  0x153: tft.print(" - Bass Decrement"); break;
      case  0x154: tft.print(" - Treble Increment"); break;
      case  0x155: tft.print(" - Treble Decrement"); break;
      case  0x160: tft.print(" - Speaker System"); break;
      case  0x161: tft.print(" - Channel Left"); break;
      case  0x162: tft.print(" - Channel Right"); break;
      case  0x163: tft.print(" - Channel Center"); break;
      case  0x164: tft.print(" - Channel Front"); break;
      case  0x165: tft.print(" - Channel Center Front"); break;
      case  0x166: tft.print(" - Channel Side"); break;
      case  0x167: tft.print(" - Channel Surround"); break;
      case  0x168: tft.print(" - Channel Low Frequency Enhancement"); break;
      case  0x169: tft.print(" - Channel Top"); break;
      case  0x16A: tft.print(" - Channel Unknown"); break;
      case  0x170: tft.print(" - Sub-channel"); break;
      case  0x171: tft.print(" - Sub-channel Increment"); break;
      case  0x172: tft.print(" - Sub-channel Decrement"); break;
      case  0x173: tft.print(" - Alternate Audio Increment"); break;
      case  0x174: tft.print(" - Alternate Audio Decrement"); break;
      case  0x180: tft.print(" - Application Launch Buttons"); break;
      case  0x181: tft.print(" - AL Launch Button Configuration Tool"); break;
      case  0x182: tft.print(" - AL Programmable Button Configuration"); break;
      case  0x183: tft.print(" - AL Consumer Control Configuration"); break;
      case  0x184: tft.print(" - AL Word Processor"); break;
      case  0x185: tft.print(" - AL Text Editor"); break;
      case  0x186: tft.print(" - AL Spreadsheet"); break;
      case  0x187: tft.print(" - AL Graphics Editor"); break;
      case  0x188: tft.print(" - AL Presentation App"); break;
      case  0x189: tft.print(" - AL Database App"); break;
      case  0x18A: tft.print(" - AL Email Reader"); break;
      case  0x18B: tft.print(" - AL Newsreader"); break;
      case  0x18C: tft.print(" - AL Voicemail"); break;
      case  0x18D: tft.print(" - AL Contacts/Address Book"); break;
      case  0x18E: tft.print(" - AL Calendar/Schedule"); break;
      case  0x18F: tft.print(" - AL Task/Project Manager"); break;
      case  0x190: tft.print(" - AL Log/Journal/Timecard"); break;
      case  0x191: tft.print(" - AL Checkbook/Finance"); break;
      case  0x192: tft.print(" - AL Calculator"); break;
      case  0x193: tft.print(" - AL A/V Capture/Playback"); break;
      case  0x194: tft.print(" - AL Local Machine Browser"); break;
      case  0x195: tft.print(" - AL LAN/WAN Browser"); break;
      case  0x196: tft.print(" - AL Internet Browser"); break;
      case  0x197: tft.print(" - AL Remote Networking/ISP Connect"); break;
      case  0x198: tft.print(" - AL Network Conference"); break;
      case  0x199: tft.print(" - AL Network Chat"); break;
      case  0x19A: tft.print(" - AL Telephony/Dialer"); break;
      case  0x19B: tft.print(" - AL Logon"); break;
      case  0x19C: tft.print(" - AL Logoff"); break;
      case  0x19D: tft.print(" - AL Logon/Logoff"); break;
      case  0x19E: tft.print(" - AL Terminal Lock/Screensaver"); break;
      case  0x19F: tft.print(" - AL Control Panel"); break;
      case  0x1A0: tft.print(" - AL Command Line Processor/Run"); break;
      case  0x1A1: tft.print(" - AL Process/Task Manager"); break;
      case  0x1A2: tft.print(" - AL Select Tast/Application"); break;
      case  0x1A3: tft.print(" - AL Next Task/Application"); break;
      case  0x1A4: tft.print(" - AL Previous Task/Application"); break;
      case  0x1A5: tft.print(" - AL Preemptive Halt Task/Application"); break;
      case  0x200: tft.print(" - Generic GUI Application Controls"); break;
      case  0x201: tft.print(" - AC New"); break;
      case  0x202: tft.print(" - AC Open"); break;
      case  0x203: tft.print(" - AC Close"); break;
      case  0x204: tft.print(" - AC Exit"); break;
      case  0x205: tft.print(" - AC Maximize"); break;
      case  0x206: tft.print(" - AC Minimize"); break;
      case  0x207: tft.print(" - AC Save"); break;
      case  0x208: tft.print(" - AC Print"); break;
      case  0x209: tft.print(" - AC Properties"); break;
      case  0x21A: tft.print(" - AC Undo"); break;
      case  0x21B: tft.print(" - AC Copy"); break;
      case  0x21C: tft.print(" - AC Cut"); break;
      case  0x21D: tft.print(" - AC Paste"); break;
      case  0x21E: tft.print(" - AC Select All"); break;
      case  0x21F: tft.print(" - AC Find"); break;
      case  0x220: tft.print(" - AC Find and Replace"); break;
      case  0x221: tft.print(" - AC Search"); break;
      case  0x222: tft.print(" - AC Go To"); break;
      case  0x223: tft.print(" - AC Home"); break;
      case  0x224: tft.print(" - AC Back"); break;
      case  0x225: tft.print(" - AC Forward"); break;
      case  0x226: tft.print(" - AC Stop"); break;
      case  0x227: tft.print(" - AC Refresh"); break;
      case  0x228: tft.print(" - AC Previous Link"); break;
      case  0x229: tft.print(" - AC Next Link"); break;
      case  0x22A: tft.print(" - AC Bookmarks"); break;
      case  0x22B: tft.print(" - AC History"); break;
      case  0x22C: tft.print(" - AC Subscriptions"); break;
      case  0x22D: tft.print(" - AC Zoom In"); break;
      case  0x22E: tft.print(" - AC Zoom Out"); break;
      case  0x22F: tft.print(" - AC Zoom"); break;
      case  0x230: tft.print(" - AC Full Screen View"); break;
      case  0x231: tft.print(" - AC Normal View"); break;
      case  0x232: tft.print(" - AC View Toggle"); break;
      case  0x233: tft.print(" - AC Scroll Up"); break;
      case  0x234: tft.print(" - AC Scroll Down"); break;
      case  0x235: tft.print(" - AC Scroll"); break;
      case  0x236: tft.print(" - AC Pan Left"); break;
      case  0x237: tft.print(" - AC Pan Right"); break;
      case  0x238: tft.print(" - AC Pan"); break;
      case  0x239: tft.print(" - AC New Window"); break;
      case  0x23A: tft.print(" - AC Tile Horizontally"); break;
      case  0x23B: tft.print(" - AC Tile Vertically"); break;
      case  0x23C: tft.print(" - AC Format"); break;

    }
  }
  tft.println();
  tft.updateScreen(); // update the screen now
}

void OnHIDExtrasRelease(uint32_t top, uint16_t key)
{
  tft.print("HID (");
  tft.print(top, HEX);
  tft.print(") key release:");
  tft.println(key, HEX);
}
