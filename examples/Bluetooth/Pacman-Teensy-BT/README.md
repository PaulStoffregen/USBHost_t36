# Pacman-Arduino-Due / Modified for Teensy 3.6 and T4.x
Pacman Game for Arduino Due with tft ILI9341 and VGA output support

Pacman Game on Arduino Due with ILI9341 and VGA support (available 2 outputs at the same time), playable with keypad, 5 sample levels, VGA output is 240x320, source code avaliable on Github, Licensed under MIT License.

Original Pacman sketch modified to use a Teensy 3.6/T4 with ILI9341_t3n and USBHost_t36 libraries.  Intended as demo for the incorporating BT or Serial gamepads such as the PS3 and PS4 into a user application.

Originally posted on:
Arduino Forum:

http://forum.arduino.cc/index.php?topic=375394.0

Video:

https://www.youtube.com/watch?v=2Hdzr6m4QdU



<pre>
/******************************************************************************/
/*                                                                            */
/*  PACMAN GAME FOR TEENSY                                                    */
/*                                                                            */
/******************************************************************************/
/*  Copyright (c) 2014  Dr. NCX (mirracle.mxx@gmail.com)                      */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL              */
/* WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED              */
/* WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR    */
/* BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES      */
/* OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,     */
/* WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,     */
/* ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS        */
/* SOFTWARE.                                                                  */
/*                                                                            */
/*  MIT license, all text above must be included in any redistribution.       */
/******************************************************************************/
/*  Original Pacman sketch modified to use a Teensy 3.6/T4 with ILI9341_t3n   */
/*  and USBHost_t36 libraries.  Intended as demo for the incorporating BT     */
/*  or Serial gamepads such as the PS3 and PS4 into a user application.       */
/*  The original version for the Arduino Due and SNES controllers can be      */
/*  found at                                                                  */
/*  https://forum.arduino.cc/index.php?topic=375394.0                         */
/*  and                                                                       */
/*  https://github.com/DrNCXCortex/Pacman-Arduino-Due                         */
/*                                                                            */
/******************************************************************************/
/*  ILI9341:                                                                  */
/*----------------------------------------------------------------------------*/
/*   8 = RST                                                                  */
/*   9 = D/C                                                                  */
/*  10 = CS                                                                   */
/*  can be changed at user discretion                                         */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/*  KEYPAD:                                                                   */
/*----------------------------------------------------------------------------*/
/*  PS3: button START/Pause                                                   */
/*  	 button SELECT                                                        */
/*  	 button A/Diamond                                                     */
/*  	 button B /Circle                                                     */
/*  	 button UP                                                            */
/*  	 button DOWN                                                          */
/*  	 button LEFT                                                          */
/*  	 button RIGHT                                                         */
/*                                                                            */
/*  PS4: button START/Pause = Share on PS4                                    */
/*  	 button SELECT = Options on PS4                                       */
/*  	 button A/Square                                                      */
/*  	 button B /Circle                                                     */
/*  	 button UP                                                            */
/*  	 button DOWN                                                          */
/*  	 button LEFT                                                          */
/*  	 button RIGHT                                                         */
/*                                                                            *//******************************************************************************/
</pre>
