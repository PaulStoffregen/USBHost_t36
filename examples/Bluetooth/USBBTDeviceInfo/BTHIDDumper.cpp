#include "elapsedMillis.h"
#include <MemoryHexDump.h>
/* USB EHCI Host for Teensy 3.6
   Copyright 2017 Paul Stoffregen (paul@pjrc.com)

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, shiublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "BTHIDDumper.h"

// Uncomment for HID PARSER DEBUG OUTPUT
//#define DEBUG_HID_PARSE

class PDBGSerial_class : public Print {
  virtual size_t write(uint8_t b) {
#ifdef DEBUG_HID_PARSE
    Serial.write(b);
#endif
    return 1;
  }
};

static PDBGSerial_class PDBGSerial;


bool BTHIDDumpController::show_raw_data = true;
bool BTHIDDumpController::show_formated_data = true;
bool BTHIDDumpController::changed_data_only = false;

void BTHIDDumpController::init() {
  USBHost::contribute_Transfers(mytransfers, sizeof(mytransfers) / sizeof(Transfer_t));
  BluetoothController::driver_ready_for_bluetooth(this);
}

hidclaim_t BTHIDDumpController::claim_bluetooth(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint8_t *remoteName, int type)
//bool BTHIDDumpController::claim_bluetooth(BluetoothController *driver, uint32_t bluetooth_class, uint8_t *remoteName)
{
  // How to handle combo devices?
  Serial.printf("BTHIDDumpController Controller::claim_bluetooth - Class %x\n", bluetooth_class);
  // start off only claiming HID type devices.
  // If we are already in use than don't grab another one.  Likewise don't grab if it is used as USB or HID object
  if (btconnect && (btconnection != btconnect)) return CLAIM_NO;
  if (((bluetooth_class & 0xff00) == 0x2500) || ((bluetooth_class & 0xff00) == 0x500)) {
    Serial.printf("BTHIDDumpController::claim_bluetooth TRUE\n");
    btconnect = btconnection;
    btdevice = (Device_t *)btconnect->btController_;  // remember this way
    btdriver_ = btconnect->btController_;
    btconnect_ = btconnect;

    bluetooth_class_low_byte_ = bluetooth_class & 0xff;  // remember which HID type...
    // experiment if we want to allow the device to stay in HID mode
    //driver->useHIDProtocol(true);
    return CLAIM_INTERFACE;
  }
  return CLAIM_NO;
}
bool BTHIDDumpController::process_bluetooth_HID_data(const uint8_t *data, uint16_t length) {
  Serial.printf("(BTHID(%p, %u): ", data, length);
  dump_hexbytes(data, length, 16);
  if (decode_input_boot_data_) {
    if (data[0] == 0x01) return decode_boot_report1(data + 1, length - 1);
  }
  parse(0x0100 | data[0], data + 1, length - 1);
  return false;
}
void BTHIDDumpController::release_bluetooth() {
  Serial.println("BTHIDDumpController Controller::release_bluetooth");
  btdevice = nullptr;
  connection_complete_ = true;
  driver_ = nullptr;
  btdriver_ = nullptr;
  btconnect_ = nullptr;
  decode_input_boot_data_ = false;
}

bool BTHIDDumpController::remoteNameComplete(const uint8_t *remoteName) {
  // From Joystick.  Lets see if we need to do some special connecting...
  if (strncmp((const char *)remoteName, "Wireless Controller", 19) == 0) {
    Serial.printf("  (%s)PS4 Try special connection order\n", remoteName);
    //special_process_required = SP_NEED_CONNECT;
  }

  return true;
}


void BTHIDDumpController::connectionComplete(void) {
  // here is where I am going to try to get data...
  Serial.println("\n$$$ connectionComplete");
  connection_complete_ = true;
}

bool BTHIDDumpController::decode_boot_report1(const uint8_t *data, uint16_t length) {
  // Lets see if keyboard type:
  // lets look through the bits for the modifier keys and print out the information.
  if (bluetooth_class_low_byte_ & 0x40) {
    Serial.println("Boot Mode Keyboard update:");
    uint8_t mask = 0x01;
    uint32_t usage;
    for (usage = 0x700E0; mask; usage++) {
      if (data[0] & mask) {
        Serial.printf("usage=%X, value=1 ", usage);
        printUsageInfo(usage >> 16, usage & 0xffff);
        Serial.println();
      }
      mask <<= 1;  // shift to next bit
    }

    for (uint16_t i = 2; i < length; i++) {
      if (data[i] != 0) {
        Serial.printf("usage=%X, value=1 ", 0x70000 + data[i]);
        printUsageInfo(0x7, data[i]);
        Serial.println();
      }
    }

  } else if ((bluetooth_class_low_byte_ == 0x4) || (bluetooth_class_low_byte_ == 0x8) || (bluetooth_class_low_byte_ == 0xc)) {
    // Joystick, or gamepad or remote control
    Serial.println("Joystick RPT1 update: ");
    dump_hexbytes(data, length, 22);
  }
  return true;
}

//BUGBUG: move to class or ...
DMAMEM uint8_t sdp_buffer[4096];

int BTHIDDumpController::extract_next_SDP_Token(uint8_t *pbElement, int cb_left, sdp_element_t &sdpe) {
  uint8_t element = *pbElement;  // first byte is type of element
  sdpe.element_type = element >> 3;
  sdpe.element_size = element & 7;
  sdpe.data.luw = 0;  // start off 0

  switch (element) {
    case 0:  // nil
      sdpe.dtype = DNIL;
      return 1;  // one byte used.

    case 0x08:  // unsigned one byte
    case 0x18:  // UUID one byte
    case 0x28:  // bool one byte
      sdpe.dtype = DU32;
      sdpe.data.uw = pbElement[1];
      return 2;

    case 0x09:  // unsigned 2  byte
    case 0x19:  // uuid 2  byte
      sdpe.dtype = DU32;
      sdpe.data.uw = (pbElement[1] << 8) + pbElement[2];
      return 3;
    case 0x0A:  // unsigned 4  byte
    case 0x1A:  // UUID 4  byte
      sdpe.dtype = DU32;
      sdpe.data.uw = (uint32_t)(pbElement[1] << 24) + (uint32_t)(pbElement[2] << 16) + (pbElement[3] << 8) + pbElement[4];
      return 5;
    case 0x0B:  // unsigned 8  byte
      sdpe.dtype = DU64;
      sdpe.data.luw = ((uint64_t)pbElement[1] << 52) + ((uint64_t)pbElement[2] << 48) + ((uint64_t)pbElement[3] << 40) + ((uint64_t)pbElement[4] << 32) + (uint32_t)(pbElement[5] << 24) + (uint32_t)(pbElement[6] << 16) + (pbElement[7] << 8) + pbElement[8];
      return 9;

    // type = 2 signed
    case 0x10:  // unsigned one byte
      sdpe.dtype = DS32;
      sdpe.data.sw = (int8_t)pbElement[1];
      return 2;
    case 0x11:  // unsigned 2  byte
      sdpe.dtype = DS32;
      sdpe.data.sw = (int16_t)((pbElement[1] << 8) + pbElement[2]);
      return 3;

    case 0x12:  // unsigned 4  byte
      sdpe.dtype = DS32;
      sdpe.data.sw = (int32_t)((uint32_t)(pbElement[1] << 24) + (uint32_t)(pbElement[2] << 16) + (pbElement[3] << 8) + pbElement[4]);
      return 5;
    case 0x13:  //
      sdpe.dtype = DS64;
      sdpe.data.lsw = (int64_t)(((uint64_t)pbElement[1] << 52) + ((uint64_t)pbElement[2] << 48) + ((uint64_t)pbElement[3] << 40) + ((uint64_t)pbElement[4] << 32) + (uint32_t)(pbElement[5] << 24) + (uint32_t)(pbElement[6] << 16) + (pbElement[7] << 8) + pbElement[8]);
      return 9;

    // string one byte size.
    case 0x25:
      sdpe.dtype = DPB;
      sdpe.element_size = pbElement[1];
      sdpe.data.pb = &pbElement[2];
      return sdpe.element_size + 2;

    case 0x26:
      sdpe.dtype = DPB;
      sdpe.element_size = (pbElement[1] << 8) + pbElement[2];
      sdpe.data.pb = &pbElement[3];
      return sdpe.element_size + 3;

    // type = 7 Data element sequence
    case 0x35:  //
    case 0x3D:  //
      sdpe.dtype = DLVL;
      sdpe.element_size = pbElement[1];
      sdpe.data.pb = &pbElement[2];
      return 2;
    case 0x36:  //
    case 0x3E:  //
      sdpe.dtype = DLVL;
      sdpe.element_size = (pbElement[1] << 8) + pbElement[2];
      sdpe.data.pb = &pbElement[3];
      return 3;
    case 0x37:  //
    case 0x3F:  //
      sdpe.dtype = DLVL;
      sdpe.element_size = (uint32_t)(pbElement[1] << 24) + (uint32_t)(pbElement[2] << 16) + (pbElement[3] << 8) + pbElement[4];
      sdpe.data.pb = &pbElement[3];
      return 5;
    default:
      Serial.printf("### DECODE failed %x ###\n", element);
      return -1;
  }
}

void BTHIDDumpController::print_sdpe_val(sdp_element_t &sdpe, bool verbose) {
  switch (sdpe.dtype) {
    case DNIL: break;
    case DU32: Serial.printf(" %u(0x%X)", sdpe.data.uw, sdpe.data.uw); break;
    case DS32: Serial.printf(" %d(0x%X)", sdpe.data.sw, sdpe.data.sw); break;
    case DU64: Serial.printf(" %llu(0x%llX)", sdpe.data.luw, sdpe.data.luw); break;
    case DS64: Serial.printf(" %lld(0x%llX", sdpe.data.lsw, sdpe.data.lsw); break;
    case DPB:
      {
        // two pass, see if it looks like the data is text string:
        bool printable = true;
        for (uint16_t i = 0; i < sdpe.element_size; i++) {
          if ((sdpe.data.pb[i] < ' ') || (sdpe.data.pb[i] > '~')) {
            printable = false;
            break;
          }
        }
        if (printable) {
          Serial.print(" '");
          Serial.write(sdpe.data.pb, sdpe.element_size);
          Serial.print("'");
        } else {
          if (verbose) {
            Serial.println();
            MemoryHexDump(Serial, sdpe.data.pb, sdpe.element_size, true);
          } else {
            Serial.printf(" (%u)<", sdpe.element_size);
            for (uint16_t i = 0; i < sdpe.element_size; i++) {
              if (i == 16) {
                Serial.print("...");
                break;
              } else Serial.printf(" %02X", sdpe.data.pb[i]);
            }
            Serial.print(">");
          }
        }
      }
      break;
    case DLVL:
      if (verbose) Serial.printf("%u", sdpe.element_size);
      else Serial.print(" {");
      break;
  }
}

void BTHIDDumpController::decode_SDP_buffer(bool verbose_output) {

  uint32_t cb_buffer_used = btconnect_->SDPRequestBufferUsed();

  Serial.printf("SDP Data returned: %u bytes\n", cb_buffer_used);
  if (verbose_output) MemoryHexDump(Serial, sdp_buffer, cb_buffer_used, true);

  int cb_left = cb_buffer_used;
  uint8_t *pb = &sdp_buffer[0];  // start at second byte;
  int level_bytes_left[10];
  int count_levels = 0;
  bool next_is_attribute_num = true;

  sdp_element_t sdpe;
  while (cb_left > 0) {
    int cb = extract_next_SDP_Token(pb, cb_left, sdpe);
    if (cb < 0) break;

    for (int i = 0; i < count_levels; i++) {
      level_bytes_left[i] -= cb;
    }

    if (verbose_output) {
      // Decrement counts of byes left in levels
      for (int i = 0; i < count_levels; i++) {
        Serial.print("  ");
      }
      switch (sdpe.element_type) {
        case 0: Serial.print("NIL"); break;
        case 1: Serial.print("UINT:"); break;
        case 2: Serial.print("INT:"); break;
        case 3: Serial.print("UUID:"); break;
        case 4: Serial.print("St:"); break;
        case 5: Serial.print("Bool:"); break;
        case 6: Serial.print("{s:"); break;
        case 7: Serial.print("{a:"); break;
        case 8: Serial.print("URL:"); break;
      }

      // print out the value
      print_sdpe_val(sdpe, true);
      if (sdpe.dtype == DLVL) level_bytes_left[count_levels++] = sdpe.element_size;

      for (int i = count_levels - 1; i >= 0; i--) {
        if (level_bytes_left[i] <= 0) {
          Serial.print(" }");
          count_levels--;
        }
      }
      Serial.println();
    } else {
      //---------------------------------------
      // Lets see if we can do this structured:
      //---------------------------------------
      // levels 0=whole, 1=Record?, 2=attribute (attribute number ) (attribute value)
      if (next_is_attribute_num) {
        // this should be Attribute number
        if (count_levels == 2) {
          Serial.print("Attribute:");
          next_is_attribute_num = false;
          print_sdpe_val(sdpe, false);
          int attribute_id = -1;
          switch (sdpe.dtype) {
            case DU32: attribute_id = (int)sdpe.data.uw; break;
            case DS32: attribute_id = (int)sdpe.data.sw; break;
          }
          switch (attribute_id) {
            case 0x0000: Serial.print("(ServiceRecordHandle)"); break;
            case 0x0001: Serial.print("(ServiceClassIDList)"); break;
            case 0x0002: Serial.print("(ServiceRecordState)"); break;
            case 0x0003: Serial.print("(ServiceID)"); break;
            case 0x0004: Serial.print("(ProtocolDescriptorList)"); break;
            case 0x0005: Serial.print("(BrowseGroupList)"); break;
            case 0x0006: Serial.print("(LanguageBaseAttributeIDList)"); break;
            case 0x0007: Serial.print("(ServiceInfoTimeToLive)"); break;
            case 0x0008: Serial.print("(ServiceAvailability)"); break;
            case 0x0009: Serial.print("(BluetoothProfileDescriptorList)"); break;
            case 0x000A: Serial.print("(DocumentationURL)"); break;
            case 0x000B: Serial.print("(ClientExecutableURL)"); break;
            case 0x000C: Serial.print("(IconURL)"); break;
            case 0x000D: Serial.print("(AdditionalProtocolDescriptorLists)"); break;
            case 0x0100: Serial.print("(ServiceName)"); break;
            case 0x0101: Serial.print("(ServiceDescription)"); break;
            case 0x0102: Serial.print("(ProviderName)"); break;
            case 0x0200: Serial.print("(HIDDeviceReleaseNumber (Deprecated))"); break;
            case 0x0201: Serial.print("(HIDParserVersion)"); break;
            case 0x0202: Serial.print("(HIDDeviceSubclass)"); break;
            case 0x0203: Serial.print("(HIDCountryCode)"); break;
            case 0x0204: Serial.print("(HIDVirtualCable)"); break;
            case 0x0205: Serial.print("(HIDReconnectInitiate)"); break;
            case 0x0206: Serial.print("(HIDDescriptorList)"); break;
            case 0x0207: Serial.print("(HIDLANGIDBaseList)"); break;
            case 0x0208: Serial.print("(HIDSDPDisable (Deprecated))"); break;
            case 0x0209: Serial.print("(HIDBatteryPower)"); break;
            case 0x020A: Serial.print("(HIDRemoteWake)"); break;
            case 0x020B: Serial.print("(HIDProfileVersion)"); break;
            case 0x020C: Serial.print("(HIDSupervisionTimeout)"); break;
            case 0x020D: Serial.print("(HIDNormallyConnectable)"); break;
            case 0x200E: Serial.print("(HIDBootDevice)"); break;
            case 0x200F: Serial.print("(HIDSSRHostMaxLatency)"); break;
            case 0x2010: Serial.print("(HIDSSRHostMinTimeout)"); break;
          }
          Serial.print(" value:");
        } else if (sdpe.dtype == DLVL) level_bytes_left[count_levels++] = sdpe.element_size;
        else {
          Serial.printf("<order issue?>");
          print_sdpe_val(sdpe, false);
        }

        for (int i = count_levels - 1; i >= 0; i--) {
          if (level_bytes_left[i] <= 0) {
            count_levels--;
          }
        }

      } else {
        switch (sdpe.element_type) {
          case 3: Serial.print("UUID:"); break;
          case 5: Serial.print("Bool:"); break;
          case 8: Serial.print("URL:"); break;
        }
        print_sdpe_val(sdpe, false);
        if (sdpe.dtype == DLVL) level_bytes_left[count_levels++] = sdpe.element_size;
        for (int i = count_levels - 1; i >= 0; i--) {
          if (level_bytes_left[i] <= 0) {
            Serial.print(" }");
            count_levels--;
          }
        }
        if (count_levels == 2) next_is_attribute_num = true;
        if (count_levels <= 2) Serial.println();
      }
    }
    cb_left -= cb;
    pb += cb;
  }
}

void BTHIDDumpController::decode_SDP_Data(bool by_user_command) {
  // Start the search.
  // Maybe try setting to HID
  //return;
  if (!by_user_command) {
    if (bluetooth_class_low_byte_ && 0xc0) {
      Serial.println("Try force into HID mode");
      btdriver_->updateHIDProtocol(0x01);
    }
    // give it a little time
    delay(10);
    USBHost::Task();
  }

  Serial.println("Start Deecode SDP Data - Full Range.");
  elapsedMillis em;

  bool sdp_attributeSearch_started = btconnect_->startSDP_ServiceSearchAttributeRequest(0x00, 0xffff, sdp_buffer, sizeof(sdp_buffer));
  if (!sdp_attributeSearch_started && by_user_command) {
    Serial.println("*** SDP_ServiceSearchAttributeRequest failed try to do connect to SDP again");
    btconnect_->connectToSDP();  // see if we can try to startup SDP after
    for (uint8_t i = 0; i < 10; i++) {
      USBHost::Task();
      delay(2);
    }
    sdp_attributeSearch_started = btconnect_->startSDP_ServiceSearchAttributeRequest(0x00, 0xffff, sdp_buffer, sizeof(sdp_buffer));
  }


  if (sdp_attributeSearch_started) {
    while ((em < 2000) && !btconnect_->SDPRequestCompleted()) {
      USBHost::Task();
      delay(10);
    }
    if (!btconnect_->SDPRequestCompleted()) {
      Serial.println("Error: Decide SDP Data timed out");
    }
    Serial.println("\n=========================== Verbose ==========================");
    decode_SDP_buffer(true);
    Serial.println("\n=========================== Structured ==========================");
    decode_SDP_buffer(false);
  } else {
    Serial.println("Error: request failed");
    decode_input_boot_data_ = true;
  }
  Serial.println("\nStart Deecode SDP Data - Just Report desciptor.");
  em = 0;

  if (btconnect_->startSDP_ServiceSearchAttributeRequest(0x206, 0x206, sdp_buffer, sizeof(sdp_buffer))) {
    while ((em < 2000) && !btconnect_->SDPRequestCompleted()) {
      USBHost::Task();
      delay(10);
    }
    if (!btconnect_->SDPRequestCompleted()) {
      Serial.println("Error: Decide SDP Data timed out");
    }

    Serial.println("\n=========================== Verbose ==========================");
    decode_SDP_buffer(true);
    Serial.println("\n=========================== Structured ==========================");
    decode_SDP_buffer(false);
  } else {
    Serial.println("Error: request failed");
  }

  // Now real hack:
  // Lets see if we can now print out the report descriptor.
  uint32_t cb_left = btconnect_->SDPRequestBufferUsed();
  uint8_t *pb = &sdp_buffer[0];  // start at second byte;

  sdp_element_t sdpe;
  while (cb_left > 0) {
    int cb = extract_next_SDP_Token(pb, cb_left, sdpe);
    if (cb < 0) break;
    // Should do a lot more validation, but ...
    if ((sdpe.element_type == 4) && (sdpe.dtype == DPB)) {
      descsize = sdpe.element_size;
      memcpy(descriptor, sdpe.data.pb, descsize);
      dumpHIDReportDescriptor(descriptor, descsize);
    }

    cb_left -= cb;
    pb += cb;
  }
}



void BTHIDDumpController::dump_hexbytes(const void *ptr, uint32_t len, uint32_t indent) {
  if (ptr == NULL || len == 0) return;
  uint32_t count = 0;
  //  if (len > 64) len = 64; // don't go off deep end...
  const uint8_t *p = (const uint8_t *)ptr;
  while (len--) {
    if (*p < 16) Serial.print('0');
    Serial.print(*p++, HEX);
    count++;
    if (((count & 0x1f) == 0) && len) {
      Serial.print("\n");
      for (uint32_t i = 0; i < indent; i++) Serial.print(" ");
    } else
      Serial.print(' ');
  }
  Serial.println();
}


void indent_level(int level) {
  if ((level > 5) || (level < 0)) return;  // bail if something is off...
  while (level--) Serial.print("  ");
}

void BTHIDDumpController::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax) {
  // Lets do simplified data for changed only
  if (changed_data_only) return;

  indent_level(hid_input_begin_level_);
  Serial.printf("Begin topusage:%x type:%x min:%d max:%d indent:%u\n", topusage, type, lgmin, lgmax, hid_input_begin_level_);
  if (hid_input_begin_level_ < 2)
    hid_input_begin_level_++;
}

void BTHIDDumpController::hid_input_end() {
  // Lets do simplified data for changed only
  if (changed_data_only) return;
  if (hid_input_begin_level_) {
    // right now we are calling too many times
    hid_input_begin_level_--;
    indent_level(hid_input_begin_level_);
    Serial.println("END:");
  }
}


void BTHIDDumpController::hid_input_data(uint32_t usage, int32_t value) {

  bool output_data = !changed_data_only;

  // See if something changed.
  if (index_usages_ < count_usages_) {
    if ((usage != usages_[index_usages_]) || (value != values_[index_usages_])) {
      output_data = true;
    }
  } else {
    output_data = true;
  }
  if (index_usages_ < MAX_CHANGE_TRACKED) {
    usages_[index_usages_] = usage;
    values_[index_usages_] = value;
    index_usages_++;
  }

  if (output_data) {
    indent_level(hid_input_begin_level_);
    Serial.printf("usage=%X, value=%d ", usage, value);
    if ((value >= ' ') && (value <= '~')) Serial.printf(":%c", value);

    // maybe print out some information about some of the Usage numbers that we know about
    // The information comes from the USB document, HID Usage Tables
    // https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf

    uint16_t usage_page = usage >> 16;
    usage = usage & 0xffff;  // keep the lower part
    printUsageInfo(usage_page, usage);

    Serial.println();
  }
}


void BTHIDDumpController::printUsageInfo(uint8_t usage_page, uint16_t usage) {
  switch (usage_page) {
    case 1:  // Generic Desktop control:
      switch (usage) {
        case 0x01: Serial.print("(Pointer)"); break;
        case 0x02: Serial.print("(Mouse)"); break;
        case 0x04: Serial.print("(Joystick)"); break;
        case 0x05: Serial.print("(Gamepad)"); break;
        case 0x06: Serial.print("(Keyboard)"); break;
        case 0x07: Serial.print("(Keypad)"); break;

        case 0x30: Serial.print("(X)"); break;
        case 0x31: Serial.print("(Y)"); break;
        case 0x32: Serial.print("(Z)"); break;
        case 0x33: Serial.print("(Rx)"); break;
        case 0x34: Serial.print("(Ry)"); break;
        case 0x35: Serial.print("(Rz)"); break;
        case 0x36: Serial.print("(Slider)"); break;
        case 0x37: Serial.print("(Dial)"); break;
        case 0x38: Serial.print("(Wheel)"); break;
        case 0x39: Serial.print("(Hat)"); break;
        case 0x3D: Serial.print("(Start)"); break;
        case 0x3E: Serial.print("(Select)"); break;
        case 0x40: Serial.print("(Vx)"); break;
        case 0x41: Serial.print("(Vy)"); break;
        case 0x42: Serial.print("(Vz)"); break;
        case 0x43: Serial.print("(Vbrx)"); break;
        case 0x44: Serial.print("(Vbry)"); break;
        case 0x45: Serial.print("(Vbrz)"); break;
        case 0x46: Serial.print("(Vno)"); break;
        case 0x81: Serial.print("(System Power Down)"); break;
        case 0x82: Serial.print("(System Sleep)"); break;
        case 0x83: Serial.print("(System Wake Up)"); break;
        case 0x90: Serial.print("(D-Up)"); break;
        case 0x91: Serial.print("(D-Dn)"); break;
        case 0x92: Serial.print("(D-Right)"); break;
        case 0x93: Serial.print("(D-Left)"); break;
        default:
          Serial.print("(?)");
          break;
      }
      break;
    case 6:  // Generic Desktop control:
      switch (usage) {
        case 0x020: Serial.print("(Battery Strength)"); break;
        case 0x21: Serial.print("(Wireless Channel)"); break;
        case 0x22: Serial.print("(Wireless ID)"); break;
        case 0x23: Serial.print("(Discover Wireless Ctrl)"); break;
        case 0x24: Serial.print("(Security Code Entered)"); break;
        case 0x25: Serial.print("(Security Code erase)"); break;
        case 0x26: Serial.print("(Security Code cleared)");
        default: Serial.print("(?)"); break;
      }
      break;
    case 7:  // keyboard/keycode
      switch (usage) {
        case 0x04: Serial.print("(a and A)"); break;
        case 0x05: Serial.print("(b and B)"); break;
        case 0x06: Serial.print("(c and C)"); break;
        case 0x07: Serial.print("(d and D)"); break;
        case 0x08: Serial.print("(e and E)"); break;
        case 0x09: Serial.print("(f and F)"); break;
        case 0x0A: Serial.print("(g and G)"); break;
        case 0x0B: Serial.print("(h and H)"); break;
        case 0x0C: Serial.print("(i and I)"); break;
        case 0x0D: Serial.print("(j and J)"); break;
        case 0x0E: Serial.print("(k and K)"); break;
        case 0x0F: Serial.print("(l and L)"); break;
        case 0x10: Serial.print("(m and M)"); break;
        case 0x11: Serial.print("(n and N)"); break;
        case 0x12: Serial.print("(o and O)"); break;
        case 0x13: Serial.print("(p and P)"); break;
        case 0x14: Serial.print("(q and Q)"); break;
        case 0x15: Serial.print("(r and R)"); break;
        case 0x16: Serial.print("(s and S)"); break;
        case 0x17: Serial.print("(t and T)"); break;
        case 0x18: Serial.print("(u and U)"); break;
        case 0x19: Serial.print("(v and V)"); break;
        case 0x1A: Serial.print("(w and W)"); break;
        case 0x1B: Serial.print("(x and X)"); break;
        case 0x1C: Serial.print("(y and Y)"); break;
        case 0x1D: Serial.print("(z and Z)"); break;
        case 0x1E: Serial.print("(1 and !)"); break;
        case 0x1F: Serial.print("(2 and @)"); break;
        case 0x20: Serial.print("(3 and #)"); break;
        case 0x21: Serial.print("(4 and $)"); break;
        case 0x22: Serial.print("(5 and %)"); break;
        case 0x23: Serial.print("(6 and ^)"); break;
        case 0x24: Serial.print("(7 and &)"); break;
        case 0x25: Serial.print("(8 and *)"); break;
        case 0x26: Serial.print("(9 and ()"); break;
        case 0x27: Serial.print("(0 and ))"); break;
        case 0x28: Serial.print("(Return (ENTER))"); break;
        case 0x29: Serial.print("(ESCAPE)"); break;
        case 0x2A: Serial.print("(DELETE (Backspace))"); break;
        case 0x2B: Serial.print("(Tab)"); break;
        case 0x2C: Serial.print("(Spacebar)"); break;
        case 0x2D: Serial.print("(- and (underscore))"); break;
        case 0x2E: Serial.print("(= and +)"); break;
        case 0x2F: Serial.print("([ and {)"); break;
        case 0x30: Serial.print("(] and })"); break;
        case 0x31: Serial.print("(\and |)"); break;
        case 0x32: Serial.print("(Non-US # and ˜)"); break;
        case 0x33: Serial.print("(; and :)"); break;
        case 0x34: Serial.print("(‘ and “)"); break;
        case 0x35: Serial.print("(Grave Accent and Tilde)"); break;
        case 0x36: Serial.print("(, and <)"); break;
        case 0x37: Serial.print("(. and >)"); break;
        case 0x38: Serial.print("(/ and ?)"); break;
        case 0x39: Serial.print("(Caps Lock)"); break;
        case 0x3A: Serial.print("(F1)"); break;
        case 0x3B: Serial.print("(F2)"); break;
        case 0x3C: Serial.print("(F3)"); break;
        case 0x3D: Serial.print("(F4)"); break;
        case 0x3E: Serial.print("(F5)"); break;
        case 0x3F: Serial.print("(F6)"); break;
        case 0x40: Serial.print("(F7)"); break;
        case 0x41: Serial.print("(F8)"); break;
        case 0x42: Serial.print("(F9)"); break;
        case 0x43: Serial.print("(F10)"); break;
        case 0x44: Serial.print("(F11)"); break;
        case 0x45: Serial.print("(F12)"); break;
        case 0x46: Serial.print("(PrintScreen)"); break;
        case 0x47: Serial.print("(Scroll Lock)"); break;
        case 0x48: Serial.print("(Pause)"); break;
        case 0x49: Serial.print("(Insert)"); break;
        case 0x4A: Serial.print("(Home)"); break;
        case 0x4B: Serial.print("(PageUp)"); break;
        case 0x4C: Serial.print("(Delete Forward)"); break;
        case 0x4D: Serial.print("(End)"); break;
        case 0x4E: Serial.print("(PageDown)"); break;
        case 0x4F: Serial.print("(RightArrow)"); break;
        case 0x50: Serial.print("(LeftArrow)"); break;
        case 0x51: Serial.print("(DownArrow)"); break;
        case 0x52: Serial.print("(UpArrow)"); break;
        case 0x53: Serial.print("(Keypad Num Lock and Clear)"); break;
        case 0x54: Serial.print("(Keypad /)"); break;
        case 0x55: Serial.print("(Keypad *)"); break;
        case 0x56: Serial.print("(Keypad -)"); break;
        case 0x57: Serial.print("(Keypad +)"); break;
        case 0x58: Serial.print("(Keypad ENTER)"); break;
        case 0x59: Serial.print("(Keypad 1 and End)"); break;
        case 0x5A: Serial.print("(Keypad 2 and Down Arrow)"); break;
        case 0x5B: Serial.print("(Keypad 3 and PageDn)"); break;
        case 0x5C: Serial.print("(Keypad 4 and Left Arrow)"); break;
        case 0x5D: Serial.print("(Keypad 5)"); break;
        case 0x5E: Serial.print("(Keypad 6 and Right Arrow)"); break;
        case 0x5F: Serial.print("(Keypad 7 and Home)"); break;
        case 0x60: Serial.print("(Keypad 8 and Up Arrow)"); break;
        case 0x61: Serial.print("(Keypad 9 and PageUp)"); break;
        case 0x62: Serial.print("(Keypad 0 and Insert)"); break;
        case 0x63: Serial.print("(Keypad . and Delete)"); break;
        case 0x64: Serial.print("(Non-US \and |)"); break;
        case 0x65: Serial.print("(Application)"); break;
        case 0x66: Serial.print("(Power)"); break;
        case 0x67: Serial.print("(Keypad =)"); break;
        case 0x68: Serial.print("(F13)"); break;
        case 0x69: Serial.print("(F14)"); break;
        case 0x6A: Serial.print("(F15)"); break;
        case 0x6B: Serial.print("(F16)"); break;
        case 0x6C: Serial.print("(F17)"); break;
        case 0x6D: Serial.print("(F18)"); break;
        case 0x6E: Serial.print("(F19)"); break;
        case 0x6F: Serial.print("(F20)"); break;
        case 0x70: Serial.print("(F21)"); break;
        case 0x71: Serial.print("(F22)"); break;
        case 0x72: Serial.print("(F23)"); break;
        case 0x73: Serial.print("(F24)"); break;
        case 0x74: Serial.print("(Execute)"); break;
        case 0x75: Serial.print("(Help)"); break;
        case 0x76: Serial.print("(Menu)"); break;
        case 0x77: Serial.print("(Select)"); break;
        case 0x78: Serial.print("(Stop)"); break;
        case 0x79: Serial.print("(Again)"); break;
        case 0x7A: Serial.print("(Undo)"); break;
        case 0x7B: Serial.print("(Cut)"); break;
        case 0x7C: Serial.print("(Copy)"); break;
        case 0x7D: Serial.print("(Paste)"); break;
        case 0x7E: Serial.print("(Find)"); break;
        case 0x7F: Serial.print("(Mute)"); break;
        case 0x80: Serial.print("(Volume Up)"); break;
        case 0x81: Serial.print("(Volume Down)"); break;
        case 0x82: Serial.print("(Locking Caps Lock)"); break;
        case 0x83: Serial.print("(Locking Num Lock)"); break;
        case 0x84: Serial.print("(Locking Scroll Lock)"); break;
        case 0x85: Serial.print("(Keypad Comma)"); break;
        case 0x99: Serial.print("(Alternate Erase)"); break;
        case 0x9A: Serial.print("(SysReq/Attention)"); break;
        case 0x9B: Serial.print("(Cancel)"); break;
        case 0x9C: Serial.print("(Clear)"); break;
        case 0x9D: Serial.print("(Prior)"); break;
        case 0x9E: Serial.print("(Return)"); break;
        case 0x9F: Serial.print("(Separator)"); break;
        case 0xA0: Serial.print("(Out)"); break;
        case 0xA1: Serial.print("(Oper)"); break;
        case 0xA2: Serial.print("(Clear/Again)"); break;
        case 0xA3: Serial.print("(CrSel/Props)"); break;
        case 0xA4: Serial.print("(ExSel)"); break;
        case 0xA5: Serial.print("(AF Reserved)"); break;
        case 0xB0: Serial.print("(Keypad 00)"); break;
        case 0xB1: Serial.print("(Keypad 000)"); break;
        case 0xB2: Serial.print("(Thousands Separator)"); break;
        case 0xB3: Serial.print("(Decimal Separator)"); break;
        case 0xB4: Serial.print("(Currency Unit)"); break;
        case 0xB5: Serial.print("(Currency Sub-unit)"); break;
        case 0xB6: Serial.print("(Keypad ()"); break;
        case 0xB7: Serial.print("(Keypad ))"); break;
        case 0xB8: Serial.print("(Keypad {)"); break;
        case 0xB9: Serial.print("(Keypad })"); break;
        case 0xBA: Serial.print("(Keypad Tab)"); break;
        case 0xBB: Serial.print("(Keypad Backspace)"); break;
        case 0xBC: Serial.print("(Keypad A)"); break;
        case 0xBD: Serial.print("(Keypad B)"); break;
        case 0xBE: Serial.print("(Keypad C)"); break;
        case 0xBF: Serial.print("(Keypad D)"); break;
        case 0xC0: Serial.print("(Keypad E)"); break;
        case 0xC1: Serial.print("(Keypad F)"); break;
        case 0xC2: Serial.print("(Keypad XOR)"); break;
        case 0xC3: Serial.print("(Keypad ^)"); break;
        case 0xC4: Serial.print("(Keypad %)"); break;
        case 0xC5: Serial.print("(Keypad <)"); break;
        case 0xC6: Serial.print("(Keypad >)"); break;
        case 0xC7: Serial.print("(Keypad &)"); break;
        case 0xC8: Serial.print("(Keypad &&)"); break;
        case 0xC9: Serial.print("(Keypad |)"); break;
        case 0xCA: Serial.print("(Keypad ||)"); break;
        case 0xCB: Serial.print("(Keypad :)"); break;
        case 0xCC: Serial.print("(Keypad #)"); break;
        case 0xCD: Serial.print("(Keypad Space)"); break;
        case 0xCE: Serial.print("(Keypad @)"); break;
        case 0xCF: Serial.print("(Keypad !)"); break;
        case 0xD0: Serial.print("(Keypad Memory Store)"); break;
        case 0xD1: Serial.print("(Keypad Memory Recall)"); break;
        case 0xD2: Serial.print("(Keypad Memory Clear)"); break;
        case 0xD3: Serial.print("(Keypad Memory Add)"); break;
        case 0xD4: Serial.print("(Keypad Memory Subtract)"); break;
        case 0xD5: Serial.print("(Keypad Memory Multiply)"); break;
        case 0xD6: Serial.print("(Keypad Memory Divide)"); break;
        case 0xD7: Serial.print("(Keypad +/-)"); break;
        case 0xD8: Serial.print("(Keypad Clear)"); break;
        case 0xD9: Serial.print("(Keypad Clear Entry)"); break;
        case 0xDA: Serial.print("(Keypad Binary)"); break;
        case 0xDB: Serial.print("(Keypad Octal)"); break;
        case 0xDC: Serial.print("(Keypad Decimal)"); break;
        case 0xDD: Serial.print("(Keypad Hexadecimal)"); break;

        case 0xE0: Serial.print("(Left Control)"); break;
        case 0xE1: Serial.print("(Left Shift)"); break;
        case 0xE2: Serial.print("(Left Alt)"); break;
        case 0xE3: Serial.print("(Left GUI)"); break;
        case 0xE4: Serial.print("(Right Control)"); break;
        case 0xE5: Serial.print("(Right Shift)"); break;
        case 0xE6: Serial.print("(Right Alt)"); break;
        case 0xE7: Serial.print("(Right GUI)"); break;
        default:
          Serial.printf("(Keycode %u)", usage);
          break;
      }
      break;
    case 9:  // Button
      Serial.printf(" (BUTTON %d)", usage);
      break;
    case 0xC:  // Consummer page
      switch (usage) {
        case 0x01: Serial.print("(Consumer Controls)"); break;
        case 0x20: Serial.print("(+10)"); break;
        case 0x21: Serial.print("(+100)"); break;
        case 0x22: Serial.print("(AM/PM)"); break;
        case 0x30: Serial.print("(Power)"); break;
        case 0x31: Serial.print("(Reset)"); break;
        case 0x32: Serial.print("(Sleep)"); break;
        case 0x33: Serial.print("(Sleep After)"); break;
        case 0x34: Serial.print("(Sleep Mode)"); break;
        case 0x35: Serial.print("(Illumination)"); break;
        case 0x36: Serial.print("(Function Buttons)"); break;
        case 0x40: Serial.print("(Menu)"); break;
        case 0x41: Serial.print("(Menu  Pick)"); break;
        case 0x42: Serial.print("(Menu Up)"); break;
        case 0x43: Serial.print("(Menu Down)"); break;
        case 0x44: Serial.print("(Menu Left)"); break;
        case 0x45: Serial.print("(Menu Right)"); break;
        case 0x46: Serial.print("(Menu Escape)"); break;
        case 0x47: Serial.print("(Menu Value Increase)"); break;
        case 0x48: Serial.print("(Menu Value Decrease)"); break;
        case 0x60: Serial.print("(Data On Screen)"); break;
        case 0x61: Serial.print("(Closed Caption)"); break;
        case 0x62: Serial.print("(Closed Caption Select)"); break;
        case 0x63: Serial.print("(VCR/TV)"); break;
        case 0x64: Serial.print("(Broadcast Mode)"); break;
        case 0x65: Serial.print("(Snapshot)"); break;
        case 0x66: Serial.print("(Still)"); break;
        case 0x80: Serial.print("(Selection)"); break;
        case 0x81: Serial.print("(Assign Selection)"); break;
        case 0x82: Serial.print("(Mode Step)"); break;
        case 0x83: Serial.print("(Recall Last)"); break;
        case 0x84: Serial.print("(Enter Channel)"); break;
        case 0x85: Serial.print("(Order Movie)"); break;
        case 0x86: Serial.print("(Channel)"); break;
        case 0x87: Serial.print("(Media Selection)"); break;
        case 0x88: Serial.print("(Media Select Computer)"); break;
        case 0x89: Serial.print("(Media Select TV)"); break;
        case 0x8A: Serial.print("(Media Select WWW)"); break;
        case 0x8B: Serial.print("(Media Select DVD)"); break;
        case 0x8C: Serial.print("(Media Select Telephone)"); break;
        case 0x8D: Serial.print("(Media Select Program Guide)"); break;
        case 0x8E: Serial.print("(Media Select Video Phone)"); break;
        case 0x8F: Serial.print("(Media Select Games)"); break;
        case 0x90: Serial.print("(Media Select Messages)"); break;
        case 0x91: Serial.print("(Media Select CD)"); break;
        case 0x92: Serial.print("(Media Select VCR)"); break;
        case 0x93: Serial.print("(Media Select Tuner)"); break;
        case 0x94: Serial.print("(Quit)"); break;
        case 0x95: Serial.print("(Help)"); break;
        case 0x96: Serial.print("(Media Select Tape)"); break;
        case 0x97: Serial.print("(Media Select Cable)"); break;
        case 0x98: Serial.print("(Media Select Satellite)"); break;
        case 0x99: Serial.print("(Media Select Security)"); break;
        case 0x9A: Serial.print("(Media Select Home)"); break;
        case 0x9B: Serial.print("(Media Select Call)"); break;
        case 0x9C: Serial.print("(Channel Increment)"); break;
        case 0x9D: Serial.print("(Channel Decrement)"); break;
        case 0x9E: Serial.print("(Media Select SAP)"); break;
        case 0xA0: Serial.print("(VCR Plus)"); break;
        case 0xA1: Serial.print("(Once)"); break;
        case 0xA2: Serial.print("(Daily)"); break;
        case 0xA3: Serial.print("(Weekly)"); break;
        case 0xA4: Serial.print("(Monthly)"); break;
        case 0xB0: Serial.print("(Play)"); break;
        case 0xB1: Serial.print("(Pause)"); break;
        case 0xB2: Serial.print("(Record)"); break;
        case 0xB3: Serial.print("(Fast Forward)"); break;
        case 0xB4: Serial.print("(Rewind)"); break;
        case 0xB5: Serial.print("(Scan Next Track)"); break;
        case 0xB6: Serial.print("(Scan Previous Track)"); break;
        case 0xB7: Serial.print("(Stop)"); break;
        case 0xB8: Serial.print("(Eject)"); break;
        case 0xB9: Serial.print("(Random Play)"); break;
        case 0xBA: Serial.print("(Select DisC)"); break;
        case 0xBB: Serial.print("(Enter Disc)"); break;
        case 0xBC: Serial.print("(Repeat)"); break;
        case 0xBD: Serial.print("(Tracking)"); break;
        case 0xBE: Serial.print("(Track Normal)"); break;
        case 0xBF: Serial.print("(Slow Tracking)"); break;
        case 0xC0: Serial.print("(Frame Forward)"); break;
        case 0xC1: Serial.print("(Frame Back)"); break;
        case 0xC2: Serial.print("(Mark)"); break;
        case 0xC3: Serial.print("(Clear Mark)"); break;
        case 0xC4: Serial.print("(Repeat From Mark)"); break;
        case 0xC5: Serial.print("(Return To Mark)"); break;
        case 0xC6: Serial.print("(Search Mark Forward)"); break;
        case 0xC7: Serial.print("(Search Mark Backwards)"); break;
        case 0xC8: Serial.print("(Counter Reset)"); break;
        case 0xC9: Serial.print("(Show Counter)"); break;
        case 0xCA: Serial.print("(Tracking Increment)"); break;
        case 0xCB: Serial.print("(Tracking Decrement)"); break;
        case 0xCD: Serial.print("(Pause/Continue)"); break;
        case 0xE0: Serial.print("(Volume)"); break;
        case 0xE1: Serial.print("(Balance)"); break;
        case 0xE2: Serial.print("(Mute)"); break;
        case 0xE3: Serial.print("(Bass)"); break;
        case 0xE4: Serial.print("(Treble)"); break;
        case 0xE5: Serial.print("(Bass Boost)"); break;
        case 0xE6: Serial.print("(Surround Mode)"); break;
        case 0xE7: Serial.print("(Loudness)"); break;
        case 0xE8: Serial.print("(MPX)"); break;
        case 0xE9: Serial.print("(Volume Up)"); break;
        case 0xEA: Serial.print("(Volume Down)"); break;
        case 0xF0: Serial.print("(Speed Select)"); break;
        case 0xF1: Serial.print("(Playback Speed)"); break;
        case 0xF2: Serial.print("(Standard Play)"); break;
        case 0xF3: Serial.print("(Long Play)"); break;
        case 0xF4: Serial.print("(Extended Play)"); break;
        case 0xF5: Serial.print("(Slow)"); break;
        case 0x100: Serial.print("(Fan Enable)"); break;
        case 0x101: Serial.print("(Fan Speed)"); break;
        case 0x102: Serial.print("(Light)"); break;
        case 0x103: Serial.print("(Light Illumination Level)"); break;
        case 0x104: Serial.print("(Climate Control Enable)"); break;
        case 0x105: Serial.print("(Room Temperature)"); break;
        case 0x106: Serial.print("(Security Enable)"); break;
        case 0x107: Serial.print("(Fire Alarm)"); break;
        case 0x108: Serial.print("(Police Alarm)"); break;
        case 0x150: Serial.print("(Balance Right)"); break;
        case 0x151: Serial.print("(Balance Left)"); break;
        case 0x152: Serial.print("(Bass Increment)"); break;
        case 0x153: Serial.print("(Bass Decrement)"); break;
        case 0x154: Serial.print("(Treble Increment)"); break;
        case 0x155: Serial.print("(Treble Decrement)"); break;
        case 0x160: Serial.print("(Speaker System)"); break;
        case 0x161: Serial.print("(Channel Left)"); break;
        case 0x162: Serial.print("(Channel Right)"); break;
        case 0x163: Serial.print("(Channel Center)"); break;
        case 0x164: Serial.print("(Channel Front)"); break;
        case 0x165: Serial.print("(Channel Center Front)"); break;
        case 0x166: Serial.print("(Channel Side)"); break;
        case 0x167: Serial.print("(Channel Surround)"); break;
        case 0x168: Serial.print("(Channel Low Frequency Enhancement)"); break;
        case 0x169: Serial.print("(Channel Top)"); break;
        case 0x16A: Serial.print("(Channel Unknown)"); break;
        case 0x170: Serial.print("(Sub-channel)"); break;
        case 0x171: Serial.print("(Sub-channel Increment)"); break;
        case 0x172: Serial.print("(Sub-channel Decrement)"); break;
        case 0x173: Serial.print("(Alternate Audio Increment)"); break;
        case 0x174: Serial.print("(Alternate Audio Decrement)"); break;
        case 0x180: Serial.print("(Application Launch Buttons)"); break;
        case 0x181: Serial.print("(AL Launch Button Configuration Tool)"); break;
        case 0x182: Serial.print("(AL Programmable Button Configuration)"); break;
        case 0x183: Serial.print("(AL Consumer Control Configuration)"); break;
        case 0x184: Serial.print("(AL Word Processor)"); break;
        case 0x185: Serial.print("(AL Text Editor)"); break;
        case 0x186: Serial.print("(AL Spreadsheet)"); break;
        case 0x187: Serial.print("(AL Graphics Editor)"); break;
        case 0x188: Serial.print("(AL Presentation App)"); break;
        case 0x189: Serial.print("(AL Database App)"); break;
        case 0x18A: Serial.print("(AL Email Reader)"); break;
        case 0x18B: Serial.print("(AL Newsreader)"); break;
        case 0x18C: Serial.print("(AL Voicemail)"); break;
        case 0x18D: Serial.print("(AL Contacts/Address Book)"); break;
        case 0x18E: Serial.print("(AL Calendar/Schedule)"); break;
        case 0x18F: Serial.print("(AL Task/Project Manager)"); break;
        case 0x190: Serial.print("(AL Log/Journal/Timecard)"); break;
        case 0x191: Serial.print("(AL Checkbook/Finance)"); break;
        case 0x192: Serial.print("(AL Calculator)"); break;
        case 0x193: Serial.print("(AL A/V Capture/Playback)"); break;
        case 0x194: Serial.print("(AL Local Machine Browser)"); break;
        case 0x195: Serial.print("(AL LAN/WAN Browser)"); break;
        case 0x196: Serial.print("(AL Internet Browser)"); break;
        case 0x197: Serial.print("(AL Remote Networking/ISP Connect)"); break;
        case 0x198: Serial.print("(AL Network Conference)"); break;
        case 0x199: Serial.print("(AL Network Chat)"); break;
        case 0x19A: Serial.print("(AL Telephony/Dialer)"); break;
        case 0x19B: Serial.print("(AL Logon)"); break;
        case 0x19C: Serial.print("(AL Logoff)"); break;
        case 0x19D: Serial.print("(AL Logon/Logoff)"); break;
        case 0x19E: Serial.print("(AL Terminal Lock/Screensaver)"); break;
        case 0x19F: Serial.print("(AL Control Panel)"); break;
        case 0x1A0: Serial.print("(AL Command Line Processor/Run)"); break;
        case 0x1A1: Serial.print("(AL Process/Task Manager)"); break;
        case 0x1A2: Serial.print("(AL Select Tast/Application)"); break;
        case 0x1A3: Serial.print("(AL Next Task/Application)"); break;
        case 0x1A4: Serial.print("(AL Previous Task/Application)"); break;
        case 0x1A5: Serial.print("(AL Preemptive Halt Task/Application)"); break;
        case 0x200: Serial.print("(Generic GUI Application Controls)"); break;
        case 0x201: Serial.print("(AC New)"); break;
        case 0x202: Serial.print("(AC Open)"); break;
        case 0x203: Serial.print("(AC Close)"); break;
        case 0x204: Serial.print("(AC Exit)"); break;
        case 0x205: Serial.print("(AC Maximize)"); break;
        case 0x206: Serial.print("(AC Minimize)"); break;
        case 0x207: Serial.print("(AC Save)"); break;
        case 0x208: Serial.print("(AC Print)"); break;
        case 0x209: Serial.print("(AC Properties)"); break;
        case 0x21A: Serial.print("(AC Undo)"); break;
        case 0x21B: Serial.print("(AC Copy)"); break;
        case 0x21C: Serial.print("(AC Cut)"); break;
        case 0x21D: Serial.print("(AC Paste)"); break;
        case 0x21E: Serial.print("(AC Select All)"); break;
        case 0x21F: Serial.print("(AC Find)"); break;
        case 0x220: Serial.print("(AC Find and Replace)"); break;
        case 0x221: Serial.print("(AC Search)"); break;
        case 0x222: Serial.print("(AC Go To)"); break;
        case 0x223: Serial.print("(AC Home)"); break;
        case 0x224: Serial.print("(AC Back)"); break;
        case 0x225: Serial.print("(AC Forward)"); break;
        case 0x226: Serial.print("(AC Stop)"); break;
        case 0x227: Serial.print("(AC Refresh)"); break;
        case 0x228: Serial.print("(AC Previous Link)"); break;
        case 0x229: Serial.print("(AC Next Link)"); break;
        case 0x22A: Serial.print("(AC Bookmarks)"); break;
        case 0x22B: Serial.print("(AC History)"); break;
        case 0x22C: Serial.print("(AC Subscriptions)"); break;
        case 0x22D: Serial.print("(AC Zoom In)"); break;
        case 0x22E: Serial.print("(AC Zoom Out)"); break;
        case 0x22F: Serial.print("(AC Zoom)"); break;
        case 0x230: Serial.print("(AC Full Screen View)"); break;
        case 0x231: Serial.print("(AC Normal View)"); break;
        case 0x232: Serial.print("(AC View Toggle)"); break;
        case 0x233: Serial.print("(AC Scroll Up)"); break;
        case 0x234: Serial.print("(AC Scroll Down)"); break;
        case 0x235: Serial.print("(AC Scroll)"); break;
        case 0x236: Serial.print("(AC Pan Left)"); break;
        case 0x237: Serial.print("(AC Pan Right)"); break;
        case 0x238: Serial.print("(AC Pan)"); break;
        case 0x239: Serial.print("(AC New Window)"); break;
        case 0x23A: Serial.print("(AC Tile Horizontally)"); break;
        case 0x23B: Serial.print("(AC Tile Vertically)"); break;
        case 0x23C: Serial.print("(AC Format)"); break;
        default: Serial.print("(?)"); break;
      }
      break;
  }
}


void BTHIDDumpController::dumpHIDReportDescriptor(uint8_t *pb, uint16_t cb) {

  const uint8_t *p = pb;
  uint16_t report_size = cb;

  const uint8_t *pend = p + report_size;
  uint8_t collection_level = 0;
  uint16_t usage_page = 0;
  enum { USAGE_LIST_LEN = 24 };
  uint16_t usage[USAGE_LIST_LEN] = { 0, 0 };
  uint8_t usage_count = 0;
  uint32_t topusage;
  cnt_feature_reports_ = 0;
  uint8_t last_report_id = 0;
  Serial.printf("\nHID Report Descriptor (%p) size: %u\n", p, report_size);
  while (p < pend) {
    uint8_t tag = *p;
    for (uint8_t i = 0; i < collection_level; i++) Serial.print("  ");
    Serial.printf("  %02X", tag);

    if (tag == 0xFE) {  // Long Item (unsupported)
      p += p[1] + 3;
      continue;
    }
    uint32_t val;
    switch (tag & 0x03) {  // Short Item data
      case 0:
        val = 0;
        p++;
        break;
      case 1:
        val = p[1];
        // could be better;
        Serial.printf(" %02X", p[1]);
        p += 2;
        break;
      case 2:
        val = p[1] | (p[2] << 8);
        Serial.printf(" %02X %02X", p[1], p[2]);
        p += 3;
        break;
      case 3:
        val = p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24);
        Serial.printf(" %02X %02X %02X %02X", p[1], p[2], p[3], p[4]);
        p += 5;
        break;
    }
    if (p > pend) break;

    bool reset_local = false;
    switch (tag & 0xfc) {
      case 0x4:  //usage Page
        {
          usage_page = val;
          Serial.printf("\t// Usage Page(%x) - ", val);
          switch (usage_page) {
            case 0x01: Serial.print("Generic Desktop"); break;
            case 0x06: Serial.print("Generic Device Controls"); break;
            case 0x07: Serial.print("Keycode"); break;
            case 0x08: Serial.print("LEDs"); break;
            case 0x09: Serial.print("Button"); break;
            case 0x0C: Serial.print("Consumer"); break;
            case 0x0D:
            case 0xFF0D: Serial.print("Digitizer"); break;
            default:
              if (usage_page >= 0xFF00) Serial.print("Vendor Defined");
              else Serial.print("Other ?");
              break;
          }
        }
        break;
      case 0x08:  //usage
        Serial.printf("\t// Usage(%x) -", val);
        printUsageInfo(usage_page, val);
        if (usage_count < USAGE_LIST_LEN) {
          // Usages: 0 is reserved 0x1-0x1f is sort of reserved for top level things like
          // 0x1 - Pointer - A collection... So lets try ignoring these
          if (val > 0x1f) {
            usage[usage_count++] = val;
          }
        }
        break;
      case 0x14:  // Logical Minimum (global)
        Serial.printf("\t// Logical Minimum(%x)", val);
        break;
      case 0x24:  // Logical Maximum (global)
        Serial.printf("\t// Logical maximum(%x)", val);
        break;
      case 0x74:  // Report Size (global)
        Serial.printf("\t// Report Size(%x)", val);
        break;
      case 0x94:  // Report Count (global)
        Serial.printf("\t// Report Count(%x)", val);
        break;
      case 0x84:  // Report ID (global)
        Serial.printf("\t// Report ID(%x)", val);
        last_report_id = val;
        break;
      case 0x18:  // Usage Minimum (local)
        usage[0] = val;
        usage_count = 255;
        Serial.printf("\t// Usage Minimum(%x) - ", val);
        printUsageInfo(usage_page, val);
        break;
      case 0x28:  // Usage Maximum (local)
        usage[1] = val;
        usage_count = 255;
        Serial.printf("\t// Usage Maximum(%x) - ", val);
        printUsageInfo(usage_page, val);
        break;
      case 0xA0:  // Collection
        Serial.printf("\t// Collection(%x)", val);
        // discard collection info if not top level, hopefully that's ok?
        if (collection_level == 0) {
          topusage = ((uint32_t)usage_page << 16) | usage[0];
          Serial.printf(" top Usage(%x)", topusage);
          collection_level++;
        }
        reset_local = true;
        break;
      case 0xC0:  // End Collection
        Serial.print("\t// End Collection");
        if (collection_level > 0) collection_level--;
        break;

      case 0x80:  // Input
        Serial.printf("\t// Input(%x)\t// (", val);
        print_input_output_feature_bits(val);
        reset_local = true;
        break;
      case 0x90:  // Output
        Serial.printf("\t// Output(%x)\t// (", val);
        print_input_output_feature_bits(val);
        reset_local = true;
        break;
      case 0xB0:  // Feature
        Serial.printf("\t// Feature(%x)\t// (", val);
        print_input_output_feature_bits(val);
        if (cnt_feature_reports_ < MAX_FEATURE_REPORTS) {
          feature_report_ids_[cnt_feature_reports_++] = last_report_id;
        }
        reset_local = true;
        break;

      case 0x34:  // Physical Minimum (global)
        Serial.printf("\t// Physical Minimum(%x)", val);
        break;
      case 0x44:  // Physical Maximum (global)
        Serial.printf("\t// Physical Maximum(%x)", val);
        break;
      case 0x54:  // Unit Exponent (global)
        Serial.printf("\t// Unit Exponent(%x)", val);
        break;
      case 0x64:  // Unit (global)
        Serial.printf("\t// Unit(%x)", val);
        break;
    }
    if (reset_local) {
      usage_count = 0;
      usage[0] = 0;
      usage[1] = 0;
    }

    Serial.println();
  }
}

void BTHIDDumpController::print_input_output_feature_bits(uint8_t val) {
  Serial.print((val & 0x01) ? "Constant" : "Data");
  Serial.print((val & 0x02) ? ", Variable" : ", Array");
  Serial.print((val & 0x04) ? ", Relative" : ", Absolute");
  if (val & 0x08) Serial.print(", Wrap");
  if (val & 0x10) Serial.print(", Non Linear");
  if (val & 0x20) Serial.print(", No Preferred");
  if (val & 0x40) Serial.print(", Null State");
  if (val & 0x80) Serial.print(", Volatile");
  if (val & 0x100) Serial.print(", Buffered Bytes");
  Serial.print(")");
}

//=============================================================================
// Lets try copy of the HID Parse code and see what happens with with it.
//=============================================================================

// Extract 1 to 32 bits from the data array, starting at bitindex.
static uint32_t bitfield(const uint8_t *data, uint32_t bitindex, uint32_t numbits) {
  uint32_t output = 0;
  uint32_t bitcount = 0;
  data += (bitindex >> 3);
  uint32_t offset = bitindex & 7;
  if (offset) {
    output = (*data++) >> offset;
    bitcount = 8 - offset;
  }
  while (bitcount < numbits) {
    output |= (uint32_t)(*data++) << bitcount;
    bitcount += 8;
  }
  if (bitcount > numbits && numbits < 32) {
    output &= ((1 << numbits) - 1);
  }
  return output;
}

// convert a number with the specified number of bits from unsigned to signed,
// so the result is a proper 32 bit signed integer.
static int32_t signext(uint32_t num, uint32_t bitcount) {
  if (bitcount < 32 && bitcount > 0 && (num & (1 << (bitcount - 1)))) {
    num |= ~((1 << bitcount) - 1);
  }
  return (int32_t)num;
}

// convert a tag's value to a signed integer.
static int32_t signedval(uint32_t num, uint8_t tag) {
  tag &= 3;
  if (tag == 1) return (int8_t)num;
  if (tag == 2) return (int16_t)num;
  return (int32_t)num;
}


void BTHIDDumpController::parse(uint16_t type_and_report_id, const uint8_t *data, uint32_t len) {
  const uint8_t *p = descriptor;
  const uint8_t *end = p + descsize;
  //USBHIDInput *driver = NULL;
  BTHIDDumpController *driver = this;  // hack for now everything feeds back to us...
  uint32_t topusage = 0;
  //uint8_t topusage_index = 0;
  uint8_t collection_level = 0;
  uint16_t usage[USAGE_LIST_LEN] = { 0, 0 };
  uint8_t usage_count = 0;
  uint8_t usage_min_max_count = 0;
  uint8_t usage_min_max_mask = 0;
  uint8_t report_id = 0;
  uint16_t report_size = 0;
  uint16_t report_count = 0;
  uint16_t usage_page = 0;
  uint32_t last_usage = 0;
  int32_t logical_min = 0;
  int32_t logical_max = 0;
  uint32_t bitindex = 0;

  while (p < end) {
    uint8_t tag = *p;
    if (tag == 0xFE) {  // Long Item (unsupported)
      p += p[1] + 3;
      continue;
    }
    uint32_t val;
    switch (tag & 0x03) {  // Short Item data
      case 0:
        val = 0;
        p++;
        break;
      case 1:
        val = p[1];
        p += 2;
        break;
      case 2:
        val = p[1] | (p[2] << 8);
        p += 3;
        break;
      case 3:
        val = p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24);
        p += 5;
        break;
    }
    if (p > end) break;
    bool reset_local = false;
    switch (tag & 0xFC) {
      case 0x04:  // Usage Page (global)
        usage_page = val;
        break;
      case 0x14:  // Logical Minimum (global)
        logical_min = signedval(val, tag);
        break;
      case 0x24:  // Logical Maximum (global)
        logical_max = signedval(val, tag);
        break;
      case 0x74:  // Report Size (global)
        report_size = val;
        break;
      case 0x94:  // Report Count (global)
        report_count = val;
        break;
      case 0x84:  // Report ID (global)
        report_id = val;
        break;
      case 0x08:  // Usage (local)
        if (usage_count < USAGE_LIST_LEN) {
          // Usages: 0 is reserved 0x1-0x1f is sort of reserved for top level things like
          // 0x1 - Pointer - A collection... So lets try ignoring these
          if (val > 0x1f) {
            usage[usage_count++] = val;
          }
        }
        break;
      case 0x18:  // Usage Minimum (local)
        // Note: Found a report with multiple min/max
        if (usage_count != 255) {
          usage_count = 255;
          usage_min_max_count = 0;
          usage_min_max_mask = 0;
        }
        usage[usage_min_max_count * 2] = val;
        usage_min_max_mask |= 1;
        if (usage_min_max_mask == 3) {
          usage_min_max_count++;
          usage_min_max_mask = 0;
        }
        break;
      case 0x28:  // Usage Maximum (local)
        if (usage_count != 255) {
          usage_count = 255;
          usage_min_max_count = 0;
          usage_min_max_mask = 0;
        }
        usage[usage_min_max_count * 2 + 1] = val;
        usage_min_max_mask |= 2;
        if (usage_min_max_mask == 3) {
          usage_min_max_count++;
          usage_min_max_mask = 0;
        }
        break;
      case 0xA0:  // Collection
        if (collection_level == 0) {
          topusage = ((uint32_t)usage_page << 16) | usage[0];
#if 0
				driver = NULL;
				if (topusage_index < TOPUSAGE_LIST_LEN) {
					driver = topusage_drivers[topusage_index++];
				}
#endif
        }
        // discard collection info if not top level, hopefully that's ok?
        collection_level++;
        reset_local = true;
        break;
      case 0xC0:  // End Collection
        if (collection_level > 0) {
          collection_level--;
          if (collection_level == 0 && driver != NULL) {
            driver->hid_input_end();
            //driver = NULL;
          }
        }
        reset_local = true;
        break;
      case 0x80:  // Input
        if (use_report_id && (report_id != (type_and_report_id & 0xFF))) {
          // completely ignore and do not advance bitindex
          // for descriptors of other report IDs
          reset_local = true;
          break;
        }
        if ((val & 1) || (driver == NULL)) {
          // skip past constant fields or when no driver is listening
          bitindex += report_count * report_size;
        } else {
          PDBGSerial.print("begin, usage=");
          PDBGSerial.println(topusage, HEX);
          PDBGSerial.print("       type= ");
          PDBGSerial.println(val, HEX);
          PDBGSerial.print("       min=  ");
          PDBGSerial.println(logical_min);
          PDBGSerial.print("       max=  ");
          PDBGSerial.println(logical_max);
          PDBGSerial.print("       reportcount=");
          PDBGSerial.println(report_count);
          PDBGSerial.print("       usage count=");
          PDBGSerial.println(usage_count);
          PDBGSerial.print("       usage min max count=");
          PDBGSerial.println(usage_min_max_count);

          driver->hid_input_begin(topusage, val, logical_min, logical_max);
          PDBGSerial.print("Input, total bits=");
          PDBGSerial.println(report_count * report_size);
          if ((val & 2)) {
            // ordinary variable format
            uint32_t uindex = 0;
            uint32_t uindex_max = 0xffff;  // assume no MAX
            bool uminmax = false;
            uint8_t uminmax_index = 0;
            if (usage_count > USAGE_LIST_LEN) {
              // usage numbers by min/max, not from list
              uindex = usage[0];
              uindex_max = usage[1];
              uminmax = true;
            } else if ((report_count > 1) && (usage_count <= 1)) {
              // Special cases:  Either only one or no usages specified and there are more than one
              // report counts .
              if (usage_count == 1) {
                uindex = usage[0];
              } else {
                // BUGBUG:: Not sure good place to start?  maybe round up from last usage to next higher group up of 0x100?
                uindex = (last_usage & 0xff00) + 0x100;
              }
              uminmax = true;
            }
            //USBHDBGPDBGSerial.printf("TU:%x US:%x %x %d %d: C:%d, %d, MM:%d, %x %x\n", topusage, usage_page, val, logical_min, logical_max,
            //			report_count, usage_count, uminmax, usage[0], usage[1]);
            for (uint32_t i = 0; i < report_count; i++) {
              uint32_t u;
              if (uminmax) {
                u = uindex;
                if (uindex < uindex_max) uindex++;
                else if (uminmax_index < usage_min_max_count) {
                  uminmax_index++;
                  uindex = usage[uminmax_index * 2];
                  uindex_max = usage[uminmax_index * 2 + 1];
                  //USBHDBGPDBGSerial.printf("$$ next min/max pair: %u %u %u\n", uminmax_index, uindex, uindex_max);
                }
              } else {
                u = usage[uindex++];
                if (uindex >= USAGE_LIST_LEN - 1) {
                  uindex = USAGE_LIST_LEN - 1;
                }
              }
              last_usage = u;  // remember the last one we used...
              u |= (uint32_t)usage_page << 16;
              PDBGSerial.print("  usage = ");
              PDBGSerial.print(u, HEX);

              uint32_t n = bitfield(data, bitindex, report_size);
              if (logical_min >= 0) {
                PDBGSerial.print("  data = ");
                PDBGSerial.println(n);
                driver->hid_input_data(u, n);
              } else {
                int32_t sn = signext(n, report_size);
                PDBGSerial.print("  sdata = ");
                PDBGSerial.println(sn);
                driver->hid_input_data(u, sn);
              }
              bitindex += report_size;
            }
          } else {
            // array format, each item is a usage number
            // maybe act like the 2 case...
            if (usage_min_max_count && (report_size == 1)) {
              uint32_t uindex = usage[0];
              uint32_t uindex_max = usage[1];
              uint8_t uminmax_index = 0;
              uint32_t u;

              for (uint32_t i = 0; i < report_count; i++) {
                u = uindex;
                if (uindex < uindex_max) uindex++;
                else if (uminmax_index < usage_min_max_count) {
                  uminmax_index++;
                  uindex = usage[uminmax_index * 2];
                  uindex_max = usage[uminmax_index * 2 + 1];
                  //USBHDBGPDBGSerial.printf("$$ next min/max pair: %u %u %u\n", uminmax_index, uindex, uindex_max);
                }

                u |= (uint32_t)usage_page << 16;
                uint32_t n = bitfield(data, bitindex, report_size);
                if (logical_min >= 0) {
                  PDBGSerial.print("  data = ");
                  PDBGSerial.println(n);
                  driver->hid_input_data(u, n);
                } else {
                  int32_t sn = signext(n, report_size);
                  PDBGSerial.print("  sdata = ");
                  PDBGSerial.println(sn);
                  driver->hid_input_data(u, sn);
                }

                bitindex += report_size;
              }

            } else {
              for (uint32_t i = 0; i < report_count; i++) {
                uint32_t u = bitfield(data, bitindex, report_size);
                int n = u;
                if (n >= logical_min && n <= logical_max) {
                  u |= (uint32_t)usage_page << 16;
                  PDBGSerial.print("  usage = ");
                  PDBGSerial.print(u, HEX);
                  PDBGSerial.println("  data = 1");
                  driver->hid_input_data(u, 1);
                } else {
                  PDBGSerial.print("  usage =");
                  PDBGSerial.print(u, HEX);
                  PDBGSerial.print(" out of range: ");
                  PDBGSerial.print(logical_min, HEX);
                  PDBGSerial.print(" ");
                  PDBGSerial.println(logical_max, HEX);
                }
                bitindex += report_size;
              }
            }
          }
        }
        reset_local = true;
        break;
      case 0x90:  // Output
        // TODO.....
        reset_local = true;
        break;
      case 0xB0:  // Feature
        // TODO.....
        reset_local = true;
        break;

      case 0x34:  // Physical Minimum (global)
      case 0x44:  // Physical Maximum (global)
      case 0x54:  // Unit Exponent (global)
      case 0x64:  // Unit (global)
        break;    // Ignore these commonly used tags.  Hopefully not needed?

      case 0xA4:  // Push (yikes! Hope nobody really uses this?!)
      case 0xB4:  // Pop (yikes! Hope nobody really uses this?!)
      case 0x38:  // Designator Index (local)
      case 0x48:  // Designator Minimum (local)
      case 0x58:  // Designator Maximum (local)
      case 0x78:  // String Index (local)
      case 0x88:  // String Minimum (local)
      case 0x98:  // String Maximum (local)
      case 0xA8:  // Delimiter (local)
      default:
        PDBGSerial.print("Ruh Roh, unsupported tag, not a good thing Scoob ");
        PDBGSerial.println(tag, HEX);
        break;
    }
    if (reset_local) {
      usage_count = 0;
      usage_min_max_count = 0;
      usage[0] = 0;
      usage[1] = 0;
    }
  }
}