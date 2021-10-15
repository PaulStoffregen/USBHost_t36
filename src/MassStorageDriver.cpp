/* MSC Teensy36 USB Host Mass Storage library
 * Copyright (c) 2017-2019 Warren Watson.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

//MassStorageDriver.cpp

#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info

#define print   USBHost::print_
#define println USBHost::println_

// Uncomment this to display function usage and sequencing.
//#define DBGprint 1

// Big Endian/Little Endian
#define swap32(x) ((x >> 24) & 0xff) | \
				  ((x << 8) & 0xff0000) | \
				  ((x >> 8) & 0xff00) |  \
                  ((x << 24) & 0xff000000)

void msController::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
	driver_ready_for_device(this);
}

bool msController::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	println("msController claim this=", (uint32_t)this, HEX);
	// only claim at interface level

	if (type != 1) return false;
	if (len < 9+7+7) return false; // Interface descriptor + 2 endpoint decriptors 

	print_hexbytes(descriptors, len);

	uint32_t numendpoint = descriptors[4];
	if (numendpoint < 1) return false; 
	if (descriptors[5] != 8) return false; // bInterfaceClass, 8 = MASS Storage class
	if (descriptors[6] != 6) return false; // bInterfaceSubClass, 6 = SCSI transparent command set (SCSI Standards)
	if (descriptors[7] != 80) return false; // bInterfaceProtocol, 80 = BULK-ONLY TRANSPORT

	bInterfaceNumber = descriptors[2];

	uint8_t desc_index = 9;
	uint8_t in_index = 0xff, out_index = 0xff;

	println("numendpoint=", numendpoint, HEX);
	while (numendpoint--) {
		if ((descriptors[desc_index] != 7) || (descriptors[desc_index+1] != 5)) return false; // not an end point
		if (descriptors[desc_index+3] == 2) {  // Bulk end point
			if (descriptors[desc_index+2] & 0x80)
				in_index = desc_index;
			else
				out_index = desc_index;
		}
		desc_index += 7;	// point to next one...
	}
	if ((in_index == 0xff) || (out_index == 0xff)) return false;	// did not find end point
	endpointIn = descriptors[in_index+2]; // bulk-in descriptor 1 81h
	endpointOut = descriptors[out_index+2]; // bulk-out descriptor 2 02h

	println("endpointIn=", endpointIn, HEX);
	println("endpointOut=", endpointOut, HEX);

	uint32_t sizeIn = descriptors[in_index+4] | (descriptors[in_index+5] << 8);
	println("packet size in (msController) = ", sizeIn);

	uint32_t sizeOut = descriptors[out_index+4] | (descriptors[out_index+5] << 8);
	println("packet size out (msController) = ", sizeOut);
	packetSizeIn = sizeIn;	
	packetSizeOut = sizeOut;	

	uint32_t intervalIn = descriptors[in_index+6];
	uint32_t intervalOut = descriptors[out_index+6];

	println("polling intervalIn = ", intervalIn);
	println("polling intervalOut = ", intervalOut);
	datapipeIn = new_Pipe(dev, 2, endpointIn, 1, packetSizeIn, intervalIn);
	datapipeOut = new_Pipe(dev, 2, endpointOut, 0, packetSizeOut, intervalOut);
	datapipeIn->callback_function = callbackIn;
	datapipeOut->callback_function = callbackOut;

	idVendor = dev->idVendor;
	idProduct = dev->idProduct;
	hubNumber = dev->hub_address;
	deviceAddress = dev->address;
	hubPort = dev->hub_port; // Used for device ID with multiple drives.

	msOutCompleted = false;
	msInCompleted = false;
	msControlCompleted = false;
	deviceAvailable = true;
	msDriveInfo.initialized = false;
	msDriveInfo.connected = true;
#ifdef DBGprint
	print("   connected = ");
	println(msDriveInfo.connected);
	print("   initialized = ");
	println(msDriveInfo.initialized);
#endif	
	return true;
}

void msController::disconnect()
{
	deviceAvailable = false;
	println("Device Disconnected...");
	msDriveInfo.connected = false;
	msDriveInfo.initialized = false;
	memset(&msDriveInfo, 0, sizeof(msDriveInfo_t));

#ifdef DBGprint
	print("   connected ");
	println(msDriveInfo.connected);
	print("   initialized ");
	println(msDriveInfo.initialized);
#endif
}

void msController::control(const Transfer_t *transfer)
{
	println("control CallbackIn (msController)");
	print_hexbytes(report, 8);
	msControlCompleted = true;

}

void msController::callbackIn(const Transfer_t *transfer)
{
	println("msController CallbackIn (static)");
	if (transfer->driver) {
		print("transfer->qtd.token = ");
		println(transfer->qtd.token & 255);
		((msController *)(transfer->driver))->new_dataIn(transfer);
	}
}

void msController::callbackOut(const Transfer_t *transfer)
{
	println("msController CallbackOut (static)");
	if (transfer->driver) {
		print("transfer->qtd.token = ");
		println(transfer->qtd.token & 255);
		((msController *)(transfer->driver))->new_dataOut(transfer);
	}
}

void msController::new_dataOut(const Transfer_t *transfer)
{
	uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
	println("msController dataOut (static)", len, DEC);
	print_hexbytes((uint8_t*)transfer->buffer, (len < 32)? len : 32 );
	msOutCompleted = true; // Last out transaction is completed.
}

void msController::new_dataIn(const Transfer_t *transfer)
{
	uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
	println("msController dataIn (static): ", len, DEC);
	print_hexbytes((uint8_t*)transfer->buffer, (len < 32)? len : 32 );
	if (_read_sectors_callback) {
		_emlastRead = 0; // remember that we received something. 
		(*_read_sectors_callback)(_read_sectors_token, (uint8_t*)transfer->buffer);
		_read_sectors_remaining--;
		if (_read_sectors_remaining > 1) queue_Data_Transfer(datapipeIn, transfer->buffer, len, this);
		if (!_read_sectors_remaining) {
			_read_sectors_callback = nullptr;
			msInCompleted = true; // Last in transaction is completed.
		}
#ifdef DBGprint
		Serial.write('@');
		if ((_read_sectors_remaining & 0x3f) == 0) Serial.printf("\n");
#endif
	}
	else msInCompleted = true; // Last in transaction is completed.
}

// Initialize Mass Storage Device
uint8_t msController::mscInit(void) {
#ifdef DBGprint
	println("mscIint()");
#endif
	uint8_t msResult = MS_CBW_PASS;

	CBWTag = 0;
	uint32_t start = millis();
	// Check if device is connected.
	do {
		if((millis() - start) >= MSC_CONNECT_TIMEOUT) {
			return MS_NO_MEDIA_ERR;  // Not connected Error.
		}
		yield();
	} while(!available());
  
	msReset();
	// delay(500); // Not needed any more.
	maxLUN = msGetMaxLun();

//	msResult = msReportLUNs(&maxLUN);
//println("maxLUN = ");
//println(maxLUN);
//	delay(150);
	//-------------------------------------------------------
	msResult = msStartStopUnit(1);
	msResult = WaitMediaReady();
	if(msResult)
		return msResult;
		
	// Retrieve drive information.
	msDriveInfo.initialized = true;
	msDriveInfo.hubNumber = getHubNumber();			// Which HUB.
	msDriveInfo.hubPort = getHubPort();				// Which HUB port.
	msDriveInfo.deviceAddress = getDeviceAddress();	// Device addreess.
	msDriveInfo.idVendor = getIDVendor();  			// USB Vendor ID.
	msDriveInfo.idProduct = getIDProduct();  		// USB Product ID.
	msResult = msDeviceInquiry(&msInquiry);			// Config Info.
	if(msResult)
		return msResult;
	msResult = msReadDeviceCapacity(&msCapacity);	// Size Info.
	if(msResult)
		return msResult;
	memcpy(&msDriveInfo.inquiry, &msInquiry, sizeof(msInquiryResponse_t));
	memcpy(&msDriveInfo.capacity, &msCapacity, sizeof(msSCSICapacity_t));
	return msResult;
}

//---------------------------------------------------------------------------
// Perform Mass Storage Reset
void msController::msReset(void) {
#ifdef DBGprint
	println("msReset()");
#endif
	mk_setup(setup, 0x21, 0xff, 0, bInterfaceNumber, 0);
	queue_Control_Transfer(device, &setup, NULL, this);
	while (!msControlCompleted) yield();
	msControlCompleted = false;
}

//---------------------------------------------------------------------------
// Get MAX LUN
uint8_t msController::msGetMaxLun(void) {
#ifdef DBGprint
	println("msGetMaxLun()");
#endif
	report[0] = 0;
	mk_setup(setup, 0xa1, 0xfe, 0, bInterfaceNumber, 1);
	queue_Control_Transfer(device, &setup, report, this);
	while (!msControlCompleted) yield();
	msControlCompleted = false;
	maxLUN = report[0];
	return maxLUN;
}

uint8_t msController::WaitMediaReady() {
	uint8_t msResult;
	uint32_t start = millis();
#ifdef DBGprint
	println("WaitMediaReady()");
#endif
	do {
		if((millis() - start) >= MEDIA_READY_TIMEOUT) {
			return MS_UNIT_NOT_READY;  // Not Ready Error.
		}
		msResult = msTestReady();
		yield();
	 } while(msResult == 1);
	return msResult;
}

// Check if drive is connected and Initialized.
uint8_t msController::checkConnectedInitialized(void) {
	uint8_t msResult = MS_CBW_PASS;
#ifdef DBGprint
	print("checkConnectedInitialized()");
#endif
	if(!msDriveInfo.connected) {
		return MS_NO_MEDIA_ERR;
	}
	if(!msDriveInfo.initialized) {
		msResult = mscInit();
		if(msResult != MS_CBW_PASS) return MS_UNIT_NOT_READY; // Not Initialized
	}
	return MS_CBW_PASS;
}

//---------------------------------------------------------------------------
// Send SCSI Command
// Do a complete 3 stage transfer.
uint8_t msController::msDoCommand(msCommandBlockWrapper_t *CBW,	void *buffer)
{
	uint8_t CSWResult = 0;
	mscTransferComplete = false;
#ifdef DBGprint
	println("msDoCommand()");
#endif	
	if(CBWTag == 0xFFFFFFFF) CBWTag = 1;
	// digitalWriteFast(2, HIGH);
	queue_Data_Transfer(datapipeOut, CBW, sizeof(msCommandBlockWrapper_t), this); // Command stage.
	while(!msOutCompleted) yield();
	// digitalWriteFast(2, LOW);
	msOutCompleted = false;
	if((CBW->Flags == CMD_DIR_DATA_IN)) { // Data stage from device.
		queue_Data_Transfer(datapipeIn, buffer, CBW->TransferLength, this);
	while(!msInCompleted) yield();
	// digitalWriteFast(2, HIGH);
	msInCompleted = false;
	} else {							  // Data stage to device.
		queue_Data_Transfer(datapipeOut, buffer, CBW->TransferLength, this);
	while(!msOutCompleted) yield();
	// digitalWriteFast(2, LOW);
	msOutCompleted = false;
	}
	CSWResult = msGetCSW(); // Status stage.
	// All stages of this transfer have completed.
	//Check for special cases. 
	//If test for unit ready command is given then
	//  return the CSW status byte.
	//Bit 0 == 1 == not ready else
	//Bit 0 == 0 == ready.
	//And the Start/Stop Unit command as well.
	if((CBW->CommandData[0] == CMD_TEST_UNIT_READY) ||
	   (CBW->CommandData[0] == CMD_START_STOP_UNIT))
		return CSWResult;
	else // Process possible SCSI errors.
		return msProcessError(CSWResult);
}

//---------------------------------------------------------------------------
// Get Command Status Wrapper
uint8_t msController::msGetCSW(void) {
#ifdef DBGprint
	println("msGetCSW()");
#endif
	msCommandStatusWrapper_t StatusBlockWrapper = (msCommandStatusWrapper_t)
	{
		.Signature = CSW_SIGNATURE,
		.Tag = 0,
		.DataResidue = 0, // TODO: Proccess this if received.
		.Status = 0
	};
	queue_Data_Transfer(datapipeIn, &StatusBlockWrapper, sizeof(StatusBlockWrapper), this);
	while(!msInCompleted) yield();
	msInCompleted = false;
	mscTransferComplete = true;
	if(StatusBlockWrapper.Signature != CSW_SIGNATURE) return msProcessError(MS_CSW_SIG_ERROR); // Signature error
	if(StatusBlockWrapper.Tag != CBWTag) return msProcessError(MS_CSW_TAG_ERROR); // Tag mismatch error
	return StatusBlockWrapper.Status;
}

//---------------------------------------------------------------------------
// Test Unit Ready
uint8_t msController::msTestReady() {
#ifdef DBGprint
	println("msTestReady()");
#endif
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = 0,
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 6,
		.CommandData        = {CMD_TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00}
	};
	queue_Data_Transfer(datapipeOut, &CommandBlockWrapper, sizeof(CommandBlockWrapper), this);
	while(!msOutCompleted) yield();
	msOutCompleted = false;
	return msGetCSW();
}

//---------------------------------------------------------------------------
// Start/Stop unit
uint8_t msController::msStartStopUnit(uint8_t mode) {
#ifdef DBGprint
	println("msStartStopUnit()");
#endif
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = 0,
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 6,
		.CommandData        = {CMD_START_STOP_UNIT, 0x01, 0x00, 0x00, mode, 0x00}
	};
	queue_Data_Transfer(datapipeOut, &CommandBlockWrapper, sizeof(CommandBlockWrapper), this);
	while(!msOutCompleted) yield();
	msOutCompleted = false;
	return msGetCSW();
}

//---------------------------------------------------------------------------
// Read Mass Storage Device Capacity (Number of Blocks and Block Size)
uint8_t msController::msReadDeviceCapacity(msSCSICapacity_t * const Capacity) {
#ifdef DBGprint
	println("msReadDeviceCapacity()");
#endif
	uint8_t result = 0;
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = sizeof(msSCSICapacity_t),
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 10,
		.CommandData        = {CMD_RD_CAPACITY_10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
	};
	result = msDoCommand(&CommandBlockWrapper, Capacity);
	Capacity->Blocks = swap32(Capacity->Blocks);
	Capacity->BlockSize = swap32(Capacity->BlockSize);
	return result;
}

//---------------------------------------------------------------------------
// Do Mass Storage Device Inquiry
uint8_t msController::msDeviceInquiry(msInquiryResponse_t * const Inquiry)
{
#ifdef DBGprint
	println("msDeviceInquiry()");
#endif
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = sizeof(msInquiryResponse_t),
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 6,
		.CommandData        = {CMD_INQUIRY,0x00,0x00,0x00,sizeof(msInquiryResponse_t),0x00}
	};
	return msDoCommand(&CommandBlockWrapper, Inquiry);
}

//---------------------------------------------------------------------------
// Request Sense Data
uint8_t msController::msRequestSense(msRequestSenseResponse_t * const Sense)
{
#ifdef DBGprint
	println("msRequestSense()");
#endif
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = sizeof(msRequestSenseResponse_t),
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 6,
		.CommandData        = {CMD_REQUEST_SENSE, 0x00, 0x00, 0x00, sizeof(msRequestSenseResponse_t), 0x00}
	};
	return msDoCommand(&CommandBlockWrapper, Sense);
}

//---------------------------------------------------------------------------
// Report LUNs
uint8_t msController::msReportLUNs(uint8_t *Buffer)
{
#ifdef DBGprint
	println("msReportLuns()");
#endif
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = MAXLUNS,
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 12,
		.CommandData        = {CMD_REPORT_LUNS, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, MAXLUNS, 0x00, 0x00}
	};
	return msDoCommand(&CommandBlockWrapper, Buffer);
}


//---------------------------------------------------------------------------
// Read Sectors (Multi Sector Capable)
uint8_t msController::msReadBlocks(
									const uint32_t BlockAddress,
									const uint16_t Blocks,
									const uint16_t BlockSize,
									void * sectorBuffer)
	{
	println("msReadBlocks()");
#ifdef DBGprint
	Serial.printf("<<< msReadBlocks(%x %x %x)\n", BlockAddress, Blocks, BlockSize);
#endif
	uint8_t BlockHi = (Blocks >> 8) & 0xFF;
	uint8_t BlockLo = Blocks & 0xFF;
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
	
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = (uint32_t)(Blocks * BlockSize),
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 10,
		.CommandData        = {CMD_RD_10, 0x00,
							  (uint8_t)(BlockAddress >> 24),
							  (uint8_t)(BlockAddress >> 16),
							  (uint8_t)(BlockAddress >> 8),
							  (uint8_t)(BlockAddress & 0xFF),
							   0x00, BlockHi, BlockLo, 0x00}
	};
	return msDoCommand(&CommandBlockWrapper, sectorBuffer);
}

//---------------------------------------------------------------------------
// Read Sectors (Multi Sector Capable)

uint8_t msController::msReadSectorsWithCB(
									const uint32_t BlockAddress,
									const uint16_t Blocks,
									void (*callback)(uint32_t, uint8_t *), 
									uint32_t token)	
	{
#ifdef DBGprint
	Serial.printf("<<< msReadSectorsWithCB(%x %u %x)\n", BlockAddress, Blocks, (uint32_t)callback);
#endif
	if ((callback == nullptr) || (!Blocks)) return MS_CBW_FAIL;

	uint8_t BlockHi = (Blocks >> 8) & 0xFF;
	uint8_t BlockLo = Blocks & 0xFF;
	static const uint16_t BlockSize = 512;

	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
	
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = (uint32_t)(Blocks * BlockSize),
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 10,
		.CommandData        = {CMD_RD_10, 0x00,
							  (uint8_t)(BlockAddress >> 24),
							  (uint8_t)(BlockAddress >> 16),
							  (uint8_t)(BlockAddress >> 8),
							  (uint8_t)(BlockAddress & 0xFF),
							   0x00, BlockHi, BlockLo, 0x00}
	};

	// We need to remember how many blocks and call back function
	_read_sectors_callback = callback;
	_read_sectors_remaining = Blocks;
	_read_sectors_token = token;
	_emlastRead = 0; // reset the timeout. 

	// lets unwrap the msDoCommand here...
	uint8_t CSWResult = 0;
	mscTransferComplete = false;

	if(CBWTag == 0xFFFFFFFF) CBWTag = 1;
	// digitalWriteFast(2, HIGH);
	queue_Data_Transfer(datapipeOut, &CommandBlockWrapper, sizeof(msCommandBlockWrapper_t), this); // Command stage.

	while(!msOutCompleted && (_emlastRead < READ_CALLBACK_TIMEOUT_MS)) yield();
	// digitalWriteFast(2, LOW);

	msOutCompleted = false;

	queue_Data_Transfer(datapipeIn, _read_sector_buffer1, BlockSize, this);
	if (_read_sectors_remaining > 1) 	queue_Data_Transfer(datapipeIn, _read_sector_buffer2, BlockSize, this);

	while(!msInCompleted && (_emlastRead < READ_CALLBACK_TIMEOUT_MS)) ;
	// digitalWriteFast(2, HIGH);

	if (!msInCompleted) {
		// clear this out..
		#ifdef DBGprint
			Serial.printf("!!! msReadBlocks Timed Out(%u)\n", _read_sectors_remaining);
		#endif
		_read_sectors_callback = nullptr;
		_read_sectors_remaining = 0;
		return MS_CBW_FAIL;
	}	

	msInCompleted = false;

	CSWResult = msGetCSW(); // Status stage.
	#ifdef DBGprint
	Serial.printf("  CSWResult: %x CD:%x\n", CSWResult, CommandBlockWrapper.CommandData[0] );
	#endif
	// All stages of this transfer have completed.
	//Check for special cases. 
	//If test for unit ready command is given then
	//  return the CSW status byte.
	//Bit 0 == 1 == not ready else
	//Bit 0 == 0 == ready.
	//And the Start/Stop Unit command as well.
	if((CommandBlockWrapper.CommandData[0] == CMD_TEST_UNIT_READY) ||
	   (CommandBlockWrapper.CommandData[0] == CMD_START_STOP_UNIT))
		return CSWResult;
	return msProcessError(CSWResult);
}


//---------------------------------------------------------------------------
// Write Sectors (Multi Sector Capable)
uint8_t msController::msWriteBlocks(
                                  const uint32_t BlockAddress,
                                  const uint16_t Blocks,
                                  const uint16_t BlockSize,
								  const void * sectorBuffer)
	{
#ifdef DBGprint
	println("msWriteBlocks()");
#endif
	uint8_t BlockHi = (Blocks >> 8) & 0xFF;
	uint8_t BlockLo = Blocks & 0xFF;
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = (uint32_t)(Blocks * BlockSize),
		.Flags              = CMD_DIR_DATA_OUT,
		.LUN                = currentLUN,
		.CommandLength      = 10,
		.CommandData        = {CMD_WR_10, 0x00,
		                      (uint8_t)(BlockAddress >> 24),
							  (uint8_t)(BlockAddress >> 16),
							  (uint8_t)(BlockAddress >> 8),
							  (uint8_t)(BlockAddress & 0xFF),
							  0x00, BlockHi, BlockLo, 0x00}
	};
	return msDoCommand(&CommandBlockWrapper, (void *)sectorBuffer);
}

// Proccess Possible SCSI errors
uint8_t msController::msProcessError(uint8_t msStatus) {
#ifdef DBGprint
	println("msProcessError()");
#endif
	uint8_t msResult = 0;
	switch(msStatus) {
		case MS_CBW_PASS:
			return MS_CBW_PASS;
			break;
		case MS_CBW_PHASE_ERROR:
			print("SCSI Phase Error: ");
			println(msStatus);
			return MS_SCSI_ERROR;
			break;
		case MS_CSW_TAG_ERROR:
			print("CSW Tag Error: ");
			println(MS_CSW_TAG_ERROR);
			return MS_CSW_TAG_ERROR;
			break;
		case MS_CSW_SIG_ERROR:
			print("CSW Signature Error: ");
			println(MS_CSW_SIG_ERROR);
			return MS_CSW_SIG_ERROR;
			break;
		case MS_CBW_FAIL:
			if((msResult = msRequestSense(&msSense))) {
				print("Failed to get sense codes. Returned code: ");
				println(msResult);
			}
			return MS_CBW_FAIL;
			break;
		default:
			print("SCSI Error: ");
			println(msStatus);
			return msStatus;
	}
}
