/*
ILI9341_due_config.h - Arduino Due library for interfacing with ILI9341-based TFTs

Code: https://github.com/marekburiak/ILI9341_due
Documentation: http://marekburiak.github.io/ILI9341_due/

Copyright (c) 2015  Marek Buriak

*/

#ifndef _ILI9341_due_configH_
#define _ILI9341_due_configH_

// comment out the SPI mode you want to use (does not matter for AVR)
#define ILI9341_SPI_MODE_NORMAL	// uses SPI library
//#define ILI9341_SPI_MODE_EXTENDED	// uses Extended SPI in Due, make sure you use pin 4, 10 or 52 for CS
//#define ILI9341_SPI_MODE_DMA		// uses DMA in Due

// set the clock divider
#if defined ARDUINO_SAM_DUE
#define ILI9341_SPI_CLKDIVIDER 4	// for Due
#elif defined ARDUINO_ARCH_AVR
#define ILI9341_SPI_CLKDIVIDER SPI_CLOCK_DIV2	// for Uno, Mega,...
#elif defined(TEENSYDUINO)
#define SPICLOCK 30000000
#endif



// uncomment if you want to use SPI transactions. Uncomment it if the library does not work when used with other libraries.
#define ILI_USE_SPI_TRANSACTION

// comment out if you do need to use scaled text. The text will draw then faster.
#define TEXT_SCALING_ENABLED

// default letter spacing
#define DEFAULT_LETTER_SPACING 2

// default line spacing
#define DEFAULT_LINE_SPACING 0

// sets the space between lines as part of the text
//#define LINE_SPACING_AS_PART_OF_LETTERS

// number representing the maximum angle (e.g. if 100, then if you pass in start=0 and end=50, you get a half circle)
// this can be changed with setArcParams function at runtime
#define DEFAULT_ARC_ANGLE_MAX 360		

// rotational offset in degrees defining position of value 0 (-90 will put it at the top of circle)
// this can be changed with setAngleOffset function at runtime
#define DEFAULT_ANGLE_OFFSET -90	




#endif
