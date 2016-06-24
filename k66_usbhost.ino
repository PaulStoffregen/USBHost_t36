// usb host experiments....

void setup()
{
	while (!Serial) ; // wait
	print("USB Host Testing");
	MCG_C1 |= MCG_C1_IRCLKEN;  // enable MCGIRCLK 32kHz
	OSC0_CR |= OSC_ERCLKEN;
	SIM_SOPT2 |= SIM_SOPT2_USBREGEN; // turn on USB regulator
	SIM_SOPT2 &= ~SIM_SOPT2_USBSLSRC; // use IRC for slow clock
	print("power up USBHS PHY");
	SIM_USBPHYCTL |= SIM_USBPHYCTL_USBDISILIM; // disable USB current limit
	SIM_USBPHYCTL = SIM_USBPHYCTL_USBDISILIM | SIM_USBPHYCTL_USB3VOUTTRG(6); // pg 237
	SIM_SCGC3 |= SIM_SCGC3_USBHSDCD | SIM_SCGC3_USBHSPHY | SIM_SCGC3_USBHS;
	print("init USBHS PHY & PLL");
	// init process: page 1681-1682
	USBPHY_CTRL &= ~USBPHY_CTRL_SFTRST;	// CTRL pg 1698
	USBPHY_CTRL &= ~USBPHY_CTRL_CLKGATE;
	USBPHY_PLL_SIC |= USBPHY_PLL_SIC_PLL_POWER;
	USBPHY_PLL_SIC |= USBPHY_PLL_SIC_PLL_DIV_SEL(1);  // PLL_SIC pg 1708
	// wait for the PLL to lock
	int count=0;
	while ((USBPHY_PLL_SIC & USBPHY_PLL_SIC_PLL_LOCK) == 0) {
		count++;
	}
	print("PLL locked, waited ", count);

	// sanity check, connect 470K pullup & 100K pulldown and watch D+ voltage change
	//USBPHY_ANACTRL_CLR = (1<<10); // turn off both 15K pulldowns... works! :)

	SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(3); // LPO 1kHz
	SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(2); // Flash
	SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(6); // XTAL
	SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(7); // IRC 48MHz
	SIM_SOPT2 = SIM_SOPT2 & (~SIM_SOPT2_CLKOUTSEL(7)) | SIM_SOPT2_CLKOUTSEL(4); // MCGIRCLK
	CORE_PIN9_CONFIG = PORT_PCR_MUX(5);  // CLKOUT on PTC3 Alt5 (Arduino pin 9)



}


void loop()
{
	// do nothing (yet)
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

