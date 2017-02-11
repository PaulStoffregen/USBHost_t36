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
 */

#include "host.h"

uint32_t periodictable[32] __attribute__ ((aligned(4096), used));
uint8_t port_state;
#define PORT_STATE_DISCONNECTED   0
#define PORT_STATE_DEBOUNCE       1
#define PORT_STATE_RESET          2
#define PORT_STATE_RECOVERY       3
#define PORT_STATE_ACTIVE         4
Device_t *rootdev=NULL;
Transfer_t *async_followup_first=NULL;
Transfer_t *async_followup_last=NULL;
Transfer_t *periodic_followup_first=NULL;
Transfer_t *periodic_followup_last=NULL;

void setup()
{
	// Test board has a USB data mux (this won't be on final Teensy 3.6)
	pinMode(32, OUTPUT);	// pin 32 = USB switch, high=connect device
	digitalWrite(32, LOW);
	pinMode(30, OUTPUT);	// pin 30 = debug info - use oscilloscope
	digitalWrite(30, LOW);
	// Teensy 3.6 has USB host power controlled by PTE6
	PORTE_PCR6 = PORT_PCR_MUX(1);
	GPIOE_PDDR |= (1<<6);
	GPIOE_PSOR = (1<<6); // turn on USB host power
	while (!Serial) ; // wait
	Serial.println("USB Host Testing");
	Serial.print("sizeof Device = ");
	Serial.println(sizeof(Device_t));
	Serial.print("sizeof Pipe = ");
	Serial.println(sizeof(Pipe_t));
	Serial.print("sizeof Transfer = ");
	Serial.println(sizeof(Transfer_t));

	// configure the MPU to allow USBHS DMA to access memory
	MPU_RGDAAC0 |= 0x30000000;
	Serial.print("MPU_RGDAAC0 = ");
	Serial.println(MPU_RGDAAC0, HEX);

	// turn on clocks
	MCG_C1 |= MCG_C1_IRCLKEN;  // enable MCGIRCLK 32kHz
	OSC0_CR |= OSC_ERCLKEN;
	SIM_SOPT2 |= SIM_SOPT2_USBREGEN; // turn on USB regulator
	SIM_SOPT2 &= ~SIM_SOPT2_USBSLSRC; // use IRC for slow clock
	print("power up USBHS PHY");
	SIM_USBPHYCTL |= SIM_USBPHYCTL_USBDISILIM; // disable USB current limit
	//SIM_USBPHYCTL = SIM_USBPHYCTL_USBDISILIM | SIM_USBPHYCTL_USB3VOUTTRG(6); // pg 237
	SIM_SCGC3 |= SIM_SCGC3_USBHSDCD | SIM_SCGC3_USBHSPHY | SIM_SCGC3_USBHS;
	USBHSDCD_CLOCK = 33 << 2;
	print("init USBHS PHY & PLL");
	// init process: page 1681-1682
	USBPHY_CTRL_CLR = (USBPHY_CTRL_SFTRST | USBPHY_CTRL_CLKGATE); // // CTRL pg 1698
	USBPHY_TRIM_OVERRIDE_EN_SET = 1;
	USBPHY_PLL_SIC = USBPHY_PLL_SIC_PLL_POWER | USBPHY_PLL_SIC_PLL_ENABLE |
		USBPHY_PLL_SIC_PLL_DIV_SEL(1) | USBPHY_PLL_SIC_PLL_EN_USB_CLKS;
	// wait for the PLL to lock
	int count=0;
	while ((USBPHY_PLL_SIC & USBPHY_PLL_SIC_PLL_LOCK) == 0) {
		count++;
	}
	Serial.print("PLL locked, waited ");
	Serial.println(count);

	// turn on power to PHY
	USBPHY_PWD = 0;
	delay(10);

	// sanity check, connect 470K pullup & 100K pulldown and watch D+ voltage change
	//USBPHY_ANACTRL_CLR = (1<<10); // turn off both 15K pulldowns... works! :)

	// sanity check, output clocks on pin 9 for testing
	//SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(3); // LPO 1kHz
	//SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(2); // Flash
	//SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(6); // XTAL
	//SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(7); // IRC 48MHz
	//SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(4); // MCGIRCLK
	//CORE_PIN9_CONFIG = PORT_PCR_MUX(5);  // CLKOUT on PTC3 Alt5 (Arduino pin 9)

	// now with the PHY up and running, start up USBHS
	print("begin ehci reset");
	USBHS_USBCMD |= USBHS_USBCMD_RST;
	count = 0;
	while (USBHS_USBCMD & USBHS_USBCMD_RST) {
		count++;
	}
	print(" reset waited ", count);

	init_Device_Pipe_Transfer_memory();
	for (int i=0; i < 32; i++) {
		periodictable[i] = 1;
	}
	port_state = PORT_STATE_DISCONNECTED;

	USBHS_USB_SBUSCFG = 1; //  System Bus Interface Configuration

	// turn on the USBHS controller
	//USBHS_USBMODE = USBHS_USBMODE_TXHSD(5) | USBHS_USBMODE_CM(3); // host mode
	USBHS_USBMODE = USBHS_USBMODE_CM(3); // host mode
	USBHS_USBINTR = 0;
	USBHS_PERIODICLISTBASE = (uint32_t)periodictable;
	USBHS_FRINDEX = 0;
	USBHS_ASYNCLISTADDR = 0;
	USBHS_USBCMD = USBHS_USBCMD_ITC(8) | USBHS_USBCMD_RS |
		USBHS_USBCMD_ASP(3) | USBHS_USBCMD_ASPE |
		USBHS_USBCMD_FS2 | USBHS_USBCMD_FS(1);  // periodic table is 32 pointers

	// turn on the USB port
	//USBHS_PORTSC1 = USBHS_PORTSC_PP;
	USBHS_PORTSC1 |= USBHS_PORTSC_PP;
	//USBHS_PORTSC1 |= USBHS_PORTSC_PFSC; // force 12 Mbit/sec
	//USBHS_PORTSC1 |= USBHS_PORTSC_PHCD; // phy off

	Serial.print("USBHS_ASYNCLISTADDR = ");
	Serial.println(USBHS_ASYNCLISTADDR, HEX);
	Serial.print("USBHS_PERIODICLISTBASE = ");
	Serial.println(USBHS_PERIODICLISTBASE, HEX);
	Serial.print("periodictable = ");
	Serial.println((uint32_t)periodictable, HEX);

	// enable interrupts, after this point interruts to all the work
	NVIC_ENABLE_IRQ(IRQ_USBHS);
	USBHS_USBINTR = USBHS_USBINTR_PCE | USBHS_USBINTR_TIE0;
	USBHS_USBINTR |= USBHS_USBINTR_UEE | USBHS_USBINTR_SEE;
	USBHS_USBINTR |= USBHS_USBINTR_AAE;
	USBHS_USBINTR |= USBHS_USBINTR_UPIE | USBHS_USBINTR_UAIE;

	delay(25);
	Serial.println("Plug in device...");
	digitalWrite(32, HIGH); // connect device

#if 0
	delay(5000);
	Serial.println();
	Serial.println("Ring Doorbell");
	USBHS_USBCMD |= USBHS_USBCMD_IAA;
	if (rootdev) print(rootdev->control_pipe);
#endif
}

void loop()
{
}


void pulse(int usec)
{
	// connect oscilloscope to see these pulses....
	digitalWriteFast(30, HIGH);
	delayMicroseconds(usec);
	digitalWriteFast(30, LOW);
}

// EHCI registers         page  default
// --------------         ----  -------
// USBHS_USBCMD           1599  00080000  USB Command
// USBHS_USBSTS           1602  00000000  USB Status
// USBHS_USBINTR          1606  00000000  USB Interrupt Enable
// USBHS_FRINDEX          1609  00000000  Frame Index Register
// USBHS_PERIODICLISTBASE 1610  undefine  Periodic Frame List Base Address
// USBHS_ASYNCLISTADDR    1612  undefine  Asynchronous List Address
// USBHS_PORTSC1          1619  00002000  Port Status and Control
// USBHS_USBMODE          1629  00005000  USB Mode
// USBHS_GPTIMERnCTL      1591  00000000  General Purpose Timer n Control

// PORT_STATE_DISCONNECTED   0
// PORT_STATE_DEBOUNCE       1
// PORT_STATE_RESET          2
// PORT_STATE_RECOVERY       3
// PORT_STATE_ACTIVE         4


void usbhs_isr(void)
{
	uint32_t stat = USBHS_USBSTS;
	USBHS_USBSTS = stat; // clear pending interrupts
	//stat &= USBHS_USBINTR; // mask away unwanted interrupts
	Serial.println();
	Serial.print("ISR: ");
	Serial.print(stat, HEX);
	Serial.println();
	if (stat & USBHS_USBSTS_UI)  Serial.println(" USB Interrupt");
	if (stat & USBHS_USBSTS_UEI) Serial.println(" USB Error");
	if (stat & USBHS_USBSTS_PCI) Serial.println(" Port Change");
	if (stat & USBHS_USBSTS_FRI) Serial.println(" Frame List Rollover");
	if (stat & USBHS_USBSTS_SEI) Serial.println(" System Error");
	if (stat & USBHS_USBSTS_AAI) Serial.println(" Async Advance (doorbell)");
	if (stat & USBHS_USBSTS_URI) Serial.println(" Reset Recv");
	if (stat & USBHS_USBSTS_SRI) Serial.println(" SOF");
	if (stat & USBHS_USBSTS_SLI) Serial.println(" Suspend");
	if (stat & USBHS_USBSTS_HCH) Serial.println(" Host Halted");
	if (stat & USBHS_USBSTS_RCL) Serial.println(" Reclamation");
	if (stat & USBHS_USBSTS_PS)  Serial.println(" Periodic Sched En");
	if (stat & USBHS_USBSTS_AS)  Serial.println(" Async Sched En");
	if (stat & USBHS_USBSTS_NAKI) Serial.println(" NAK");
	if (stat & USBHS_USBSTS_UAI) Serial.println(" USB Async");
	if (stat & USBHS_USBSTS_UPI) Serial.println(" USB Periodic");
	if (stat & USBHS_USBSTS_TI0) Serial.println(" Timer0");
	if (stat & USBHS_USBSTS_TI1) Serial.println(" Timer1");

	if (stat & USBHS_USBSTS_UAI) { // completed qTD(s) from the async schedule
		Serial.println("Async Followup");
		print(async_followup_first, async_followup_last);
		Transfer_t *p = async_followup_first;
		while (p) {
			if (followup_Transfer(p)) {
				// transfer completed
				Transfer_t *next = p->next_followup;
				remove_from_async_followup_list(p);
				free_Transfer(p);
				p = next;
			} else {
				// transfer still pending
				p = p->next_followup;
			}
		}
		print(async_followup_first, async_followup_last);
	}
	if (stat & USBHS_USBSTS_UPI) { // completed qTD(s) from the periodic schedule
		Serial.println("Periodic Followup");
		Transfer_t *p = periodic_followup_first;
		while (p) {
			if (followup_Transfer(p)) {
				// transfer completed
				Transfer_t *next = p->next_followup;
				remove_from_periodic_followup_list(p);
				free_Transfer(p);
				p = next;
			} else {
				// transfer still pending
				p = p->next_followup;
			}
		}
	}

	if (stat & USBHS_USBSTS_PCI) { // port change detected
		const uint32_t portstat = USBHS_PORTSC1;
		Serial.print("port change: ");
		Serial.print(portstat, HEX);
		Serial.println();
		USBHS_PORTSC1 = portstat | (USBHS_PORTSC_OCC|USBHS_PORTSC_PEC|USBHS_PORTSC_CSC);
		if (portstat & USBHS_PORTSC_OCC) {
			Serial.println("  overcurrent change");
		}
		if (portstat & USBHS_PORTSC_CSC) {
			if (portstat & USBHS_PORTSC_CCS) {
				Serial.println("    connect");
				if (port_state == PORT_STATE_DISCONNECTED
				  || port_state == PORT_STATE_DEBOUNCE) {
					// 100 ms debounce (USB 2.0: TATTDB, page 150 & 188)
					port_state = PORT_STATE_DEBOUNCE;
					USBHS_GPTIMER0LD = 100000; // microseconds
					USBHS_GPTIMER0CTL =
						USBHS_GPTIMERCTL_RST | USBHS_GPTIMERCTL_RUN;
					stat &= ~USBHS_USBSTS_TI0;
				}
			} else {
				Serial.println("    disconnect");
				port_state = PORT_STATE_DISCONNECTED;
				USBPHY_CTRL_CLR = USBPHY_CTRL_ENHOSTDISCONDETECT;
				// TODO: delete & clean up device state...
			}
		}
		if (portstat & USBHS_PORTSC_PEC) {
			// PEC bit only detects disable
			Serial.println("  disable");
		} else if (port_state == PORT_STATE_RESET && portstat & USBHS_PORTSC_PE) {
			Serial.println("  port enabled");
			port_state = PORT_STATE_RECOVERY;
			// 10 ms reset recover (USB 2.0: TRSTRCY, page 151 & 188)
			USBHS_GPTIMER0LD = 10000; // microseconds
			USBHS_GPTIMER0CTL = USBHS_GPTIMERCTL_RST | USBHS_GPTIMERCTL_RUN;
			if (USBHS_PORTSC1 & USBHS_PORTSC_HSP) {
				// turn on high-speed disconnect detector
				USBPHY_CTRL_SET = USBPHY_CTRL_ENHOSTDISCONDETECT;
			}
		}
		if (portstat & USBHS_PORTSC_FPR) {
			Serial.println("  force resume");

		}
		 pulse(1);
	}
	if (stat & USBHS_USBSTS_TI0) { // timer 0
		Serial.println("timer");
		 pulse(2);
		if (port_state == PORT_STATE_DEBOUNCE) {
			port_state = PORT_STATE_RESET;
			USBHS_PORTSC1 |= USBHS_PORTSC_PR; // begin reset sequence
			Serial.println("  begin reset");
		} else if (port_state == PORT_STATE_RECOVERY) {
			port_state = PORT_STATE_ACTIVE;
			Serial.println("  end recovery");

			//  HCSPARAMS  TTCTRL  page 1671
			uint32_t speed = (USBHS_PORTSC1 >> 26) & 3;
			rootdev = new_Device(speed, 0, 0);
		}
	}

}

void mk_setup(setup_t &s, uint32_t bmRequestType, uint32_t bRequest,
		uint32_t wValue, uint32_t wIndex, uint32_t wLength)
{
	s.word1 = bmRequestType | (bRequest << 8) | (wValue << 16);
	s.word2 = wIndex | (wLength << 16);
}

static uint8_t enumbuf[256] __attribute__ ((aligned(16)));

void enumeration(const Transfer_t *transfer)
{
	uint32_t len;

	Serial.print("      CALLBACK: ");
	print_hexbytes(transfer->buffer, transfer->length);
	//print(transfer);
	Device_t *dev = transfer->pipe->device;

	while (1) {
		// Within this large switch/case, "break" means we've done
		// some work, but more remains to be done in a different
		// state.  Generally break is used after parsing received
		// data, but what happens next could be different states.
		// When completed, return is used.  Generally, return happens
		// only after a new control transfer is queued, or when
		// enumeration is complete and no more communication is needed.
		switch (dev->enum_state) {
		case 0: // read 8 bytes of device desc, set max packet, and send set address
			pipe_set_maxlen(dev->control_pipe, enumbuf[7]);
			mk_setup(dev->setup, 0, 5, assign_addr(), 0, 0); // 5=SET_ADDRESS
			new_Transfer(dev->control_pipe, NULL, 0);
			dev->enum_state = 1;
			return;
		case 1: // request all 18 bytes of device descriptor
			pipe_set_addr(dev->control_pipe, dev->setup.wValue);
			mk_setup(dev->setup, 0x80, 6, 0x0100, 0, 18); // 6=GET_DESCRIPTOR
			new_Transfer(dev->control_pipe, enumbuf, 18);
			dev->enum_state = 2;
			return;
		case 2: // parse 18 device desc bytes
			dev->bDeviceClass = enumbuf[4];
			dev->bDeviceSubClass = enumbuf[5];
			dev->bDeviceProtocol = enumbuf[6];
			dev->idVendor = enumbuf[8] | (enumbuf[9] << 8);
			dev->idProduct = enumbuf[10] | (enumbuf[11] << 8);
			enumbuf[0] = enumbuf[14];
			enumbuf[1] = enumbuf[15];
			enumbuf[2] = enumbuf[16];
			if ((enumbuf[0] | enumbuf[1] | enumbuf[2]) > 0) {
				dev->enum_state = 3;
			} else {
				dev->enum_state = 11;
			}
			break;
		case 3: // request Language ID
			len = sizeof(enumbuf) - 4;
			mk_setup(dev->setup, 0x80, 6, 0x0300, 0, len); // 6=GET_DESCRIPTOR
			new_Transfer(dev->control_pipe, enumbuf + 4, len);
			dev->enum_state = 4;
			return;
		case 4: // parse Language ID
			if (enumbuf[4] < 4 || enumbuf[5] != 3) {
				dev->enum_state = 11;
			} else {
				dev->LanguageID = enumbuf[6] | (enumbuf[7] << 8);
				if (enumbuf[0]) dev->enum_state = 5;
				else if (enumbuf[1]) dev->enum_state = 7;
				else if (enumbuf[2]) dev->enum_state = 9;
				else dev->enum_state = 11;
			}
			break;
		case 5: // request Manufacturer string
			len = sizeof(enumbuf) - 4;
			mk_setup(dev->setup, 0x80, 6, 0x0300 | enumbuf[0], dev->LanguageID, len);
			new_Transfer(dev->control_pipe, enumbuf + 4, len);
			dev->enum_state = 6;
			return;
		case 6: // parse Manufacturer string
			// TODO: receive the string...
			if (enumbuf[1]) dev->enum_state = 7;
			else if (enumbuf[2]) dev->enum_state = 9;
			else dev->enum_state = 11;
			break;
		case 7: // request Product string
			len = sizeof(enumbuf) - 4;
			mk_setup(dev->setup, 0x80, 6, 0x0300 | enumbuf[1], dev->LanguageID, len);
			new_Transfer(dev->control_pipe, enumbuf + 4, len);
			dev->enum_state = 8;
			return;
		case 8: // parse Product string
			// TODO: receive the string...
			if (enumbuf[2]) dev->enum_state = 9;
			else dev->enum_state = 11;
			break;
		case 9: // request Serial Number string
			len = sizeof(enumbuf) - 4;
			mk_setup(dev->setup, 0x80, 6, 0x0300 | enumbuf[2], dev->LanguageID, len);
			new_Transfer(dev->control_pipe, enumbuf + 4, len);
			dev->enum_state = 10;
			return;
		case 10: // parse Serial Number string
			// TODO: receive the string...
			dev->enum_state = 11;
			break;
		case 11: // request first 9 bytes of config desc
			mk_setup(dev->setup, 0x80, 6, 0x0200, 0, 9); // 6=GET_DESCRIPTOR
			new_Transfer(dev->control_pipe, enumbuf, 9);
			dev->enum_state = 12;
			return;
		case 12: // read 9 bytes, request all of config desc
			len = enumbuf[2] | (enumbuf[3] << 8);
			Serial.print("Config data length = ");
			Serial.println(len);
			if (len > sizeof(enumbuf)) {
				// TODO: how to handle device with too much config data
			}
			mk_setup(dev->setup, 0x80, 6, 0x0200, 0, len); // 6=GET_DESCRIPTOR
			new_Transfer(dev->control_pipe, enumbuf, len);
			dev->enum_state = 13;
			return;
		case 13: // read all config desc, send set config
			Serial.print("bNumInterfaces = ");
			Serial.println(enumbuf[4]);
			Serial.print("bConfigurationValue = ");
			Serial.println(enumbuf[5]);
			// TODO: actually do something with interface descriptor?
			mk_setup(dev->setup, 0, 9, enumbuf[5], 0, 0); // 9=SET_CONFIGURATION
			new_Transfer(dev->control_pipe, NULL, 0);
			dev->enum_state = 14;
			return;
		case 14: // device is now configured
			// TODO: initialize drivers??
			dev->enum_state = 15;
			return;
		case 15: // control transfers for other stuff??
		default:
			return;
		}
	}
}

uint32_t assign_addr(void)
{
	return 29; // TODO: when multiple devices, assign a unique address
}

void pipe_set_maxlen(Pipe_t *pipe, uint32_t maxlen)
{
	Serial.print("pipe_set_maxlen ");
	Serial.println(maxlen);
	pipe->qh.capabilities[0] = (pipe->qh.capabilities[0] & 0x8000FFFF) | (maxlen << 16);
}

void pipe_set_addr(Pipe_t *pipe, uint32_t addr)
{
	Serial.print("pipe_set_addr ");
	Serial.println(addr);
	pipe->qh.capabilities[0] = (pipe->qh.capabilities[0] & 0xFFFFFF80) | addr;
}

uint32_t pipe_get_addr(Pipe_t *pipe)
{
	return pipe->qh.capabilities[0] & 0xFFFFFF80;
}


// Create a new device and begin the enumeration process
//
Device_t * new_Device(uint32_t speed, uint32_t hub_addr, uint32_t hub_port)
{
	Device_t *dev;

	Serial.print("new_Device: ");
	switch (speed) {
	  case 0: Serial.print("12"); break;
	  case 1: Serial.print("1.5"); break;
	  case 2: Serial.print("480"); break;
	  default: Serial.print("??");
	}
	Serial.println(" Mbit/sec");
	dev = allocate_Device();
	if (!dev) return NULL;
	memset(dev, 0, sizeof(Device_t));
	dev->speed = speed;
	dev->address = 0;
	dev->hub_address = hub_addr;
	dev->hub_port = hub_port;
	dev->control_pipe = new_Pipe(dev, 0, 0, 0, 8);
	if (!dev->control_pipe) {
		free_Device(dev);
		return NULL;
	}
	dev->control_pipe->callback_function = &enumeration;
	dev->control_pipe->direction = 1; // 1=IN
	mk_setup(dev->setup, 0x80, 6, 0x0100, 0, 8); // 6=GET_DESCRIPTOR
	new_Transfer(dev->control_pipe, enumbuf, 8);

	return dev;
}



static uint32_t QH_capabilities1(uint32_t nak_count_reload, uint32_t control_endpoint_flag,
	uint32_t max_packet_length, uint32_t head_of_list, uint32_t data_toggle_control,
	uint32_t speed, uint32_t endpoint_number, uint32_t inactivate, uint32_t address)
{
	return ( (nak_count_reload << 28) | (control_endpoint_flag << 27) |
		(max_packet_length << 16) | (head_of_list << 15) |
		(data_toggle_control << 14) | (speed << 12) | (endpoint_number << 8) |
		(inactivate << 7) | (address << 0) );
}

static uint32_t QH_capabilities2(uint32_t high_bw_mult, uint32_t hub_port_number,
	uint32_t hub_address, uint32_t split_completion_mask, uint32_t interrupt_schedule_mask)
{
        return ( (high_bw_mult << 30) | (hub_port_number << 23) | (hub_address << 16) |
		(split_completion_mask << 8) | (interrupt_schedule_mask << 0) );
}

// Create a new pipe.  It's QH is added to the async or periodic schedule,
// and a halt qTD is added to the QH, so we can grow the qTD list later.
//
Pipe_t * new_Pipe(Device_t *dev, uint32_t type, uint32_t endpoint, uint32_t direction,
	uint32_t max_packet_len)
{
	Pipe_t *pipe;
	Transfer_t *halt;
	uint32_t c=0, dtc=0;

	Serial.println("new_Pipe");
	pipe = allocate_Pipe();
	if (!pipe) return NULL;
	halt = allocate_Transfer();
	if (!halt) {
		free_Pipe(pipe);
		return NULL;
	}
	memset(pipe, 0, sizeof(Pipe_t));
	memset(halt, 0, sizeof(Transfer_t));
	halt->qtd.next = 1;
	halt->qtd.token = 0x40;
	pipe->device = dev;
	pipe->qh.next = (uint32_t)halt;
	pipe->qh.alt_next = 1;
	pipe->direction = direction;
	pipe->type = type;
	if (type == 0) {
		// control
		if (dev->speed < 2) c = 1;
		dtc = 1;
	} else if (type == 2) {
		// bulk
	} else if (type == 3) {
		// interrupt
	}
	pipe->qh.capabilities[0] = QH_capabilities1(15, c, max_packet_len, 0,
		dtc, dev->speed, endpoint, 0, dev->address);
	pipe->qh.capabilities[1] = QH_capabilities2(1, dev->hub_port,
		dev->hub_address, 0, 0);

	if (type == 0 || type == 2) {
		// control or bulk: add to async queue
		Pipe_t *list = (Pipe_t *)USBHS_ASYNCLISTADDR;
		if (list == NULL) {
			pipe->qh.capabilities[0] |= 0x8000; // H bit
			pipe->qh.horizontal_link = (uint32_t)&(pipe->qh) | 2; // 2=QH
			USBHS_ASYNCLISTADDR = (uint32_t)&(pipe->qh);
			USBHS_USBCMD |= USBHS_USBCMD_ASE; // enable async schedule
			Serial.println("  first in async list");
		} else {
			// EHCI 1.0: section 4.8.1, page 72
			pipe->qh.horizontal_link = list->qh.horizontal_link;
			list->qh.horizontal_link = (uint32_t)&(pipe->qh) | 2;
			Serial.println("  added to async list");
		}
	} else if (type == 3) {
		// interrupt: add to periodic schedule
		// TODO: link it into the periodic table
	}
	return pipe;
}



// Fill in the qTD fields (token & data)
//   t       the Transfer qTD to initialize
//   buf     data to transfer
//   len     length of data
//   pid     type of packet: 0=OUT, 1=IN, 2=SETUP
//   data01  value of DATA0/DATA1 toggle on 1st packet
//   irq     whether to generate an interrupt when transfer complete
//
void init_qTD(volatile Transfer_t *t, void *buf, uint32_t len,
              uint32_t pid, uint32_t data01, bool irq)
{
	t->qtd.alt_next = 1; // 1=terminate
	if (data01) data01 = 0x80000000;
	t->qtd.token = data01 | (len << 16) | (irq ? 0x8000 : 0) | (pid << 8) | 0x80;
	uint32_t addr = (uint32_t)buf;
	t->qtd.buffer[0] = addr;
	addr &= 0xFFFFF000;
	t->qtd.buffer[1] = addr + 0x1000;
	t->qtd.buffer[2] = addr + 0x2000;
	t->qtd.buffer[3] = addr + 0x3000;
	t->qtd.buffer[4] = addr + 0x4000;
}


// Create a Transfer and queue it
//
bool new_Transfer(Pipe_t *pipe, void *buffer, uint32_t len)
{
	Serial.println("new_Transfer");
	Transfer_t *transfer = allocate_Transfer();
	if (!transfer) return false;
	if (pipe->type == 0) {
		// control transfer
		Transfer_t *data, *status;
		uint32_t status_direction;
		if (len > 16384) {
			// hopefully we never need more
			// than 16K in a control transfer
			free_Transfer(transfer);
			return false;
		}
		status = allocate_Transfer();
		if (!status) {
			free_Transfer(transfer);
			return false;
		}
		if (len > 0) {
			data = allocate_Transfer();
			if (!data) {
				free_Transfer(transfer);
				free_Transfer(status);
				return false;
			}
			init_qTD(data, buffer, len, pipe->direction, 1, false);
			transfer->qtd.next = (uint32_t)data;
			data->qtd.next = (uint32_t)status;
			status_direction = pipe->direction ^ 1;
		} else {
			transfer->qtd.next = (uint32_t)status;
			status_direction = 1; // always IN, USB 2.0 page 226
		}
		Serial.print("setup address ");
		Serial.println((uint32_t)&pipe->device->setup, HEX);
		init_qTD(transfer, &pipe->device->setup, 8, 2, 0, false);
		init_qTD(status, NULL, 0, status_direction, 1, true);
		status->pipe = pipe;
		status->buffer = buffer;
		status->length = len;
		status->qtd.next = 1;
	} else {
		// bulk, interrupt or isochronous transfer
		free_Transfer(transfer);
		return false;
	}
	// find halt qTD
	Transfer_t *halt = (Transfer_t *)(pipe->qh.next);
	while (!(halt->qtd.token & 0x40)) halt = (Transfer_t *)(halt->qtd.next);
	// transfer's token
	uint32_t token = transfer->qtd.token;
	// transfer becomes new halt qTD
	transfer->qtd.token = 0x40;
	// copy transfer non-token fields to halt
	halt->qtd.next = transfer->qtd.next;
	halt->qtd.alt_next = transfer->qtd.alt_next;
	halt->qtd.buffer[0] = transfer->qtd.buffer[0]; // TODO: optimize...
	halt->qtd.buffer[1] = transfer->qtd.buffer[1];
	halt->qtd.buffer[2] = transfer->qtd.buffer[2];
	halt->qtd.buffer[3] = transfer->qtd.buffer[3];
	halt->qtd.buffer[4] = transfer->qtd.buffer[4];
	halt->pipe = pipe;
	// find the last qTD we're adding
	Transfer_t *last = halt;
	while ((uint32_t)(last->qtd.next) != 1) last = (Transfer_t *)(last->qtd.next);
	// last points to transfer (which becomes new halt)
	last->qtd.next = (uint32_t)transfer;
	transfer->qtd.next = 1;
	// link all the new qTD by next_followup & prev_followup
	Transfer_t *prev = NULL;
	Transfer_t *p = halt;
	while (p->qtd.next != (uint32_t)transfer) {
		Transfer_t *next = (Transfer_t *)p->qtd.next;
		p->prev_followup = prev;
		p->next_followup = next;
		prev = p;
		p = next;
	}
	p->prev_followup = prev;
	p->next_followup = NULL;
	print(halt, p);
	// add them to a followup list
	if (pipe->type == 0 || pipe->type == 2) {
		// control or bulk
		add_to_async_followup_list(halt, p);
	} else {
		// interrupt
		add_to_periodic_followup_list(halt, p);
	}
	// old halt becomes new transfer, this commits all new qTDs to QH
	halt->qtd.token = token;
	return true;
}

bool followup_Transfer(Transfer_t *transfer)
{
	Serial.print("  Followup ");
	Serial.println((uint32_t)transfer, HEX);

	if (!(transfer->qtd.token & 0x80)) {
		// TODO: check error status
		if (transfer->qtd.token & 0x8000) {
			// this transfer caused an interrupt
			if (transfer->pipe->callback_function) {
				// do the callback
				(*(transfer->pipe->callback_function))(transfer);
			}
		}
		// do callback function...
		Serial.println("    completed");
		return true;
	}
	return false;
}

static void add_to_async_followup_list(Transfer_t *first, Transfer_t *last)
{
	last->next_followup = NULL; // always add to end of list
	if (async_followup_last == NULL) {
		first->prev_followup = NULL;
		async_followup_first = first;
	} else {
		first->prev_followup = async_followup_last;
		async_followup_last->next_followup = first;
	}
	async_followup_last = last;
}

static void remove_from_async_followup_list(Transfer_t *transfer)
{
	Transfer_t *next = transfer->next_followup;
	Transfer_t *prev = transfer->prev_followup;
	if (prev) {
		prev->next_followup = next;
	} else {
		async_followup_first = next;
	}
	if (next) {
		next->prev_followup = prev;
	} else {
		async_followup_last = prev;
	}
}

static void add_to_periodic_followup_list(Transfer_t *first, Transfer_t *last)
{
	last->next_followup = NULL; // always add to end of list
	if (periodic_followup_last == NULL) {
		first->prev_followup = NULL;
		periodic_followup_first = first;
	} else {
		first->prev_followup = periodic_followup_last;
		periodic_followup_last->next_followup = first;
	}
	periodic_followup_last = last;
}

static void remove_from_periodic_followup_list(Transfer_t *transfer)
{
	Transfer_t *next = transfer->next_followup;
	Transfer_t *prev = transfer->prev_followup;
	if (prev) {
		prev->next_followup = next;
	} else {
		periodic_followup_first = next;
	}
	if (next) {
		next->prev_followup = prev;
	} else {
		periodic_followup_last = prev;
	}
}

void print(const Transfer_t *transfer)
{
	if (!((uint32_t)transfer & 0xFFFFFFE0)) return;
	Serial.print("Transfer @ ");
	Serial.println(((uint32_t)transfer & 0xFFFFFFE0), HEX);
	Serial.print("   next:  ");
	Serial.println(transfer->qtd.next, HEX);
	Serial.print("   anext: ");
	Serial.println(transfer->qtd.alt_next, HEX);
	Serial.print("   token: ");
	Serial.println(transfer->qtd.token, HEX);
	Serial.print("   bufs:  ");
	for (int i=0; i < 5; i++) {
		Serial.print(transfer->qtd.buffer[i], HEX);
		if (i < 4) Serial.print(',');
	}
	Serial.println();
}

void print(const Transfer_t *first, const Transfer_t *last)
{
	Serial.print("Transfer Followup List ");
	Serial.print((uint32_t)first, HEX);
	Serial.print(" to ");
	Serial.println((uint32_t)last, HEX);
	Serial.println("    forward:");
	while (first) {
		Serial.print("    ");
		Serial.print((uint32_t)first, HEX);
		print_token(first->qtd.token);
		first = first->next_followup;
	}
	Serial.println("    backward:");
	while (last) {
		Serial.print("    ");
		Serial.print((uint32_t)last, HEX);
		print_token(last->qtd.token);
		last = last->prev_followup;
	}
}

void print_token(uint32_t token)
{
	switch ((token >> 8) & 3) {
	case 0:
		Serial.print(" OUT ");
		Serial.println((token >> 16) & 0x7FFF);
		break;
	case 1:
		Serial.print(" IN ");
		Serial.println((token >> 16) & 0x7FFF);
		break;
	case 2:
		Serial.println(" SETUP");
		break;
	default:
		Serial.println(" unknown");
	}
}

void print(const Pipe_t *pipe)
{
	if (!((uint32_t)pipe & 0xFFFFFFE0)) return;
	Serial.print("Pipe ");
	if (pipe->type == 0) Serial.print("control");
	else if (pipe->type == 1) Serial.print("isochronous");
	else if (pipe->type == 2) Serial.print("bulk");
	else if (pipe->type == 3) Serial.print("interrupt");
	Serial.print(pipe->direction ? " IN" : " OUT");
	Serial.print("  @ ");
	Serial.println((uint32_t)pipe, HEX);
	Serial.print("  horiz link:  ");
	Serial.println(pipe->qh.horizontal_link, HEX);
	Serial.print("  capabilities: ");
	Serial.print(pipe->qh.capabilities[0], HEX);
	Serial.print(',');
	Serial.println(pipe->qh.capabilities[1], HEX);
	Serial.println("  overlay:");
	Serial.print("    cur:   ");
	Serial.println(pipe->qh.current, HEX);
	Serial.print("    next:  ");
	Serial.println(pipe->qh.next, HEX);
	Serial.print("    anext: ");
	Serial.println(pipe->qh.alt_next, HEX);
	Serial.print("    token: ");
	Serial.println(pipe->qh.token, HEX);
	Serial.print("    bufs:  ");
	for (int i=0; i < 5; i++) {
		Serial.print(pipe->qh.buffer[i], HEX);
		if (i < 4) Serial.print(',');
	}
	Serial.println();
	const Transfer_t *t = (Transfer_t *)pipe->qh.next;
	while (((uint32_t)t & 0xFFFFFFE0)) {
		print(t);
		t = (Transfer_t *)t->qtd.next;
	}
	//Serial.print();
}


void print_hexbytes(const void *ptr, uint32_t len)
{
	if (ptr == NULL || len == 0) return;
	const uint8_t *p = (const uint8_t *)ptr;
	do {
		if (*p < 16) Serial.print('0');
		Serial.print(*p++, HEX);
		Serial.print(' ');
	} while (--len);
	Serial.println();
}

void print(const char *s)
{
	Serial.println(s);
	delay(10);
}

void print(const char *s, int num)
{
	Serial.print(s);
	Serial.println(num);
	delay(10);
}



// Memory allocation

static Device_t memory_Device[3];
static Pipe_t memory_Pipe[6] __attribute__ ((aligned(64)));
static Transfer_t memory_Transfer[24] __attribute__ ((aligned(64)));

Device_t * free_Device_list = NULL;
Pipe_t * free_Pipe_list = NULL;
Transfer_t * free_Transfer_list = NULL;

void init_Device_Pipe_Transfer_memory(void)
{
	Device_t *end_device = memory_Device + sizeof(memory_Device)/sizeof(Device_t);
	for (Device_t *device = memory_Device; device < end_device; device++) {
		free_Device(device);
	}
	Pipe_t *end_pipe = memory_Pipe + sizeof(memory_Pipe)/sizeof(Pipe_t);
	for (Pipe_t *pipe = memory_Pipe; pipe < end_pipe; pipe++) {
		free_Pipe(pipe);
	}
	Transfer_t *end_transfer = memory_Transfer + sizeof(memory_Transfer)/sizeof(Transfer_t);
	for (Transfer_t *transfer = memory_Transfer; transfer < end_transfer; transfer++) {
		free_Transfer(transfer);
	}
}

Device_t * allocate_Device(void)
{
	Device_t *device = free_Device_list;
	if (device) free_Device_list = *(Device_t **)device;
	return device;
}

void free_Device(Device_t *device)
{
	*(Device_t **)device = free_Device_list;
	free_Device_list = device;
}

Pipe_t * allocate_Pipe(void)
{
	Pipe_t *pipe = free_Pipe_list;
	if (pipe) free_Pipe_list = *(Pipe_t **)pipe;
	return pipe;
}

void free_Pipe(Pipe_t *pipe)
{
	*(Pipe_t **)pipe = free_Pipe_list;
	free_Pipe_list = pipe;
}

Transfer_t * allocate_Transfer(void)
{
	Transfer_t *transfer = free_Transfer_list;
	if (transfer) free_Transfer_list = *(Transfer_t **)transfer;
	return transfer;
}

void free_Transfer(Transfer_t *transfer)
{
	*(Transfer_t **)transfer = free_Transfer_list;
	free_Transfer_list = transfer;
}

