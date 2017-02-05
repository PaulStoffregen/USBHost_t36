// usb host experiments....

#include "host.h"

uint32_t periodictable[32] __attribute__ ((aligned(4096), used));
uint8_t port_state;
#define PORT_STATE_DISCONNECTED   0
#define PORT_STATE_DEBOUNCE       1
#define PORT_STATE_RESET          2
#define PORT_STATE_RECOVERY       3
#define PORT_STATE_ACTIVE         4
Device_t *rootdev=NULL;

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

	MPU_RGDAAC0 |= 0x30000000;
	Serial.print("MPU_RGDAAC0 = ");
	Serial.println(MPU_RGDAAC0, HEX);
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

	print("begin ehci reset");
	USBHS_USBCMD |= USBHS_USBCMD_RST;
	count = 0;
	while (USBHS_USBCMD & USBHS_USBCMD_RST) {
		count++;
	}
	print(" reset waited ", count);

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

	NVIC_ENABLE_IRQ(IRQ_USBHS);
	USBHS_USBINTR = USBHS_USBINTR_UE | USBHS_USBINTR_PCE | USBHS_USBINTR_TIE0;
	USBHS_USBINTR |= USBHS_USBINTR_UEE | USBHS_USBINTR_SEE;
	USBHS_USBINTR |= USBHS_USBINTR_AAE;

	delay(25);
	Serial.println("Plug in device...");
	digitalWrite(32, HIGH); // connect device


	delay(5000);
	Serial.println();
	Serial.println("Ring Doorbell");
	USBHS_USBCMD |= USBHS_USBCMD_IAA;
	if (rootdev) print(rootdev->control_pipe);
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
				// TODO: should ENHOSTDISCONDETECT be set? K66 ref, page 1701
			} else {
				Serial.println("    disconnect");
				port_state = PORT_STATE_DISCONNECTED;
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
	dev->speed = speed;
	dev->address = 0;
	dev->hub_address = hub_addr;
	dev->hub_port = hub_port;
	dev->control_pipe = new_Pipe(dev, 0, 0, 0, 8);
	if (!dev->control_pipe) {
		free_Device(dev);
		return NULL;
	}

	static uint8_t buffer[8];
	dev->control_pipe->direction = 1; // 1=IN
	dev->setup.bmRequestType = 0x80;
	dev->setup.bRequest = 0x06; // 6=GET_DESCRIPTOR
	dev->setup.wValue = 0x0100;
	dev->setup.wIndex = 0x0000;
	dev->setup.wLength = 8;
	Transfer_t *transfer = new_Transfer(dev->control_pipe, buffer, 8);
	//print(dev->control_pipe);
	if (transfer) queue_Transfer(transfer);

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

Pipe_t * new_Pipe(Device_t *dev, uint32_t type, uint32_t endpoint, uint32_t direction,
	uint32_t max_packet_len)
{
	Pipe_t *pipe;
	uint32_t c=0, dtc=0;

	Serial.println("new_Pipe");
	pipe = allocate_Pipe();
	if (!pipe) return NULL;
	pipe->device = dev;
	pipe->qh.current = 0;
	pipe->qh.next = 1;
	pipe->qh.alt_next = 1;
	pipe->qh.token = 0;
	pipe->qh.buffer[0] = 0;
	pipe->qh.buffer[1] = 0;
	pipe->qh.buffer[2] = 0;
	pipe->qh.buffer[3] = 0;
	pipe->qh.buffer[4] = 0;
	pipe->active = 0;
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
#if 0
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
		pipe->active = 1;
	} else if (type == 3) {
		// interrupt: add to periodic schedule
		// TODO: link it into the periodic table
	}
#endif
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

// Create a list of Transfers
//
Transfer_t * new_Transfer(Pipe_t *pipe, void *buffer, uint32_t len)
{
	Serial.println("new_Transfer");
	Transfer_t *transfer = allocate_Transfer();
	if (!transfer) return NULL;
	transfer->pipe = pipe;
	if (pipe->type == 0) {
		// control transfer
		Transfer_t *data, *status;
		uint32_t status_direction;
		if (len > 16384) {
			free_Transfer(transfer);
			return NULL;
		}
		status = allocate_Transfer();
		if (!status) {
			free_Transfer(transfer);
			return NULL;
		}
		if (len > 0) {
			data = allocate_Transfer();
			if (!data) {
				free_Transfer(transfer);
				free_Transfer(status);
				return NULL;
			}
			init_qTD(data, buffer, len, pipe->direction, 1, false);
			transfer->qtd.next = (uint32_t)data;
			data->qtd.next = (uint32_t)status;
			data->pipe = pipe;
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
		status->qtd.next = 1;
	} else {
		// bulk, interrupt or isochronous transfer
		free_Transfer(transfer);
		return NULL;
	}
	return transfer;
}


void queue_Transfer(Transfer_t *transfer)
{
	Serial.println("queue_Transfer");
	Pipe_t *pipe = transfer->pipe;

	if (!pipe->active) {
		if (pipe->type == 0 || pipe->type == 2) {
			// control or bulk: add to async queue
			pipe->qh.next = (uint32_t)transfer;
			Pipe_t *list = (Pipe_t *)USBHS_ASYNCLISTADDR;
			if (list == NULL) {
				Serial.println("  first in async list, with qTD");
				pipe->qh.capabilities[0] |= 0x8000; // H bit
				pipe->qh.horizontal_link = (uint32_t)(&(pipe->qh)) | 2; // 2=QH
				USBHS_ASYNCLISTADDR = (uint32_t)pipe;
				print(pipe);
				Serial.println(USBHS_USBSTS & USBHS_USBSTS_AS, HEX);
				Serial.println(USBHS_USBCMD & USBHS_USBCMD_ASE, HEX);
				//USBHS_USBCMD |= USBHS_USBCMD_IAA;
				USBHS_USBCMD |= USBHS_USBCMD_ASE; // enable async schedule
				uint32_t count=0;
				while (!(USBHS_USBSTS & USBHS_USBSTS_AS)) count++;
				Serial.print("    waited ");
				Serial.println(count);
				Serial.println(USBHS_USBCMD & USBHS_USBCMD_ASE, HEX);
				Serial.println(USBHS_USBSTS & USBHS_USBSTS_AS, HEX);
			} else {
				// EHCI 1.0: section 4.8.1, page 72
				pipe->qh.horizontal_link = list->qh.horizontal_link;
				list->qh.horizontal_link = (uint32_t)&(pipe->qh) | 2;
				Serial.println("  added to async list, with qTD");
			}
			pipe->active = 1;
		} else if (pipe->type == 3) {
			// interrupt: add to periodic schedule
			// TODO: link it into the periodic table
		}
	} else {
		Transfer_t *last = (Transfer_t *)(pipe->qh.next);
		if ((uint32_t)last & 1) {
			pipe->qh.next = (uint32_t)transfer;
			Serial.println("  first on QH");
		} else {
			while ((last->qtd.next & 1) == 0) last = (Transfer_t *)(last->qtd.next);
			last->qtd.next = (uint32_t)transfer;
			Serial.println("  added to qTD list");
		}
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


// Memory allocation... for now, just simplest leaky way to get started

Device_t * allocate_Device(void)
{
	static Device_t mem[3];
	static size_t count=0;
	if (count >= sizeof(mem)/sizeof(Device_t)) return NULL;
	return &mem[count++];
}

void free_Device(Device_t *q)
{
}

Pipe_t * allocate_Pipe(void)
{
	static Pipe_t mem[6] __attribute__ ((aligned(64)));
	static size_t count=0;
	if (count >= sizeof(mem)/sizeof(Pipe_t)) return NULL;
	return &mem[count++];
}

void free_Pipe(Pipe_t *q)
{
}

Transfer_t * allocate_Transfer(void)
{
	static Transfer_t mem[22] __attribute__ ((aligned(64)));
	static size_t count=0;
	if (count >= sizeof(mem)/sizeof(Transfer_t)) return NULL;
	return &mem[count++];
}

void free_Transfer(Transfer_t *q)
{
}

