// usb host experiments....


uint32_t periodictable[64] __attribute__ ((aligned(4096), used));
volatile uint32_t qh[12] __attribute__ ((aligned(64)));
uint32_t qtd_dummy[8] __attribute__ ((aligned(32)));
uint32_t qtd_setup[8] __attribute__ ((aligned(32)));
uint32_t qtd_in[8] __attribute__ ((aligned(32)));
uint32_t qtd_outack[8] __attribute__ ((aligned(32)));
uint32_t setupbuf[2] __attribute__ ((aligned(8)));
uint32_t inbuf[16] __attribute__ ((aligned(64)));


void setup()
{
	// Test board has a USB data mux (this won't be on final Teensy 3.6)
	pinMode(32, OUTPUT);	// pin 32 = USB switch, high=connect device
	digitalWrite(32, LOW);
	// Teensy 3.6 has USB host power controlled by PTE6
	PORTE_PCR6 = PORT_PCR_MUX(1);
	GPIOE_PDDR |= (1<<6);
	GPIOE_PSOR = (1<<6); // turn on USB host power
	while (!Serial) ; // wait
	print("USB Host Testing");
	print_mpu();
	MPU_RGDAAC0 |= 0x30000000;
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
	print("PLL locked, waited ", count);
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

// EHCI registers         page  default
// --------------         ----  -------
// USBHS_USBCMD           1599  00080000
// USBHS_USBSTS           1602  00000000
// USBHS_USBINTR          1606  00000000
// USBHS_FRINDEX          1609  00000000
// USBHS_PERIODICLISTBASE 1610  undefine
// USBHS_ASYNCLISTADDR    1612  undefine
// USBHS_PORTSC           1619  00002000
// USBHS_USBMODE          1629  00005000

	print("begin ehci reset");
	USBHS_USBCMD |= USBHS_USBCMD_RST;
	count = 0;
	while (USBHS_USBCMD & USBHS_USBCMD_RST) {
		count++;
	}
	print(" reset waited ", count);

	for (int i=0; i < 64; i++) {
		periodictable[i] = 1;
	}
	qh[0] = ((uint32_t)qh) | 2;
	qh[1] = 0x0040E000; // addr=0, ep=0
	qh[2] = 0x40000000;
	qh[3] = 0;
	qh[4] = 1;
	qh[5] = 1;
	qh[6] = 0x40;
	qh[7] = 0;
	qh[8] = 0;
	qh[9] = 0;
	qh[10] = 0;
	qh[11] = 0;
	qtd_dummy[0] = 1;
	qtd_dummy[1] = 1;
	qtd_dummy[2] = 0x40; // 0x40 = halted
	qtd_dummy[3] = 0;
	qtd_dummy[4] = 0;
	qtd_dummy[5] = 0;
	qtd_dummy[6] = 0;
	qtd_dummy[7] = 0;

	// turn on the USBHS controller
	USBHS_USBMODE = USBHS_USBMODE_TXHSD(5) | USBHS_USBMODE_CM(3); // host mode
	USBHS_USBINTR = 0;
	USBHS_PERIODICLISTBASE = (uint32_t)periodictable;
	USBHS_FRINDEX = 0;
	USBHS_ASYNCLISTADDR = (uint32_t)qh;
	USBHS_USBCMD = USBHS_USBCMD_ITC(0) | USBHS_USBCMD_RS | USBHS_USBCMD_ASP(3) |
		USBHS_USBCMD_FS2 | USBHS_USBCMD_FS(0) | // periodic table is 64 pointers
		// USBHS_USBCMD_PSE |
		USBHS_USBCMD_ASE;
	USBHS_PORTSC1 |= USBHS_PORTSC_PP;
	//USBHS_PORTSC1 |= USBHS_PORTSC_PFSC; // force 12 Mbit/sec
	//USBHS_PORTSC1 |= USBHS_PORTSC_PHCD; // phy off

	Serial.print("USBHS_ASYNCLISTADDR = ");
	Serial.println(USBHS_ASYNCLISTADDR, HEX);
	Serial.print("USBHS_PERIODICLISTBASE = ");
	Serial.println(USBHS_PERIODICLISTBASE, HEX);
	Serial.print("periodictable = ");
	Serial.println((uint32_t)periodictable, HEX);
}

void port_status()
{
	uint32_t n;

	Serial.print("Port: ");
	n = USBHS_PORTSC1;
	if (n & USBHS_PORTSC_PR) {
		Serial.print("reset ");
	}
	if (n & USBHS_PORTSC_PP) {
		Serial.print("on ");
	} else {
		Serial.print("off ");
	}
	if (n & USBHS_PORTSC_PHCD) {
		Serial.print("phyoff ");
	}
	if (n & USBHS_PORTSC_PE) {
		if (n & USBHS_PORTSC_SUSP) {
			Serial.print("suspend ");
		} else {
			Serial.print("enable ");
		}
	} else {
		Serial.print("disable ");
	}
	Serial.print("speed=");
	switch ((n >> 26) & 3) {
	  case 0: Serial.print("12 Mbps "); break;
	  case 1: Serial.print("1.5 Mbps "); break;
	  case 2: Serial.print("480 Mbps "); break;
	  default: Serial.print("(undef) ");
	}
	if (n & USBHS_PORTSC_HSP) {
		Serial.print("highspeed ");
	}
	if (n & USBHS_PORTSC_OCA) {
		Serial.print("overcurrent ");
	}
	if (n & USBHS_PORTSC_CCS) {
		Serial.print("connected ");
	} else {
		Serial.print("not-connected ");
	}
	// print info about the EHCI status
	Serial.print(" run=");
	Serial.print(USBHS_USBCMD & 1);         // running mode
	Serial.print(",halt=");
	Serial.print((USBHS_USBSTS >> 12) & 1); // halted mode
	Serial.print(",err=");
	Serial.print((USBHS_USBSTS >> 4) & 1);  // error encountered!
	Serial.print(",asyn=");
	Serial.print((USBHS_USBSTS >> 15) & 1); // running the async schedule
	Serial.print(",per=");
	Serial.print((USBHS_USBSTS >> 14) & 1); // running the periodic schedule
	Serial.print(",index=");
	Serial.print(USBHS_FRINDEX);            // periodic index

	Serial.println();
	if (USBHS_USBSTS & 16) {
		print_mpu();
		USBHS_USBSTS = 16; // clear error
	}
}


void read_descriptor(uint16_t value, uint16_t index, uint32_t len)
{
	uint32_t token;

	if (len > 512) len = 512;
	Serial.println("Read Device Descriptor...");

	qtd_setup[0] = (uint32_t)qtd_in;
	qtd_setup[1] = 1;
	qtd_setup[2] = 0x00080E80;
	qtd_setup[3] = (uint32_t)setupbuf;

	setupbuf[0] = (value << 16) | (0x06 << 8) | 0x80;
	setupbuf[1] = (len << 16) | index;

	qtd_in[0] = (uint32_t)qtd_outack;
	qtd_in[1] = 1;
	qtd_in[2] = 0x80000000 | (len << 16) | 0x0D80;
	qtd_in[3] = (uint32_t)inbuf;

	qtd_outack[0] = 1;
	qtd_outack[1] = 1;
	qtd_outack[2] = 0x80400C80;
	qtd_outack[3] = 0;

	// add to QH

	// Save the content of the token field of the first qTD to be added
	token = qtd_setup[2];

	// Change the token of the first qTD so its Halted bit is set as 1
	// and all other bits are zero
	qtd_setup[2] = 0x40;

	// copy the content of the first qTD to the dummy qTD
	memcpy(qtd_dummy, qtd_setup, 32);

	// Link the first qTD to the last of the qTD of the newly qTD list
	qtd_outack[0] = (uint32_t)qtd_setup;

	// Restore the token value to the previous dummy qTD's oken field
	qtd_dummy[2] = token;
	// qtd_setup becomes the dummy token... so this only works once!

	delay(1);
	Serial.println(qtd_dummy[2], HEX);
	Serial.println(qtd_in[2], HEX);
	Serial.println(qtd_outack[2], HEX);
	Serial.println(qtd_setup[2], HEX);
}


void loop()
{
	static unsigned int count=0;

	port_status();
	delay(1);
	count++;
	if (count == 2) {
		Serial.println("Plug in device...");
		digitalWrite(32, HIGH); // connect device
	}
	if (count == 5) {
		Serial.println("Initiate Reset Sequence...");
		USBHS_PORTSC1 |= USBHS_PORTSC_PR;
	}
	if (count == 15) {
		Serial.println("End Reset Sequence...");
		USBHS_PORTSC1 &= ~USBHS_PORTSC_PR;
	}
	if (count == 22) {
		read_descriptor(1, 0, 8); // device descriptor
	}
	if (count > 5000) {
		while (1) ; // stop here
	}
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

void print_mpu()
{
	Serial.print("MPU: CESR=");
	Serial.println(MPU_CESR, HEX);
}
