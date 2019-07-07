/* USB EHCI Host for Teensy 3.6
 * Copyright 2017 Paul Stoffregen (paul@pjrc.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * information about the BlueTooth HCI comes from logic analyzer captures
 * plus... http://affon.narod.ru/BT/bluetooth_app_c10.pdf
 */

#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info

#define print   USBHost::print_
#define println USBHost::println_//#define DEBUG_BT

//#define DEBUG_BT
//#define DEBUG_BT_VERBOSE

#ifndef DEBUG_BT
#undef DEBUG_BT_VERBOSE
void inline DBGPrintf(...) {};
#else
#define DBGPrintf USBHDBGSerial.printf
#endif

#ifndef DEBUG_BT_VERBOSE
void inline VDBGPrintf(...) {};
#else
#define VDBGPrintf USBHDBGSerial.printf
#endif


/************************************************************/
//  Define HCI Commands OGF HIgh byte OCF is low byte... 
//     Actually shifted values...
/************************************************************/
#define HCI_INQUIRY 						0x0401
#define HCI_INQUIRY_CANCEL					0x0402
#define HCI_CREATE_CONNECTION				0x0405
#define HCI_OP_ACCEPT_CONN_REQ				0x0409
#define HCI_LINK_KEY_NEG_REPLY				0x040C
#define HCI_PIN_CODE_REPLY					0x040D
#define HCI_AUTH_REQUESTED					0x0411
#define HCI_OP_REMOTE_NAME_REQ				0x0419
#define HCI_OP_REMOTE_NAME_REQ_CANCEL		0x041a
#define HCI_OP_READ_REMOTE_FEATURES			0x041b
#define HCI_OP_READ_REMOTE_VERSION_INFORMATION 0x041D


#define HCI_Write_Default_Link_Policy_Settings	0x080f
#define HCI_Set_Event_Mask					0x0c01
#define HCI_RESET 							0x0c03
#define HCI_Set_Event_Filter_Clear			0x0c05
#define HCI_Read_Local_Name					0x0c14
#define HCI_Read_Stored_Link_Key			0x0c0d
#define HCI_DELETE_STORED_LINK_KEY			0x0c12
#define HCI_WRITE_LOCAL_NAME				0x0c13
#define Write_Connection_Accept_Timeout		0x0c16
#define HCI_WRITE_SCAN_ENABLE				0x0c1a
#define HCI_Read_Page_Scan_Activity			0x0c1b
#define HCI_READ_CLASS_OF_DEVICE			0x0c23
#define HCI_WRITE_CLASS_OF_DEV				0x0C24
#define HCI_Read_Voice_Setting				0x0c25
#define HCI_Read_Number_Of_Supported_IAC	0x0c38
#define HCI_Read_Current_IAC_LAP			0x0c39
#define HCI_WRITE_INQUIRY_MODE				0x0c45
#define HCI_Read_Page_Scan_Type				0x0c46
#define HCI_WRITE_EIR						0x0c52
#define HCI_WRITE_SSP_MODE					0x0c56
#define HCI_Read_Inquiry_Response_Transmit_Power_Level 0x0c58
#define HCI_WRITE_LE_HOST_SUPPORTED			0x0c6d

#define HCI_Read_Local_Supported_Features	0x1003
#define HCI_Read_Local_Extended_Features	0x1004
#define HCI_Read_Buffer_Size				0x1005
#define HCI_Read_BD_ADDR					0x1009
#define HCI_Read_Local_Version_Information	0x1001
#define HCI_Read_Local_Supported_Commands	0x1002

#define HCI_LE_SET_EVENT_MASK				0x2001
#define HCI_LE_Read_Buffer_Size				0x2002
#define HCI_LE_Read_Local_supported_Features 0x2003
#define HCI_LE_READ_ADV_TX_POWER			0x2007
#define HCI_LE_SET_ADV_DATA					0x2008
#define HCI_LE_SET_SCAN_RSP_DATA			0x2009
#define HCI_LE_READ_WHITE_LIST_SIZE			0x200f
#define HCI_LE_CLEAR_WHITE_LIST				0x2010
#define HCI_LE_Supported_States				0x201c

/* Bluetooth L2CAP PSM - see http://www.bluetooth.org/Technical/AssignedNumbers/logical_link.htm */
#define HID_CTRL_PSM    0x11 // HID_Control PSM Value
#define HID_INTR_PSM    0x13 // HID_Interrupt PSM Value

// Used For Connection Response 
#define PENDING     0x01
#define SUCCESSFUL  0x00


/* L2CAP signaling commands */
#define L2CAP_CMD_COMMAND_REJECT        0x01
#define L2CAP_CMD_CONNECTION_REQUEST    0x02
#define L2CAP_CMD_CONNECTION_RESPONSE   0x03
#define L2CAP_CMD_CONFIG_REQUEST        0x04
#define L2CAP_CMD_CONFIG_RESPONSE       0x05
#define L2CAP_CMD_DISCONNECT_REQUEST    0x06
#define L2CAP_CMD_DISCONNECT_RESPONSE   0x07
#define L2CAP_CMD_INFORMATION_REQUEST   0x0A
#define L2CAP_CMD_INFORMATION_RESPONSE  0x0B

#define HID_THDR_DATA_INPUT				0xa1 
// HID stuff
#define HID_BOOT_PROTOCOL           	0x00
#define HID_RPT_PROTOCOL                0x01

/* HCI Events  */
	enum {EV_INQUIRY_COMPLETE= 0x01,EV_INQUIRY_RESULT= 0x02,EV_CONNECT_COMPLETE= 0x03,EV_INCOMING_CONNECT= 0x04,EV_DISCONNECT_COMPLETE= 0x05
		,EV_AUTHENTICATION_COMPLETE= 0x06,EV_REMOTE_NAME_COMPLETE= 0x07,EV_ENCRYPTION_CHANGE= 0x08,EV_CHANGE_CONNECTION_LINK= 0x09,EV_ROLE_CHANGED= 0x12
		,EV_NUM_COMPLETE_PKT= 0x13,EV_PIN_CODE_REQUEST= 0x16,EV_LINK_KEY_REQUEST= 0x17,EV_LINK_KEY_NOTIFICATION= 0x18,EV_DATA_BUFFER_OVERFLOW= 0x1A
		,EV_MAX_SLOTS_CHANGE= 0x1B,EV_READ_REMOTE_VERSION_INFORMATION_COMPLETE= 0x0C,EV_QOS_SETUP_COMPLETE= 0x0D,EV_COMMAND_COMPLETE= 0x0E,EV_COMMAND_STATUS= 0x0F
		,EV_LOOPBACK_COMMAND= 0x19,EV_PAGE_SCAN_REP_MODE= 0x20, EV_INQUIRY_RESULTS_WITH_RSSI=0x22, EV_EXTENDED_INQUIRY_RESULT=0x2F };




// different modes
enum {PC_RESET = 1, PC_WRITE_CLASS_DEVICE, PC_READ_BDADDR, PC_READ_LOCAL_VERSION, 
			PC_SEND_WRITE_INQUIRE_MODE, PC_SEND_SET_EVENT_MASK, PC_SEND_INQUIRE, 
			PC_INQUIRE_CANCEL=100, PC_AUTHENTICATION_REQUESTED=110, PC_LINK_KEY_NEGATIVE=120, PC_PIN_CODE_REPLY=130,
			PC_WRITE_SCAN_PAGE=200};
//////////////

//////////////


// Setup some states for the TX pipe where we need to chain messages
enum {STATE_TX_SEND_CONNECT_INT=200, STATE_TX_SEND_CONECT_RSP_SUCCESS, STATE_TX_SEND_CONFIG_REQ, STATE_TX_SEND_CONECT_ISR_RSP_SUCCESS, STATE_TX_SEND_CONFIG_ISR_REQ};

// This is a list of all the drivers inherited from the BTHIDInput class.
// Unlike the list of USBDriver (managed in enumeration.cpp), drivers stay
// on this list even when they have claimed a top level collection.
BTHIDInput * BluetoothController::available_bthid_drivers_list = NULL;

void BluetoothController::driver_ready_for_bluetooth(BTHIDInput *driver)
{
	driver->next = NULL;
	if (available_bthid_drivers_list == NULL) {
		available_bthid_drivers_list = driver;
	} else {
		BTHIDInput *last = available_bthid_drivers_list;
		while (last->next) last = last->next;
		last->next = driver;
	}
}

// When a new top level collection is found, this function asks drivers
// if they wish to claim it.  The driver taking ownership of the
// collection is returned, or NULL if no driver wants it.
BTHIDInput * BluetoothController::find_driver(uint32_t device_type, uint8_t *remoteName)
{
	USBHDBGSerial.printf("BluetoothController::find_driver");
	BTHIDInput *driver = available_bthid_drivers_list;
	while (driver) {
		USBHDBGSerial.printf("  driver %x\n", (uint32_t)driver);
		if (driver->claim_bluetooth(this, device_type, remoteName)) {
			USBHDBGSerial.printf("    *** Claimed ***\n");
			return driver;
		}
		driver = driver->next;
	}
	return NULL;
}
//12 01 00 02 FF 01 01 40 5C 0A E8 21 12 01 01 02 03 01
//VendorID = 0A5C, ProductID = 21E8, Version = 0112
//Class/Subclass/Protocol = 255 / 1 / 1
BluetoothController::product_vendor_mapping_t BluetoothController::pid_vid_mapping[] = {
	{ 0xA5C, 0x21E8 }};

/************************************************************/
//  Initialization and claiming of devices & interfaces
/************************************************************/

void BluetoothController::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
	driver_ready_for_device(this);
}

bool BluetoothController::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	// only claim at device level 
	println("BluetoothController claim this=", (uint32_t)this, HEX);

	if (type != 0) return false; // claim at the device level

	// Lets try to support the main USB Bluetooth class...
	// http://www.usb.org/developers/defined_class/#BaseClassE0h
	if (dev->bDeviceClass != 0xe0)  {
		bool special_case_device = false;
		for (uint8_t i=0; i < (sizeof(pid_vid_mapping)/sizeof(pid_vid_mapping[0])); i++) {
			if ((pid_vid_mapping[i].idVendor == dev->idVendor) && (pid_vid_mapping[i].idProduct == dev->idProduct)) {
				special_case_device = true;
				break;
			}
		}
		if (!special_case_device) return false;
	}
	if ((dev->bDeviceSubClass != 1) || (dev->bDeviceProtocol != 1)) return false; // Bluetooth Programming Interface

	DBGPrintf("BluetoothController claim this=%x vid:pid=%x:%x\n    ", (uint32_t)this, dev->idVendor,  dev->idProduct);
	if (len > 512) {
		DBGPrintf("  Descriptor length %d only showing first 512\n    ");
		len = 512;
	}	
	for (uint16_t i=0; i < len; i++) {
		DBGPrintf("%x ", descriptors[i]);
		if ((i & 0x3f) == 0x3f) DBGPrintf("\n    ");
	}
	DBGPrintf("\n  ");

	// Lets try to process the first Interface and get the end points...
	// Some common stuff for both XBoxs
	uint32_t count_end_points = descriptors[4];
	if (count_end_points < 2) return false;
	uint32_t rxep = 0;
	uint32_t rx2ep = 0;
	uint32_t txep = 0;
	uint8_t rx_interval = 0;
	uint8_t rx2_interval = 0;
	uint8_t tx_interval = 0;
	rx_size_ = 0;
	rx2_size_ = 0;
	tx_size_ = 0;
	uint32_t descriptor_index = 9; 
	while (count_end_points-- /*&& ((rxep == 0) || txep == 0) */) {
		if (descriptors[descriptor_index] != 7) return false; // length 7
		if (descriptors[descriptor_index+1] != 5) return false; // ep desc
		if ((descriptors[descriptor_index+4] <= 64)
			&& (descriptors[descriptor_index+5] == 0)) {
			// have a bulk EP size 
			if (descriptors[descriptor_index+2] & 0x80 ) {
				if (descriptors[descriptor_index+3] == 3) 	{ // Interrupt
					rxep = descriptors[descriptor_index+2];
					rx_size_ = descriptors[descriptor_index+4];
					rx_interval = descriptors[descriptor_index+6];
				} else if  (descriptors[descriptor_index+3] == 2) 	{ // bulk
					rx2ep = descriptors[descriptor_index+2];
					rx2_size_ = descriptors[descriptor_index+4];
					rx2_interval = descriptors[descriptor_index+6];
				}
			} else {
				txep = descriptors[descriptor_index+2]; 
				tx_size_ = descriptors[descriptor_index+4];
				tx_interval = descriptors[descriptor_index+6];
			}
		}
		descriptor_index += 7;  // setup to look at next one...
	}
	if ((rxep == 0) || (txep == 0)) {
		USBHDBGSerial.printf("Bluetooth end points not found: %d %d\n", rxep, txep);
				return false; // did not find two end points.
	}
	DBGPrintf("    rxep=%d(%d) txep=%d(%d) rx2ep=%d(%d)\n", rxep&15, rx_size_, txep, tx_size_, 
		rx2ep&15, rx2_size_);

	print("BluetoothController, rxep=", rxep & 15);
	print("(", rx_size_);
	print("), txep=", txep);
	print("(", tx_size_);
	println(")");
	rxpipe_ = new_Pipe(dev, 3, rxep & 15, 1, rx_size_, rx_interval);
	if (!rxpipe_) return false;
	txpipe_ = new_Pipe(dev, 3, txep, 0, tx_size_, tx_interval);
	if (!txpipe_) {
		//free_Pipe(rxpipe_);
		return false;
	}
	rx2pipe_ = new_Pipe(dev, 2, rx2ep & 15, 1, rx2_size_, rx2_interval);
	if (!rx2pipe_)  {
		// Free other pipes...
		return false;
	}

	rxpipe_->callback_function = rx_callback;
	queue_Data_Transfer(rxpipe_, rxbuf_, rx_size_, this);

	rx2pipe_->callback_function = rx2_callback;
	queue_Data_Transfer(rx2pipe_, rx2buf_, rx2_size_, this);

	txpipe_->callback_function = tx_callback;

	// Send out the reset
	device = dev; // yes this is normally done on return from this but should not hurt if we do it here.
	sendResetHCI();
	pending_control_ = PC_RESET; 
	pending_control_tx_ = 0;	// 

	return true;
}


void BluetoothController::disconnect()
{
	USBHDBGSerial.printf("Bluetooth Disconnect");
	if (device_driver_) {
		device_driver_->release_bluetooth();
		device_driver_->remote_name_[0] = 0;
		device_driver_ = nullptr;
	}
	connection_complete_ = false;
}

void BluetoothController::timer_event(USBDriverTimer *whichTimer)
{
	
}


void BluetoothController::control(const Transfer_t *transfer)
{
	println("    control callback (bluetooth) ", pending_control_, HEX);
#ifdef DEBUG_BT_VERBOSE
	DBGPrintf("    Control callback (bluetooth): %d : ", pending_control_);
	uint8_t *buffer = (uint8_t*)transfer->buffer;
	for (uint8_t i=0; i < transfer->length; i++) DBGPrintf("%x ", buffer[i]);
	DBGPrintf("\n");
#endif
}

/************************************************************/
//  Interrupt-based Data Movement
/************************************************************/

void BluetoothController::rx_callback(const Transfer_t *transfer)
{
	if (!transfer->driver) return;
	((BluetoothController *)(transfer->driver))->rx_data(transfer);
}

void BluetoothController::rx2_callback(const Transfer_t *transfer)
{
	if (!transfer->driver) return;
	((BluetoothController *)(transfer->driver))->rx2_data(transfer);
}

void BluetoothController::tx_callback(const Transfer_t *transfer)
{
	if (!transfer->driver) return;
	((BluetoothController *)(transfer->driver))->tx_data(transfer);
}


void BluetoothController::rx_data(const Transfer_t *transfer)
{
	uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
	print_hexbytes((uint8_t*)transfer->buffer, len);
	DBGPrintf("BT rx_data(%d): ", len);
	uint8_t *buffer = (uint8_t*)transfer->buffer;
	for (uint8_t i=0; i < len; i++) DBGPrintf("%x ", buffer[i]);
	DBGPrintf("\n");

	// Note the logical packets returned from the device may be larger
	// than can fit in one of our packets, so we will detect this and
	// the next read will be continue in or rx_buf_ in the next logical 
	// location.  We will only go into process the next logical state
	// when we have the full response read in... 
	if (rx_packet_data_remaining == 0) {	// Previous command was fully handled
		rx_packet_data_remaining = rxbuf_[1] + 2;	// length of data plus the two bytes at start...
	}		
    // Now see if the data 
    rx_packet_data_remaining -= len;	// remove the length of this packet from length

	if (rx_packet_data_remaining == 0) {	// read started at beginning of packet so get the total length of packet
	    switch(rxbuf_[0]) { // Switch on event type
		    case EV_COMMAND_COMPLETE: //0x0e
	        	handle_hci_command_complete();// Check if command succeeded
	            break;
            case EV_COMMAND_STATUS:	//0x0f
            	handle_hci_command_status();
            	break;
			case EV_INQUIRY_COMPLETE: // 0x01
				handle_hci_inquiry_complete();
				break;
			case EV_INQUIRY_RESULT:	  // 0x02
				handle_hci_inquiry_result(false);
				break;
			case EV_CONNECT_COMPLETE:	// 0x03
				handle_hci_connection_complete();
				break;
			case EV_INCOMING_CONNECT:	// 0x04
				handle_hci_incoming_connect();
				break;
			case EV_DISCONNECT_COMPLETE: // 0x05
				handle_hci_disconnect_complete();
				break;
			case EV_AUTHENTICATION_COMPLETE:// 0x06
				handle_hci_authentication_complete();
				break;
			case EV_REMOTE_NAME_COMPLETE: // 0x07
				handle_hci_remote_name_complete();
				break;	
			case EV_READ_REMOTE_VERSION_INFORMATION_COMPLETE:
				handle_hci_remote_version_information_complete();
				break;
			case EV_PIN_CODE_REQUEST: // 0x16
				handle_hci_pin_code_request();
				break;

			case EV_LINK_KEY_REQUEST:	// 0x17
				handle_hci_link_key_request();
				break;
			case EV_LINK_KEY_NOTIFICATION: // 0x18
				handle_hci_link_key_notification();	
				break;
			case EV_INQUIRY_RESULTS_WITH_RSSI:
				handle_hci_inquiry_result(true);
				break;
			case EV_EXTENDED_INQUIRY_RESULT:
				handle_hci_extended_inquiry_result();
				break;
			default:
				break;
	    }
		// Start read at start of buffer. 
		queue_Data_Transfer(rxpipe_, rxbuf_, rx_size_, this);
	} else {
		// Continue the read - Todo - maybe verify len == rx_size_
		queue_Data_Transfer(rxpipe_, buffer + rx_size_, rx_size_, this);
		return;		// Don't process the message yet as we still have data to receive. 
	}
}

//===================================================================
// Called when an HCI command completes.
void BluetoothController::handle_hci_command_complete() 
{
	uint16_t hci_command = rxbuf_[3] + (rxbuf_[4] << 8);
	uint8_t buffer_index;
    if(!rxbuf_[5]) { 
    	VDBGPrintf("    Command Completed! \n");
    } else {
    	VDBGPrintf("    Command(%x) Completed - Error: %d! \n", hci_command, rxbuf_[5]);
    	// BUGBUG:: probably need to queue something? 
    }

	switch (hci_command) {
		case HCI_OP_REMOTE_NAME_REQ:
			break;
		case HCI_RESET:	//0x0c03
			if (!rxbuf_[5]) pending_control_ = PC_WRITE_CLASS_DEVICE;
			//  If it fails, will retry. maybe should have repeat max...  
			break;
		case HCI_Set_Event_Filter_Clear:	//0x0c05
			break;
		case HCI_Read_Local_Name:	//0x0c14
			// received name back... 
			{
				//BUGBUG:: probably want to grab string object and copy to 
				USBHDBGSerial.printf("    Local name: %s\n", &rxbuf_[6]);
/*
				uint8_t len = rxbuf_[1]+2;	// Length field +2 for total bytes read
				for (uint8_t i=6; i < len; i++) {
					if (rxbuf_[i] == 0) {
						break;
					}
					USBHDBGSerial.printf("%c", rxbuf_[i]);
				}
				USBHDBGSerial.printf("\n"); */
			}
			break;
		case Write_Connection_Accept_Timeout:	//0x0c16
			break;
		case HCI_READ_CLASS_OF_DEVICE:	// 0x0c23
			break;
		case HCI_Read_Voice_Setting: 	//0x0c25
			break;
		case HCI_Read_Number_Of_Supported_IAC:	//0x0c38
			break;
		case HCI_Read_Current_IAC_LAP:	//0x0c39
			break;
		case HCI_WRITE_INQUIRY_MODE:	//0x0c45
			break;
		case HCI_Read_Inquiry_Response_Transmit_Power_Level: //0x0c58
			break;
		case HCI_Read_Local_Supported_Features:	//0x1003
			// Remember the features supported by local... 
			for (buffer_index = 0; buffer_index < 8; buffer_index++) {
				features[buffer_index] = rxbuf_[buffer_index+6];
			}
			break;
		case HCI_Read_Buffer_Size:	// 0x1005
			break;
		case HCI_Read_BD_ADDR:	//0x1009
			{
	            for(uint8_t i = 0; i < 6; i++) my_bdaddr_[i] = rxbuf_[6 + i];
		      	DBGPrintf("   BD Addr %x:%x:%x:%x:%x:%x\n", my_bdaddr_[5], my_bdaddr_[4], my_bdaddr_[3], my_bdaddr_[2], my_bdaddr_[1], my_bdaddr_[0]);
			}
			break;
		case HCI_Read_Local_Version_Information:	//0x1001
			hciVersion = rxbuf_[6];		// Should do error checking above... 
			DBGPrintf("    Local Version: %x\n", hciVersion);
			pending_control_ = (do_pair_device_)? PC_SEND_WRITE_INQUIRE_MODE : PC_WRITE_SCAN_PAGE;
			break;
		case HCI_Read_Local_Supported_Commands:	//0x1002
			break;
		case HCI_LE_Read_Buffer_Size:	//0x2002
			break;
		case HCI_LE_Read_Local_supported_Features:	//0x2003
			break;
		case HCI_LE_Supported_States:	//0x201c
			break;

		case HCI_Read_Local_Extended_Features:	//0x1004
			break;
		case HCI_Set_Event_Mask:					//0x0c01
			break;
		case HCI_Read_Stored_Link_Key:			//0x0c0d
			break;
		case HCI_Write_Default_Link_Policy_Settings:	//0x080f
			break;
		case HCI_Read_Page_Scan_Activity:			//0x0c1b
			break;
		case HCI_Read_Page_Scan_Type:				//0x0c46
			break;
		case HCI_LE_SET_EVENT_MASK:				//0x2001
			break;
		case HCI_LE_READ_ADV_TX_POWER:			//0x2007
			break;
		case HCI_LE_READ_WHITE_LIST_SIZE:			//0x200f
			break;
		case HCI_LE_CLEAR_WHITE_LIST:				//0x2010
			break;
		case HCI_DELETE_STORED_LINK_KEY:			//0x0c12
			break;
		case HCI_WRITE_LOCAL_NAME:				//0x0c13
			break;
		case HCI_WRITE_SCAN_ENABLE:				//0x0c1a
			DBGPrintf("Write_Scan_enable Completed\n");
			// See if we have driver and a remote
			if (device_driver_ && connection_complete_) {	// We have a driver call their 
				device_driver_->connectionComplete();
				connection_complete_ = false;	// only call once
			}
			break;
		case HCI_WRITE_SSP_MODE:					//0x0c56
			break;
		case HCI_WRITE_EIR:						//0x0c52
			break;
		case HCI_WRITE_LE_HOST_SUPPORTED:			//0x0c6d
			break;
		case HCI_LE_SET_SCAN_RSP_DATA:			//0x2009
			break;
	}
	// And queue up the next command
	queue_next_hci_command();
}

void BluetoothController::queue_next_hci_command()
{
	// Ok We completed a command now see if we need to queue another command
	// Still probably need to reorganize... 
    switch (pending_control_) {
    		// Initial setup states. 
 		case PC_RESET: 
			sendResetHCI();
			break;
    	case PC_WRITE_CLASS_DEVICE:
    		sendHDCWriteClassOfDev();
    		pending_control_++;
    		break;
     	case PC_READ_BDADDR:
			sendHCIReadBDAddr();
			pending_control_++;
			break;

     	case PC_READ_LOCAL_VERSION:
     		sendHCIReadLocalVersionInfo();
			//pending_control_++;
			break;

		// These are used when we are pairing. 	
	    case PC_SEND_WRITE_INQUIRE_MODE:
			sendHCIHCIWriteInquiryMode(2);  // lets set into extended inquire mode
			pending_control_++;
			break;

	    case PC_SEND_SET_EVENT_MASK:
			sendHCISetEventMask();  // Set the event mask to include extend inquire event
			pending_control_++;
			break;

		case PC_SEND_INQUIRE:
			sendHCI_INQUIRY();
			pending_control_++;
			break;
		case PC_INQUIRE_CANCEL:	
			// lets try to create a connection...
			sendHCICreateConnection();
			pending_control_++;
			break;
		case PC_AUTHENTICATION_REQUESTED:	
			break;
		case PC_LINK_KEY_NEGATIVE:
			break;
		case PC_PIN_CODE_REPLY:
			break;

		// None Pair mode
		case PC_WRITE_SCAN_PAGE:
			sendHCIWriteScanEnable(2);
			pending_control_ = 0;	// 
			break;
		default:	
			break;
    }

}


void BluetoothController::handle_hci_command_status() 
{
	// <event type><param count><status><num packets allowed to be sent><CMD><CMD>
#ifdef DEBUG_BT
	uint16_t hci_command = rxbuf_[4] + (rxbuf_[5] << 8);
	if (rxbuf_[2]) {
		DBGPrintf("    Command %x Status %x - ", hci_command, rxbuf_[2]);
		switch (rxbuf_[2]) {
			case 0x01: DBGPrintf("Unknown HCI Command\n"); break;
			case 0x02: DBGPrintf("Unknown Connection Identifier\n"); break;
			case 0x03: DBGPrintf("Hardware Failure\n"); break;
			case 0x04: DBGPrintf("Page Timeout\n"); break;
			case 0x05: DBGPrintf("Authentication Failure\n"); break;
			case 0x06: DBGPrintf("PIN or Key Missing\n"); break;
			case 0x07: DBGPrintf("Memory Capacity Exceeded\n"); break;
			case 0x08: DBGPrintf("Connection Timeout\n"); break;
			case 0x09: DBGPrintf("Connection Limit Exceeded\n"); break;
			case 0x0A: DBGPrintf("Synchronous Connection Limit To A Device Exceeded\n"); break;
			case 0x0B: DBGPrintf("Connection Already Exists\n"); break;
			case 0x0C: DBGPrintf("Command Disallowed\n"); break;
			case 0x0D: DBGPrintf("Connection Rejected due to Limited Resources\n"); break;
			case 0x0E: DBGPrintf("Connection Rejected Due To Security Reasons\n"); break;
			case 0x0F: DBGPrintf("Connection Rejected due to Unacceptable BD_ADDR\n"); break;
			default: DBGPrintf("???\n"); break;
		}
	} else {
		VDBGPrintf("    Command %x Status %x\n", hci_command, rxbuf_[2]);
	}
#endif
}

void BluetoothController::handle_hci_inquiry_result(bool fRSSI) 
{
	// result - versus result with RSSI
	//                              | Diverge here... 
	//  2 f 1 79 22 23 a c5 cc 1 2  0 40 25 0 3b 2 
	// 22 f 1 79 22 23 a c5 cc 1 2 40 25 0 3e 31 d2

	// Wondered if multiple items if all of the BDADDR are first then next field...
	// looks like it is that way...
	// Section 7.7.2
	if (fRSSI) DBGPrintf("    Inquiry Result with RSSI - Count: %d\n", rxbuf_[2]);
	else DBGPrintf("    Inquiry Result - Count: %d\n", rxbuf_[2]);
	for (uint8_t i=0; i < rxbuf_[2]; i++) {
		uint8_t index_bd = 3 + (i*6);
		uint8_t index_ps = 3 + (6*rxbuf_[2]) + i;
		uint8_t index_class = 3 + (9*rxbuf_[2]) + i;
		uint8_t index_clock_offset = 3 + (12*rxbuf_[2]) + i;
		if (fRSSI) {
			// Handle the differences in offsets here...
			index_class = 3 + (8*rxbuf_[2]) + i;
			index_clock_offset = 3 + (11*rxbuf_[2]) + i;
		} 
		uint32_t bluetooth_class = rxbuf_[index_class] + ((uint32_t)rxbuf_[index_class+1] << 8) + ((uint32_t)rxbuf_[index_class+2] << 16);
		DBGPrintf("      BD:%x:%x:%x:%x:%x:%x, PS:%d, class: %x\n", 
			rxbuf_[index_bd],rxbuf_[index_bd+1],rxbuf_[index_bd+2],rxbuf_[index_bd+3],rxbuf_[index_bd+4],rxbuf_[index_bd+5],
			rxbuf_[index_ps], bluetooth_class);
		// See if we know the class 
		if (((bluetooth_class & 0xff00) == 0x2500) || ((bluetooth_class & 0xff00) == 0x500)) {
			DBGPrintf("      Peripheral device\n");
			if (bluetooth_class & 0x80) DBGPrintf("        Mouse\n");
			if (bluetooth_class & 0x40) DBGPrintf("        Keyboard\n"); 
			switch(bluetooth_class & 0x3c) {
				case 4: DBGPrintf("        Joystick\n"); break;
				case 8: DBGPrintf("        Gamepad\n"); break;
				case 0xc: DBGPrintf("        Remote Control\n"); break;
			}

			// BUGBUG, lets hard code to go to new state...
			for (uint8_t i = 0; i < 6; i++) device_bdaddr_[i] = rxbuf_[index_bd+i];
			device_class_ = bluetooth_class;
			device_driver_ = find_driver(device_class_);

    		device_ps_repetion_mode_  = rxbuf_[index_ps]; // mode
    		device_clock_offset_[0] = rxbuf_[index_clock_offset];
    		device_clock_offset_[1] = rxbuf_[index_clock_offset+1];

			// Now we need to bail from inquiry and setup to try to connect...
			sendHCIInquiryCancel();
			pending_control_ = PC_INQUIRE_CANCEL;
			break;
		}
	}
}

void BluetoothController::handle_hci_extended_inquiry_result()
{	
	DBGPrintf("    Extended Inquiry Result - Count: %d\n", rxbuf_[2]);
	// Should always be only one result here. 
	uint8_t index_bd = 3;
	uint8_t index_ps = 9;
	uint8_t index_class = 11;
	uint8_t index_clock_offset = 14;
	//uint8_t index_rssi = 16;
	uint8_t index_eir_data = 17;
	uint8_t index_local_name = 0;
	uint8_t size_local_name = 0;
	uint32_t bluetooth_class = rxbuf_[index_class] + ((uint32_t)rxbuf_[index_class+1] << 8) + ((uint32_t)rxbuf_[index_class+2] << 16);
	DBGPrintf("      BD:%x:%x:%x:%x:%x:%x, PS:%d, class: %x\n", 
		rxbuf_[index_bd],rxbuf_[index_bd+1],rxbuf_[index_bd+2],rxbuf_[index_bd+3],rxbuf_[index_bd+4],rxbuf_[index_bd+5],
		rxbuf_[index_ps], bluetooth_class);
	// Lets see if we can find a name
	while (index_eir_data < 256) {
		if (rxbuf_[index_eir_data] == 0) break; // no more data
		switch (rxbuf_[index_eir_data+1]) {
			case 0x08: // Shortened local name
			case 0x09: // complete local name
				index_local_name = index_eir_data+2;
				size_local_name = rxbuf_[index_eir_data]-1;
				break;
		}
		index_eir_data += rxbuf_[index_eir_data] + 1;	// point to the next item

	}
	if (index_local_name && size_local_name) {
		// Hack lets null teminate the string
		rxbuf_[index_local_name+size_local_name] = 0;

		DBGPrintf("      Local Name: %s\n", &rxbuf_[index_local_name]);
	}

	// See if we know the class 
	if (((bluetooth_class & 0xff00) == 0x2500) || ((bluetooth_class & 0xff00) == 0x500)) {
		DBGPrintf("      Peripheral device\n");
		if (bluetooth_class & 0x80) DBGPrintf("        Mouse\n");
		if (bluetooth_class & 0x40) DBGPrintf("        Keyboard\n"); 
		switch(bluetooth_class & 0x3c) {
			case 4: DBGPrintf("        Joystick\n"); break;
			case 8: DBGPrintf("        Gamepad\n"); break;
			case 0xc: DBGPrintf("        Remote Control\n"); break;
		}

		// BUGBUG, lets hard code to go to new state...
		for (uint8_t i = 0; i < 6; i++) device_bdaddr_[i] = rxbuf_[index_bd+i];
		device_class_ = bluetooth_class;
		device_driver_ = find_driver(device_class_, index_local_name? &rxbuf_[index_local_name] : nullptr);

		device_ps_repetion_mode_  = rxbuf_[index_ps]; // mode
		device_clock_offset_[0] = rxbuf_[index_clock_offset];
		device_clock_offset_[1] = rxbuf_[index_clock_offset+1];

		// and if we found a driver, save away the name 
		if (device_driver_ && index_local_name && size_local_name) {
			uint8_t buffer_index;
			for (buffer_index = 0; size_local_name && (buffer_index < BTHIDInput::REMOTE_NAME_SIZE-1); buffer_index++) {
				device_driver_->remote_name_[buffer_index] = rxbuf_[index_local_name+buffer_index];
				size_local_name--;
			}
			device_driver_->remote_name_[buffer_index] = 0;	// make sure null terminated
		}

		// Now we need to bail from inquiry and setup to try to connect...
		sendHCIInquiryCancel();
		pending_control_ = PC_INQUIRE_CANCEL;
	}
}

void BluetoothController::handle_hci_inquiry_complete() {
	VDBGPrintf("    Inquiry Complete - status: %d\n", rxbuf_[2]);
}

void BluetoothController::handle_hci_connection_complete() {
	//  0  1  2  3  4  5  6  7  8 9  10 11 12
	//       ST CH CH BD BD BD BD BD BD LT EN
	// 03 0b 04 00 00 40 25 00 58 4b 00 01 00 
	device_connection_handle_ = rxbuf_[3]+ (uint16_t)(rxbuf_[4]<<8);
	DBGPrintf("    Connection Complete - ST:%x LH:%x\n", rxbuf_[2], device_connection_handle_);
	if (do_pair_device_ && !(device_driver_ && (device_driver_->special_process_required & BTHIDInput::SP_DONT_NEED_CONNECT))) {
		sendHCIAuthenticationRequested();
		pending_control_ = PC_AUTHENTICATION_REQUESTED;
	} else if (device_driver_ && (device_driver_->special_process_required & BTHIDInput::SP_NEED_CONNECT)) {
		DBGPrintf("   Needs connect to device(PS4?)\n");
		// The PS4 requires a connection request to it. 
		delay(1);
		sendl2cap_ConnectionRequest(device_connection_handle_, connection_rxid_, control_dcid_, HID_CTRL_PSM);
#if 0		
		delay(1);
		
		uint8_t packet[2];
	    memset(packet, 0, sizeof(packet));
	    packet[0] = 0x43; 
	    packet[1] = 0x02;      // Report ID
	    USBHDBGSerial.printf("SixAxis Command Issued!\r\n");
     	sendL2CapCommand(packet, sizeof(packet), 0x40);	
#endif     	
	}
}

void BluetoothController::handle_hci_incoming_connect() {
	//           BD    BD   BD    BD  BD   BD  CL    CL    CL  LT
	// 0x04 0x0A 0x79 0x22 0x23 0x0A 0xC5 0xCC 0x40 0x05 0x00 0x01
	uint32_t class_of_device  = rxbuf_[8] + (uint16_t)(rxbuf_[9]<<8) + (uint32_t)(rxbuf_[10]<<16);
	DBGPrintf("    Event: Incoming Connect -  %x:%x:%x:%x:%x:%x CL:%x LT:%x\n", 
		rxbuf_[2], rxbuf_[3], rxbuf_[4], rxbuf_[5], rxbuf_[6], rxbuf_[7], class_of_device, rxbuf_[11]);
	if (((class_of_device & 0xff00) == 0x2500) || ((class_of_device & 0xff00) == 0x500)) {
		DBGPrintf("      Peripheral device\n");
		if (class_of_device & 0x80) DBGPrintf("        Mouse\n");
		if (class_of_device & 0x40) DBGPrintf("        Keyboard\n"); 
		switch(class_of_device & 0x3c) {
			case 4: DBGPrintf("        Joystick\n"); break;
			case 8: DBGPrintf("        Gamepad\n"); break;
			case 0xc: DBGPrintf("        Remote Control\n"); break;
		}
		device_driver_ = find_driver(class_of_device);

		// We need to save away the BDADDR and class link type?
		for(uint8_t i=0; i<6; i++) device_bdaddr_[i] = rxbuf_[i+2];
		device_class_ = class_of_device;	
		sendHCIRemoteNameRequest();
	}

//	sendHCIAuthenticationRequested();
//	pending_control_ = PC_AUTHENTICATION_REQUESTED;
}


void BluetoothController::handle_hci_pin_code_request() {
	// 0x16 0x06 0x79 0x22 0x23 0x0A 0xC5 0xCC
	DBGPrintf("    Event: Pin Code Request %x:%x:%x:%x:%x:%x\n", 
		rxbuf_[2], rxbuf_[3], rxbuf_[4], rxbuf_[5], rxbuf_[6], rxbuf_[7]);
	sendHCIPinCodeReply();
	pending_control_ = PC_PIN_CODE_REPLY;
}

void BluetoothController::handle_hci_link_key_request() {
	// 17 6 79 22 23 a c5 cc 
	DBGPrintf("    Event: Link Key Request %x:%x:%x:%x:%x:%x\n", 
		rxbuf_[2], rxbuf_[3], rxbuf_[4], rxbuf_[5], rxbuf_[6], rxbuf_[7]);

	// Now here is where we need to decide to say we have key or tell them to 
	// cancel key...  right now hard code to cancel...
	sendHCILinkKeyNegativeReply();
	pending_control_ = PC_LINK_KEY_NEGATIVE;
}

void BluetoothController::handle_hci_link_key_notification() {
	// 0   1  2  3  4 5  6  7  8  9 10  1  2  3  4  5  6  7  8  9 20  1  2  3 4
	// 18 17 79 22 23 a c5 cc 5e 98 d4 5e bb 15 66 da 67 fe 4f 87 2b 61 46 b4 0 
	DBGPrintf("    Event: Link Key Notificaton %x:%x:%x:%x:%x:%x Type:%x\n    key:", 
		rxbuf_[2], rxbuf_[3], rxbuf_[4], rxbuf_[5], rxbuf_[6], rxbuf_[7], rxbuf_[24]);
	for (uint8_t i = 8; i < 24; i++) DBGPrintf("%02x ", rxbuf_[i]);
	DBGPrintf("\n");

	// Now here is where we need to decide to say we have key or tell them to 
	// cancel key...  right now hard code to cancel...

}

void BluetoothController::handle_hci_disconnect_complete()
{
	//5 4 0 48 0 13
	DBGPrintf("    Event: HCI Disconnect complete(%d): handle: %x, reason:%x\n", rxbuf_[2], 
		rxbuf_[3]+(rxbuf_[4]<<8), rxbuf_[5]);
	if (device_driver_) {
		device_driver_->release_bluetooth();
		device_driver_->remote_name_[0] = 0;
		device_driver_ = nullptr;
    	
    	// Restore to normal... 
    	control_dcid_ = 0x70;
    	interrupt_dcid_ = 0x71;
	}
	// Probably should clear out connection data. 
	device_connection_handle_ = 0;
	device_class_ = 0;	
	memset(device_bdaddr_, 0, sizeof(device_bdaddr_));
}

void BluetoothController::handle_hci_authentication_complete()
{
	//  6 3 13 48 0
	DBGPrintf("    Event: HCI Authentication complete(%d): handle: %x\n", rxbuf_[2], 
		rxbuf_[3]+(rxbuf_[4]<<8));
	// Start up lcap connection...
	connection_rxid_ = 0;
	sendl2cap_ConnectionRequest(device_connection_handle_, connection_rxid_, control_dcid_, HID_CTRL_PSM);
}



void BluetoothController::handle_hci_remote_name_complete() {
	//           STAT bd   bd   bd   bd    bd  bd
	// 0x07 0xFF 0x00 0x79 0x22 0x23 0x0A 0xC5 0xCC 0x42 0x6C 0x75 0x65 0x74 0x6F 0x6F ...
	DBGPrintf("    Event: handle_hci_remote_name_complete(%d)\n", rxbuf_[2]);
	if (rxbuf_[2] == 0) {
		DBGPrintf("    Remote Name: ");
		for (uint8_t *psz = &rxbuf_[9]; *psz; psz++) DBGPrintf("%c", *psz);
		DBGPrintf("\n");
	}

	if (device_driver_) {
		if (!device_driver_->remoteNameComplete(&rxbuf_[9])) {
			device_driver_->release_bluetooth();
			device_driver_ = nullptr;
		}
	}	
	if (!device_driver_) {
		device_driver_ = find_driver(device_class_, &rxbuf_[9]);
		// not sure I should call remote name again, but they already process...
		if (device_driver_) {
			device_driver_->remoteNameComplete(&rxbuf_[9]);
		}
	}
	if (device_driver_) {
		// lets save away the string. 
		uint8_t buffer_index;
		for (buffer_index = 0; buffer_index < BTHIDInput::REMOTE_NAME_SIZE-1; buffer_index++) {
			device_driver_->remote_name_[buffer_index] = rxbuf_[9+buffer_index];
			if (!device_driver_->remote_name_[buffer_index]) break;
		}
		device_driver_->remote_name_[buffer_index] = 0;	// make sure null terminated

		if (device_driver_->special_process_required & BTHIDInput::SP_PS3_IDS) {
			// Real hack see if PS3... 
	    	control_dcid_ = 0x40;
	    	interrupt_dcid_ = 0x41;
	    }
	}

	// If we are in the connection complete mode, then this is a pairing state and needed to call
	// get remote name later. 
	if (connection_complete_) {	
		if (device_driver_) {	// We have a driver call their 
			device_driver_->connectionComplete();
			connection_complete_ = false;	// only call once
		}
	} else {
		sendHCIAcceptConnectionRequest();		
	} 
}

void BluetoothController::handle_hci_remote_version_information_complete() {
	//           STAT bd   bd   bd   bd    bd  bd
	//c 8 0 48 0 5 45 0 0 0
	remote_ver_ = rxbuf_[6];
	remote_man_ = rxbuf_[7]+((uint16_t)rxbuf_[8]<< 8);
	remote_subv_ = rxbuf_[9];
	
	DBGPrintf("    Event: handle_hci_remote_version_information_complete(%d): ", rxbuf_[2]);
	DBGPrintf(" Handle: %x, Ver:%x, Man: %x, SV: %x\n", 
		rxbuf_[3]+((uint16_t)rxbuf_[4]<< 8), remote_ver_, remote_man_, remote_subv_);
	// Lets now try to accept the connection. 
	sendHCIAcceptConnectionRequest();
}

void BluetoothController::rx2_data(const Transfer_t *transfer)
{
	uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
	DBGPrintf("\n=====================\nBT rx2_data(%d): ", len);
	uint8_t *buffer = (uint8_t*)transfer->buffer;
	for (uint8_t i=0; i < len; i++) DBGPrintf("%x ", buffer[i]);
	DBGPrintf("\n");

	// call backs.  See if this is an L2CAP reply. example
	//  HCI      | l2cap 
	//48 20 10 0 | c 0 1 0 | 3 0 8 0 44 0 70 0 0 0 0 0
	// BUGBUG need to do more verification, like the handle
	uint16_t hci_length = buffer[2] + ((uint16_t)buffer[3]<<8);
	uint16_t l2cap_length = buffer[4] + ((uint16_t)buffer[5]<<8);
//	uint16_t rsp_packet_length = buffer[10] + ((uint16_t)buffer[11]<<8);
	if ((hci_length == (l2cap_length + 4)) /*&& (hci_length == (rsp_packet_length+8))*/) {
		// All the lengths appear to be correct...  need to do more...
		switch (buffer[8]) {
			case L2CAP_CMD_CONNECTION_REQUEST:
				process_l2cap_connection_request(&buffer[8]);
				break;
			case L2CAP_CMD_CONNECTION_RESPONSE:
				process_l2cap_connection_response(&buffer[8]);
				break;
			case L2CAP_CMD_CONFIG_REQUEST:
				process_l2cap_config_request(&buffer[8]);
				break;
			case L2CAP_CMD_CONFIG_RESPONSE:
				process_l2cap_config_response(&buffer[8]);
				break;

			case HID_THDR_DATA_INPUT:
				handleHIDTHDRData(buffer);	// Pass the whole buffer...
				break;
			case L2CAP_CMD_COMMAND_REJECT:
				process_l2cap_command_reject(&buffer[8]);
				break;
			case L2CAP_CMD_DISCONNECT_REQUEST:
				process_l2cap_disconnect_request(&buffer[8]);
				break;
		}
	}


	
	

	// Queue up for next read...
	queue_Data_Transfer(rx2pipe_, rx2buf_, rx2_size_, this);
}


void BluetoothController::sendHCICommand(uint16_t hciCommand, uint16_t cParams, const uint8_t* data)
{
	txbuf_[0] = hciCommand & 0xff;
	txbuf_[1] = (hciCommand >> 8) & 0xff;
	txbuf_[2] = cParams;
	if (cParams) {
		memcpy(&txbuf_[3], data, cParams);	// copy in the commands parameters.
	}
	uint8_t nbytes = cParams+3;
	for (uint8_t i=0; i< nbytes; i++) DBGPrintf("%02x ", txbuf_[i]);
	DBGPrintf(")\n");
	mk_setup(setup, 0x20, 0x0, 0, 0, nbytes);
	queue_Control_Transfer(device, &setup, txbuf_, this);
}

//---------------------------------------------
void  BluetoothController::sendHCIHCIWriteInquiryMode(uint8_t inquiry_mode) {
	// Setup Inquiry mode 
	DBGPrintf("HCI_WRITE_INQUIRY_MODE called (");
	sendHCICommand(HCI_WRITE_INQUIRY_MODE, 1, &inquiry_mode);	
}

void BluetoothController::sendHCISetEventMask() {
	// Setup Inquiry mode 
	DBGPrintf("HCI_Set_Event_Mask called (");
	static const uint8_t hci_event_mask_data[8] = {
		// Default: 0x0000 1FFF FFFF FFFF 
		0xff,0xff, 0xff,0xff, 0xff,0x5f, 0x00,0x00};  // default plus extended inquiry mode
	sendHCICommand(HCI_Set_Event_Mask, sizeof(hci_event_mask_data), hci_event_mask_data);	
}

//---------------------------------------------
void BluetoothController::sendHCI_INQUIRY() {
	// Start unlimited inqury, set timeout to max and 
	DBGPrintf("HCI_INQUIRY called (");
	static const uint8_t hci_inquiry_data[ ] = {
			0x33, 0x8B, 0x9E, 	// Bluetooth assigned number LAP 0x9E8B33 General/unlimited inquiry Access mode
			0x30, 0xa};			// Max inquiry time little over minute and up to 10 responses
	sendHCICommand(HCI_INQUIRY, sizeof(hci_inquiry_data), hci_inquiry_data);	
}

//---------------------------------------------
void BluetoothController::sendHCIInquiryCancel() {
	DBGPrintf("HCI_INQUIRY_CANCEL called (");
	sendHCICommand(HCI_INQUIRY_CANCEL, 0, nullptr);	
}

//---------------------------------------------
void BluetoothController::sendHCICreateConnection() {
	DBGPrintf("HCI_CREATE_CONNECTION called (");
	uint8_t connection_data[13];
	//  0    1    2    3    4    5    6    7    8   9    10   11   12
	// BD   BD   BD   BD   BD   BD   PT   PT  PRS   0    CS   CS   ARS
	//0x79 0x22 0x23 0x0A 0xC5 0xCC 0x18 0xCC 0x01 0x00 0x00 0x00 0x00
	//0x05 0x04 0x0D 0x79 0x22 0x23 0x0A 0xC5 0xCC 0x18 0xCC 0x01 0x00 0x00 0x00 0x00
	//  05   04   0d   40   25   00   c4   01   00   18   cc   01   00   00 00     00 

	for (uint8_t i=0; i<6; i++) connection_data[i] = device_bdaddr_[i];
	connection_data[6] = 0x18; //DM1/DH1
	connection_data[7] = 0xcc; //
	connection_data[8] = device_ps_repetion_mode_;  // from device
	connection_data[9] = 0;	//
	connection_data[10] = 0;  // clock offset 
	connection_data[11] = 0;  // clock offset 
	connection_data[12] = 0;  // allow role swith no 
	sendHCICommand(HCI_CREATE_CONNECTION, sizeof(connection_data), connection_data);	
}

//---------------------------------------------
void BluetoothController::sendHCIAcceptConnectionRequest() {
	DBGPrintf("HCI_OP_ACCEPT_CONN_REQ called (");
	uint8_t connection_data[7];

	//  0    1    2    3    4    5    6    7    8   9    10   11   12
	//  BD   BD   BD   BD   BD   BD  role 
	//0x79 0x22 0x23 0x0A 0xC5 0xCC 0x00
	for (uint8_t i=0; i<6; i++) connection_data[i] = device_bdaddr_[i];
	connection_data[6] = 0; // Role as master
	sendHCICommand(HCI_OP_ACCEPT_CONN_REQ, sizeof(connection_data), connection_data);	
}

//---------------------------------------------
void BluetoothController::sendHCIAuthenticationRequested() {
	DBGPrintf("HCI_AUTH_REQUESTED called (");
	uint8_t connection_data[2];
	connection_data[0] = device_connection_handle_ & 0xff;
	connection_data[1] = (device_connection_handle_>>8) & 0xff;
	sendHCICommand(HCI_AUTH_REQUESTED, sizeof(connection_data), connection_data);	
}

//---------------------------------------------
void BluetoothController::sendHCILinkKeyNegativeReply() {
	DBGPrintf("HCI_LINK_KEY_NEG_REPLY called (");
	uint8_t connection_data[6];
	for (uint8_t i=0; i<6; i++) connection_data[i] = device_bdaddr_[i];
	sendHCICommand(HCI_LINK_KEY_NEG_REPLY, sizeof(connection_data), connection_data);	
}

//---------------------------------------------
// BUGBUG:: hard code string for this pass.

void BluetoothController::sendHCIPinCodeReply() {
	// 0x0D 0x04 0x17 0x79 0x22 0x23 0x0A 0xC5 0xCC 0x04 0x30 0x30 0x30 0x30 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
	DBGPrintf("HCI_PIN_CODE_REPLY called (");
	uint8_t connection_data[23];
	uint8_t i;

	for (i=0; i<6; i++) connection_data[i] = device_bdaddr_[i];

	for (i=0; pair_pincode_[i] !=0; i++) connection_data[7+i] = pair_pincode_[i];
	connection_data[6] = i; // remember the length	
	for (uint8_t i=7+connection_data[6]; i<23; i++) connection_data[i] = 0;
	sendHCICommand(HCI_PIN_CODE_REPLY, sizeof(connection_data), connection_data);	
}

//---------------------------------------------
void BluetoothController::sendResetHCI() {
	DBGPrintf("HCI_RESET called (");
	sendHCICommand(HCI_RESET, 0, nullptr);	
}

void BluetoothController::sendHDCWriteClassOfDev() {
	// 0x24 0x0C 0x03 0x04 0x08 0x00
	const static uint8_t device_class_data[] = {BT_CLASS_DEVICE & 0xff, (BT_CLASS_DEVICE >> 8) & 0xff, (BT_CLASS_DEVICE >> 16) & 0xff};
	DBGPrintf("HCI_WRITE_CLASS_OF_DEV called (");
	sendHCICommand(HCI_WRITE_CLASS_OF_DEV, sizeof(device_class_data), device_class_data);	
}



void BluetoothController::sendHCIReadBDAddr() {
	DBGPrintf("HCI_Read_BD_ADDR called (");
	sendHCICommand(HCI_Read_BD_ADDR, 0, nullptr);	
}

void BluetoothController::sendHCIReadLocalVersionInfo() {
	DBGPrintf("HCI_Read_Local_Version_Information called (");
	sendHCICommand(HCI_Read_Local_Version_Information, 0, nullptr);	
}


void BluetoothController::sendHCIWriteScanEnable(uint8_t scan_op)	{//				0x0c1a
// 0x1A 0x0C 0x01 0x02
	DBGPrintf("HCI_WRITE_SCAN_ENABLE called(");
	sendHCICommand(HCI_WRITE_SCAN_ENABLE, 1, &scan_op);
}

void BluetoothController::sendHCIRemoteNameRequest() {		// 0x0419
	//               BD   BD   BD   BD   BD   BD   PS   0    CLK   CLK
	//0x19 0x04 0x0A 0x79 0x22 0x23 0x0A 0xC5 0xCC 0x01 0x00 0x00 0x00

	DBGPrintf("HCI_OP_REMOTE_NAME_REQ called (");
	uint8_t connection_data[10];
	for (uint8_t i=0; i<6; i++) connection_data[i] = device_bdaddr_[i];
	connection_data[6] = 1;	// page scan repeat mode...
	connection_data[7] = 0;	 // 0
	connection_data[8] = 0;	// Clk offset
	connection_data[9] = 0; 
	sendHCICommand(HCI_OP_REMOTE_NAME_REQ, sizeof(connection_data), connection_data);	
}

void BluetoothController::sendHCIRemoteVersionInfoRequest() {		// 0x041D
	//               BD   BD   BD   BD   BD   BD   PS   0    CLK   CLK
	//0x19 0x04 0x0A 0x79 0x22 0x23 0x0A 0xC5 0xCC 0x01 0x00 0x00 0x00

	DBGPrintf("HCI_OP_READ_REMOTE_VERSION_INFORMATION called (");
	uint8_t connection_data[2];
	connection_data[0] = device_connection_handle_ & 0xff;
	connection_data[1] = (device_connection_handle_>>8) & 0xff;
	sendHCICommand(HCI_OP_READ_REMOTE_VERSION_INFORMATION, sizeof(connection_data), connection_data);	
}

// l2cap support functions. 
void BluetoothController::sendl2cap_ConnectionResponse(uint16_t handle, uint8_t rxid, uint16_t dcid, uint16_t scid, uint8_t result) {
 	uint8_t l2capbuf[12];
    l2capbuf[0] = L2CAP_CMD_CONNECTION_RESPONSE; // Code
    l2capbuf[1] = rxid; // Identifier
    l2capbuf[2] = 0x08; // Length
    l2capbuf[3] = 0x00;
    l2capbuf[4] = dcid & 0xff; // Destination CID
    l2capbuf[5] = dcid >> 8;
    l2capbuf[6] = scid & 0xff; // Source CID
    l2capbuf[7] = scid >> 8;
    l2capbuf[8] = result; // Result: Pending or Success
    l2capbuf[9] = 0x00;
    l2capbuf[10] = 0x00; // No further information
    l2capbuf[11] = 0x00;

	DBGPrintf("L2CAP_CMD_CONNECTION_RESPONSE called(");
    sendL2CapCommand(handle, l2capbuf, sizeof(l2capbuf));
}



void BluetoothController::sendl2cap_ConnectionRequest(uint16_t handle, uint8_t rxid, uint16_t scid, uint16_t psm) {
 	uint8_t l2capbuf[8];
    l2capbuf[0] = L2CAP_CMD_CONNECTION_REQUEST; // Code
    l2capbuf[1] = rxid; // Identifier
    l2capbuf[2] = 0x04; // Length
    l2capbuf[3] = 0x00;
    l2capbuf[4] = (uint8_t)(psm & 0xff); // PSM
    l2capbuf[5] = (uint8_t)(psm >> 8);
    l2capbuf[6] = scid & 0xff; // Source CID
    l2capbuf[7] = (scid >> 8) & 0xff;

	DBGPrintf("ConnectionRequest called(");
    sendL2CapCommand(handle, l2capbuf, sizeof(l2capbuf));
}

void BluetoothController::sendl2cap_ConfigRequest(uint16_t handle, uint8_t rxid, uint16_t dcid) {
	 	uint8_t l2capbuf[12];
        l2capbuf[0] = L2CAP_CMD_CONFIG_REQUEST; // Code
        l2capbuf[1] = rxid; // Identifier
        l2capbuf[2] = 0x08; // Length
        l2capbuf[3] = 0x00;
        l2capbuf[4] = dcid & 0xff; // Destination CID
        l2capbuf[5] = (dcid >> 8) & 0xff;
        l2capbuf[6] = 0x00; // Flags
        l2capbuf[7] = 0x00;
        l2capbuf[8] = 0x01; // Config Opt: type = MTU (Maximum Transmission Unit) - Hint
        l2capbuf[9] = 0x02; // Config Opt: length
        l2capbuf[10] = 0xFF; // MTU
        l2capbuf[11] = 0xFF;

		DBGPrintf("L2CAP_ConfigRequest called(");
        sendL2CapCommand(handle, l2capbuf, sizeof(l2capbuf));
}

void BluetoothController::sendl2cap_ConfigResponse(uint16_t handle, uint8_t rxid, uint16_t scid) {
	 	uint8_t l2capbuf[14];
        l2capbuf[0] = L2CAP_CMD_CONFIG_RESPONSE; // Code
        l2capbuf[1] = rxid; // Identifier
        l2capbuf[2] = 0x0A; // Length
        l2capbuf[3] = 0x00;
        l2capbuf[4] = scid & 0xff; // Source CID
        l2capbuf[5] = (scid >> 8) & 0xff;
        l2capbuf[6] = 0x00; // Flag
        l2capbuf[7] = 0x00;
        l2capbuf[8] = 0x00; // Result
        l2capbuf[9] = 0x00;
        l2capbuf[10] = 0x01; // Config
        l2capbuf[11] = 0x02;
        l2capbuf[12] = 0xA0;
        l2capbuf[13] = 0x02;

		DBGPrintf("L2CAP_ConfigResponse called(");
        sendL2CapCommand(handle, l2capbuf, sizeof(l2capbuf));
}


//*******************************************************************
//*******************************************************************
void BluetoothController::tx_data(const Transfer_t *transfer)
{
	println("    tx_data(bluetooth) ", pending_control_, HEX);
#ifdef DEBUG_BT_VERBOSE
	DBGPrintf("tx_data callback (bluetooth): %d : ", pending_control_tx_);
	uint8_t *buffer = (uint8_t*)transfer->buffer;
	for (uint8_t i=0; i < transfer->length; i++) DBGPrintf("%x ", buffer[i]);
	DBGPrintf("\n");
#endif
	switch (pending_control_tx_) {
		case STATE_TX_SEND_CONNECT_INT:
		delay(1);
		connection_rxid_++;
		sendl2cap_ConnectionRequest(device_connection_handle_, connection_rxid_, interrupt_dcid_, HID_INTR_PSM);
		pending_control_tx_ = 0;
		break;
	case STATE_TX_SEND_CONECT_RSP_SUCCESS:
		delay(1);
		// Tell the device we are ready
		sendl2cap_ConnectionResponse(device_connection_handle_, connection_rxid_++, control_dcid_, control_scid_, SUCCESSFUL);
		pending_control_tx_ = STATE_TX_SEND_CONFIG_REQ;
		break;
	case STATE_TX_SEND_CONFIG_REQ:
		delay(1);
		sendl2cap_ConfigRequest(device_connection_handle_, connection_rxid_, control_scid_);
		pending_control_tx_ = 0;
		break;
	case STATE_TX_SEND_CONECT_ISR_RSP_SUCCESS:
		delay(1);
		// Tell the device we are ready
		sendl2cap_ConnectionResponse(device_connection_handle_, connection_rxid_++, interrupt_dcid_, interrupt_scid_, SUCCESSFUL);
		pending_control_tx_ = STATE_TX_SEND_CONFIG_ISR_REQ;
		break;
	case STATE_TX_SEND_CONFIG_ISR_REQ:
		delay(1);
		sendl2cap_ConfigRequest(device_connection_handle_, connection_rxid_, interrupt_scid_);
		pending_control_tx_ = 0;
		break;
	}
}


//*******************************************************************
//                                                                 
// HCI ACL Packets
// HCI Handle Low, HCI_Handle_High (PB, BC), Total length low, TLH  - HCI ACL Data packet
// length Low, length high, channel id low, channel id high - L2CAP header
// code, identifier, length, ... - Control-frame 

/************************************************************/
/*                    L2CAP Commands                        */

// Public wrrapper function
void BluetoothController::sendL2CapCommand(uint8_t* data, uint8_t nbytes, int channel) {
	uint16_t channel_out;
	switch (channel) {
		case CONTROL_SCID:
			channel_out = control_scid_;
			break;
		case INTERRUPT_SCID:
			channel_out = interrupt_scid_;
			break;
		default:
			channel_out = (uint16_t)channel;
	}
	DBGPrintf("sendL2CapCommand: %x %d %x\n", (uint32_t)data, nbytes, channel, channel_out);
	sendL2CapCommand (device_connection_handle_, data, nbytes, channel_out & 0xff, (channel_out >> 8) & 0xff);
}


void BluetoothController::sendL2CapCommand(uint16_t handle, uint8_t* data, uint8_t nbytes, uint8_t channelLow, uint8_t channelHigh)
{
    txbuf_[0] = handle & 0xff; // HCI handle with PB,BC flag
    txbuf_[1] = (((handle >> 8) & 0x0f) | 0x20);
    txbuf_[2] = (uint8_t)((4 + nbytes) & 0xff); // HCI ACL total data length
    txbuf_[3] = (uint8_t)((4 + nbytes) >> 8);
    txbuf_[4] = (uint8_t)(nbytes & 0xff); // L2CAP header: Length
    txbuf_[5] = (uint8_t)(nbytes >> 8);
    txbuf_[6] = channelLow;
    txbuf_[7] = channelHigh;
	if (nbytes) {
		memcpy(&txbuf_[8], data, nbytes);	// copy in the commands parameters.
	}
	nbytes = nbytes+8;
	for (uint8_t i=0; i< nbytes; i++) DBGPrintf("%02x ", txbuf_[i]);
	DBGPrintf(")\n");
	
	if (!queue_Data_Transfer(txpipe_, txbuf_, nbytes, this)) {
		println("sendL2CapCommand failed");
	}
}

void  BluetoothController::process_l2cap_connection_request(uint8_t *data) {
	//       ID   LEN  LEN PSM  PSM  SCID SCID        
	// 0x02 0x02 0x04 0x00 0x11 0x00 0x43 0x00

	uint16_t psm = data[4]+((uint16_t)data[5] << 8); 
	uint16_t scid = data[6]+((uint16_t)data[7] << 8);
	connection_rxid_ = data[1];
	DBGPrintf("    L2CAP Connection Request: ID: %d, PSM: %x, SCID: %x\n",connection_rxid_, psm, scid);

	// Assuming not pair mode Send response like:
	//      RXID Len  LEN  DCID DCID  SCID SCID RES 0     0    0
	// 0x03 0x02 0x08 0x00 0x70 0x00 0x43 0x00 0x01 0x00 0x00 0x00
	if (psm == HID_CTRL_PSM) {
		control_scid_ = scid;
		sendl2cap_ConnectionResponse(device_connection_handle_, connection_rxid_, control_dcid_, control_scid_, PENDING);
		pending_control_tx_ = STATE_TX_SEND_CONECT_RSP_SUCCESS;		
	} else if (psm == HID_INTR_PSM) {
		interrupt_scid_ = scid;
		sendl2cap_ConnectionResponse(device_connection_handle_, connection_rxid_, interrupt_dcid_, interrupt_scid_, PENDING);
		pending_control_tx_ = STATE_TX_SEND_CONECT_ISR_RSP_SUCCESS;		

	}
}

// Process the l2cap_connection_response...
void BluetoothController::process_l2cap_connection_response(uint8_t *data) {

	uint16_t scid = data[4]+((uint16_t)data[5] << 8); 
	uint16_t dcid = data[6]+((uint16_t)data[7] << 8); 

	DBGPrintf("    L2CAP Connection Response: ID: %d, Dest:%x, Source:%x, Result:%x, Status: %x\n",
		data[1], scid, dcid,
		data[8]+((uint16_t)data[9] << 8), data[10]+((uint16_t)data[11] << 8));

	//48 20 10 0 | c 0 1 0 | 3 0 8 0 44 0 70 0 0 0 0 0
	if (dcid == interrupt_dcid_) {
		interrupt_scid_ = scid;
		DBGPrintf("      Interrupt Response\n");
		connection_rxid_++;
		sendl2cap_ConfigRequest(device_connection_handle_, connection_rxid_, scid);
	} else if (dcid == control_dcid_) {
		control_scid_ = scid;
		DBGPrintf("      Control Response\n");
		sendl2cap_ConfigRequest(device_connection_handle_, connection_rxid_, scid);
	}
}

void BluetoothController::process_l2cap_config_request(uint8_t *data) {
	//48 20 10 0 c 0 1 0 *4 2 8 0 70 0 0 0 1 2 30 0
	uint16_t dcid = data[4]+((uint16_t)data[5] << 8); 
	DBGPrintf("    L2CAP config Request: ID: %d, Dest:%x, Flags:%x,  Options: %x %x %x %x\n",
		data[1], dcid, data[6]+((uint16_t)data[7] << 8),
		data[8], data[9], data[10], data[11]);
	// Now see which dest was specified
	if (dcid == control_dcid_) {
		DBGPrintf("      Control Configuration request\n");
		sendl2cap_ConfigResponse(device_connection_handle_, data[1], control_scid_);
	} else if (dcid == interrupt_dcid_) {
		DBGPrintf("      Interrupt Configuration request\n");
		sendl2cap_ConfigResponse(device_connection_handle_, data[1], interrupt_scid_);
	}
}

void BluetoothController::process_l2cap_config_response(uint8_t *data) {
	// 48 20 12 0 e 0 1 0 5 0 a 0 70 0 0 0 0 0 1 2 30 0
	uint16_t scid = data[4]+((uint16_t)data[5] << 8); 
	DBGPrintf("    L2CAP config Response: ID: %d, Source:%x, Flags:%x, Result:%x, Config: %x\n",
		data[1], scid, data[6]+((uint16_t)data[7] << 8),
		data[8]+((uint16_t)data[9] << 8), data[10]+((uint16_t)data[11] << 8));
	if (scid == control_dcid_) {
		// Set HID Boot mode
		// Don't do if PS3...
		if (!(device_driver_->special_process_required & BTHIDInput::SP_PS3_IDS)) {
			setHIDProtocol(HID_BOOT_PROTOCOL);  //
		}
		//setHIDProtocol(HID_RPT_PROTOCOL);  //HID_RPT_PROTOCOL
		if (do_pair_device_ && !(device_driver_ && (device_driver_->special_process_required & BTHIDInput::SP_DONT_NEED_CONNECT))) {
			pending_control_tx_ = STATE_TX_SEND_CONNECT_INT;
		} else if (device_driver_ && (device_driver_->special_process_required & BTHIDInput::SP_NEED_CONNECT)) {
			DBGPrintf("   Needs connect to device INT(PS4?)\n");
			// The PS4 requires a connection request to it. 
			pending_control_tx_ = STATE_TX_SEND_CONNECT_INT;
		} else {
			pending_control_ = 0;
		}
	} else if (scid == interrupt_dcid_) {
		// Enable SCan to page mode
		connection_complete_ = true;
		sendHCIWriteScanEnable(2);
	}
}

void BluetoothController::process_l2cap_command_reject(uint8_t *data) {		
	// 48 20 b 0 7 0 70 0 *1 0 0 0 2 0 4 
	DBGPrintf("    L2CAP command reject: ID: %d, length:%x, Reason:%x,  Data: %x %x \n",
		data[1], data[2] + ((uint16_t)data[3] << 8), data[4], data[5], data[6]);

}

void BluetoothController::process_l2cap_disconnect_request(uint8_t *data) {
	uint16_t dcid = data[4]+((uint16_t)data[5] << 8); 
	uint16_t scid = data[6]+((uint16_t)data[7] << 8); 
	DBGPrintf("    L2CAP disconnect request: ID: %d, Length:%x, Dest:%x, Source:%x\n",
		data[1], data[2] + ((uint16_t)data[3] << 8), dcid, scid);
}


void BluetoothController::setHIDProtocol(uint8_t protocol) {
	// Should verify protocol is boot or report
	uint8_t l2capbuf[1];
    l2capbuf[0] = 0x70 | protocol; // Set Protocol, see Bluetooth HID specs page 33
    DBGPrintf("Set HID Protocol %d (", protocol);
    sendL2CapCommand(device_connection_handle_, l2capbuf, sizeof(l2capbuf), control_scid_ & 0xff, control_scid_ >> 8);
}

void BluetoothController::handleHIDTHDRData(uint8_t *data) {
	// Example
	//                      T HID data
	//48 20 d 0 9 0 71 0 a1 3 8a cc c5 a 23 22 79
	uint16_t len = data[4] + ((uint16_t)data[5] << 8);
	DBGPrintf("HID HDR Data: len: %d, Type: %d\n", len, data[9]);

	// ??? How to parse??? Use HID object??? 
	if (device_driver_) {
		device_driver_->process_bluetooth_HID_data(&data[9], len-1); // We skip the first byte...
	} else {
		switch (data[9]) {
			case 1:
				DBGPrintf("    Keyboard report type\n");
				break;
			case 2: 
				DBGPrintf("    Mouse report type\n");	
				break;
			case 3:	
				DBGPrintf("    Combo keyboard/pointing\n");
				break;
			default:
				DBGPrintf("    Unknown report\n");	
		}
	}
}
