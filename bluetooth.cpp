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
#define println USBHost::println_

/************************************************************/
//  Define mapping VID/PID - to Serial Device type.
/************************************************************/

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

	// Lets try to support the main USB Bluetooth class...
	// http://www.usb.org/developers/defined_class/#BaseClassE0h
	if (dev->bDeviceClass != 0xe0) return false;	// not base class wireless controller
	if ((dev->bDeviceSubClass != 1) || (dev->bDeviceProtocol != 1)) return false; // Bluetooth Programming Interface
	if (type != 0) return false;

	Serial.printf("BluetoothController claim this=%x vid:pid=%x:%x\n    ", (uint32_t)this, dev->idVendor,  dev->idProduct);
	for (uint8_t i=0; i < len; i++) Serial.printf("%x ", descriptors[i]);
	Serial.printf("\n");

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
	while (count_end_points-- && ((rxep == 0) || txep == 0)) {
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
		Serial.printf("Bluetooth end points not found: %d %d\n", rxep, txep);
				return false; // did not find two end points.
	}
	Serial.printf("    rxep=%d(%d) txep=%d(%d)\n", rxep&15, rx_size_, txep, tx_size_);

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
	rxpipe_->callback_function = rx_callback;
	queue_Data_Transfer(rxpipe_, rxbuf_, rx_size_, this);

	txpipe_->callback_function = tx_callback;

	// Send out the reset
	device = dev; // yes this is normally done on return from this but should not hurt if we do it here.
	sendResetHCI();
	pending_control_ = 1; 	// not sure yet on what we need...

	return true;
}


void BluetoothController::disconnect()
{
}



void BluetoothController::control(const Transfer_t *transfer)
{
	println("control callback (bluetooth) ", pending_control_, HEX);
	Serial.printf("control callback (bluetooth): %d : ", pending_control_);
	uint8_t *buffer = (uint8_t*)transfer->buffer;
	for (uint8_t i=0; i < transfer->length; i++) Serial.printf("%x ", buffer[i]);
	Serial.printf("\n");

}

/************************************************************/
//  Interrupt-based Data Movement
/************************************************************/

void BluetoothController::rx_callback(const Transfer_t *transfer)
{
	if (!transfer->driver) return;
	((BluetoothController *)(transfer->driver))->rx_data(transfer);
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
	Serial.printf("BT rx_data(%d): ", len);
	uint8_t *buffer = (uint8_t*)transfer->buffer;
	for (uint8_t i=0; i < len; i++) Serial.printf("%x ", buffer[i]);
	Serial.printf("\n");

    switch(buffer[0]) { // Switch on event type
	    case EV_COMMAND_COMPLETE:
	            if(!buffer[5]) { // Check if command succeeded
	            	Serial.printf("    Command Completed! \n");
	            break;
	        }
    }

	queue_Data_Transfer(rxpipe_, rxbuf_, rx_size_, this);

	if (rx_packet_data_remaining == 0) {
		rx_packet_data_remaining = buffer[1] + 2;	// length of data plus the two bytes at start...
	}
    rx_packet_data_remaining -= len;	// remove the length of this packet from length

    switch (pending_control_) {
    	case 1:
    		{
				static uint8_t HCI_CMD3_10[] = {3, 0x10, 0};  // OCF=3, OGF=4<<2, Parameters=0 ???
				sendHCICommand(HCI_CMD3_10, sizeof(HCI_CMD3_10));	// Read local supported features. ?? look up
				pending_control_++;
			} 
			break;
     	case 2:
     		sendHCIReadLocalVersionInfo();
			pending_control_++;
			break;
     	case 3:
			hciVersion = buffer[6];		// Should do error checking above... 
			Serial.printf("    Local Version: %x\n", hciVersion);
			sendHCIReadBDAddr();
			pending_control_++;
			break;
     	case 4:
     		Serial.printf("   BD Addr");
            for(uint8_t i = 0; i < 6; i++) {
            	my_bdaddr[i] = buffer[6 + i];
            	Serial.printf(":%x", my_bdaddr[i]);
            }
			Serial.printf("\n");
			sendHCIReadBufferSize();
			pending_control_++;
			break;
     	case 5:
			sendHCIReadClassOfDevice();
			pending_control_++;
			break;
     	case 6:
			sendHCIReadLocalName();
			pending_control_++;
			break;
		case 7:
			// received name back... Not sure yet if we received
		    // full name or just start of it...
			Serial.printf("    Local name: ");
			for (uint8_t i=6; i < len; i++) {
				if (buffer[i] == 0) {
					break;
				}
				Serial.printf("%c", buffer[i]);
			}
			Serial.printf("\n");
			if (rx_packet_data_remaining) {
				pending_control_++;	// go to next state
			} else {
				pending_control_ += 2;
				sendHCIReadVoiceSettings();
			}
			break;
		case 8: 
			Serial.printf("    Local name continue: ");
			for (uint8_t i=0; i < len; i++) {
				if (buffer[i] == 0) {
					Serial.printf("\n");
					break;
				}
				Serial.printf("%c", buffer[i]);
			}
			if (rx_packet_data_remaining == 0)  {
				sendHCIReadVoiceSettings();
				pending_control_++;
			}
			break;
		case 9: 
			SendHCICommandReadNumberSupportedIAC();
			pending_control_++;
			break;
		case 10: 
			SendHCICommandReadCurrentIACLAP();
			pending_control_++;
			break;
		case 11: 
			sendHCIClearAllEventFilters();
			pending_control_++;
			break;
		case 12: 
			sendHCIWriteConnectionAcceptTimeout();
			pending_control_++;
			break;
    }
}


void BluetoothController::tx_data(const Transfer_t *transfer)
{
}

void inline BluetoothController::sendHCICommand(uint8_t* data, uint16_t nbytes)
{
	mk_setup(setup, 0x20, 0x0, 0, 0, nbytes);
	queue_Control_Transfer(device, &setup, data, this);
}

void BluetoothController::sendResetHCI() {
	Serial.printf("HCI_RESET called\n");
	static uint8_t HCI_RESET[] = {3, 0xc, 0};  // OCF=3, OGF=3<<2, Parameters=0
	sendHCICommand(HCI_RESET, sizeof(HCI_RESET));	
}

void BluetoothController::sendHCIClearAllEventFilters() {
	Serial.printf("HCI_Set_Event_Filter_Clear called\n");
	static uint8_t HCI_Set_Event_Filter_Clear[] = {5, 0xc, 1, 0};  // OCF=5, OGF=3<<2, Parameters=1, 0
	sendHCICommand(HCI_Set_Event_Filter_Clear, sizeof(HCI_Set_Event_Filter_Clear));	
}

void BluetoothController::sendHCIReadLocalName() {
	Serial.printf("HCI_Read_Local_Name called\n");
	static uint8_t HCI_Read_Local_Name[] = {0x14, 0xc, 0};  // OCF=14, OGF=3<<2, Parameters=0
	sendHCICommand(HCI_Read_Local_Name, sizeof(HCI_Read_Local_Name));	
}

void BluetoothController::sendHCIWriteConnectionAcceptTimeout() {
	// BUGBUG: Hard coded timeout here...
	Serial.printf("Write_Connection_Accept_Timeout called\n");
	static uint8_t Write_Connection_Accept_Timeout[] = {0x16, 0xc, 2, 0, 0x7d};  // OCF=16, OGF=3<<2, Parameters=2, 0, 7d
	sendHCICommand(Write_Connection_Accept_Timeout, sizeof(Write_Connection_Accept_Timeout));	

}



void BluetoothController::sendHCIReadClassOfDevice() {
	Serial.printf("HCI_READ_CLASS_OF_DEVICE called\n");
	static uint8_t HCI_READ_CLASS_OF_DEVICE[] = {0x23, 0xc, 0};  // OCF=23, OGF=3<<2, Parameters=0
	sendHCICommand(HCI_READ_CLASS_OF_DEVICE, sizeof(HCI_READ_CLASS_OF_DEVICE));	
}

void BluetoothController::sendHCIReadVoiceSettings() {
	Serial.printf("HCI_Read_Voice_Setting called\n");
	static uint8_t HCI_Read_Voice_Setting[] = {0x25, 0xc, 0};  // OCF=25, OGF=3<<2, Parameters=0
	sendHCICommand(HCI_Read_Voice_Setting, sizeof(HCI_Read_Voice_Setting));	
}

void BluetoothController::SendHCICommandReadNumberSupportedIAC() {
	Serial.printf("HCI_Read_Number_Of_Supported_IAC\n");
	static uint8_t HCI_Read_Number_Of_Supported_IAC[] = {0x38, 0xc, 0};  // OCF=38, OGF=3<<2, Parameters=0
	sendHCICommand(HCI_Read_Number_Of_Supported_IAC, sizeof(HCI_Read_Number_Of_Supported_IAC));	
}

void BluetoothController::SendHCICommandReadCurrentIACLAP() {
	Serial.printf("HCI_Read_Current_IAC_LAP called\n");
	static uint8_t HCI_Read_Current_IAC_LAP[] = {0x39, 0xc, 0};  // OCF=39, OGF=3<<2, Parameters=0
	sendHCICommand(HCI_Read_Current_IAC_LAP, sizeof(HCI_Read_Current_IAC_LAP));	
}


void BluetoothController::sendHCIReadBufferSize() {
	Serial.printf("HCI_Read_Buffer_Size called\n");
	static uint8_t HCI_Read_Buffer_Size[] = {5, 0x10, 0};  // OCF=5, OGF=4<<2, Parameters=0 ???
	sendHCICommand(HCI_Read_Buffer_Size, sizeof(HCI_Read_Buffer_Size));	
}
void BluetoothController::sendHCIReadBDAddr() {
	Serial.printf("HCI_Read_BD_ADDR called\n");
	static uint8_t HCI_Read_BD_ADDR[] = {9, 0x10, 0};  // OCF=9, OGF=4<<2, Parameters=0 ???
	sendHCICommand(HCI_Read_BD_ADDR, sizeof(HCI_Read_BD_ADDR));	
}

void BluetoothController::sendHCIReadLocalVersionInfo() {
	Serial.printf("HCI_Read_Local_Version_Information called\n");
	static uint8_t HCI_Read_Local_Version_Information[] = {1, 0x10, 0};  // OCF=1, OGF=4<<2, Parameters=0 ???
	sendHCICommand(HCI_Read_Local_Version_Information, sizeof(HCI_Read_Local_Version_Information));	
}
