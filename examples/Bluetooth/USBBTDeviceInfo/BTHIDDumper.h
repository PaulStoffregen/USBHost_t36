#ifndef __HIDDumper_h_
#define __HIDDumper_h_
#include <Arduino.h>
#include <USBHost_t36.h>

class BTHIDDumpController : public BTHIDInput {
public:
  BTHIDDumpController(USBHost &host, uint32_t index = 0, uint32_t usage = 0) : index_(index), fixed_usage_(usage) { init(); }
  uint32_t usage(void) {return usage_;}
  static bool show_raw_data;
  static bool show_formated_data;
  static bool changed_data_only;
  static void dump_hexbytes(const void *ptr, uint32_t len, uint32_t indent);
  
  enum {DNIL=0, DU32, DS32, DU64, DS64, DPB, DLVL};
  typedef struct {
    uint8_t  element_type;
    uint8_t dtype;
    uint16_t element_size;    // size of element
    union {
      uint32_t uw;
      int32_t sw;
      uint64_t luw;
      int64_t lsw;
      uint8_t *pb;
    } data;
  } sdp_element_t;

  int extract_next_SDP_Token(uint8_t *pbElement, int cb_left, sdp_element_t &sdpe);
  void print_sdpe_val(sdp_element_t &sdpe, bool verbose);
  void decode_SDP_buffer(bool verbose_output = false);
  void decode_SDP_Data(bool by_user_command);
  
	virtual operator bool() { return ((btdevice != nullptr) && connection_complete_); } // experiment to see if overriding makes sense here
protected:
  virtual hidclaim_t claim_bluetooth(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint8_t *remoteName, int type); 
//	virtual bool claim_bluetooth(BluetoothController *driver, uint32_t bluetooth_class, uint8_t *remoteName);
	virtual bool process_bluetooth_HID_data(const uint8_t *data, uint16_t length);
	virtual void release_bluetooth();
	virtual bool remoteNameComplete(const uint8_t *remoteName);
	virtual void connectionComplete(void);

private:
  void init();

  void dumpHIDReportDescriptor(uint8_t *pb, uint16_t cb);
  void printUsageInfo(uint8_t usage_page, uint16_t usage);
  void print_input_output_feature_bits(uint8_t val);
  
  bool decode_boot_report1(const uint8_t *data, uint16_t length);


  // Stuff from USBHost HID parse code
  void parse(uint16_t type_and_report_id, const uint8_t *data, uint32_t len);
	enum { TOPUSAGE_LIST_LEN = 6 };
	enum { USAGE_LIST_LEN = 24 };
	uint8_t descriptor[800];
	uint16_t descsize;
	bool use_report_id = true;

//	virtual bool hid_process_in_data(const Transfer_t *transfer) {return false;}
//	virtual bool hid_process_out_data(const Transfer_t *transfer) {return false;}
	virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
	virtual void hid_input_data(uint32_t usage, int32_t value);
	virtual void hid_input_end();


  USBHIDParser *driver_;
	BluetoothController *btdriver_ = nullptr;
  BluetoothConnection *btconnect_ = nullptr;
  uint8_t collections_claimed = 0;
  volatile int hid_input_begin_level_ = 0;
  uint32_t index_;
  uint32_t fixed_usage_;
  volatile  bool connection_complete_ = false;
  
  bool decode_input_boot_data_ = false;
  uint8_t bluetooth_class_low_byte_ = 0;
  
  uint32_t usage_ = 0;
  // Track changing fields. 
   const static int MAX_CHANGE_TRACKED = 512;
  uint32_t usages_[MAX_CHANGE_TRACKED];
  int32_t values_[MAX_CHANGE_TRACKED];
  int count_usages_ = 0;
  int index_usages_ = 0;
  
  uint8_t packet_buffer_[256];
  uint16_t seq_number_ = 0;
  // experiment to see if we can receive data from Feature reports.
  enum {MAX_FEATURE_REPORTS=20};
  uint8_t feature_report_ids_[MAX_FEATURE_REPORTS];
  uint8_t cnt_feature_reports_ = 0;

  // See if we can contribute transfers
  Transfer_t mytransfers[2] __attribute__ ((aligned(32)));
};
#endif // __HIDDumper_h_
