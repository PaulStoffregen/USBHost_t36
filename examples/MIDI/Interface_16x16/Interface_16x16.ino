/* Create a "class compliant " USB to 6 MIDI IN and 6 MIDI OUT interface,
   plus 10 more USB connected devices.  Admittedly, you could just plug
   those 10 devices directly into your computer, but this example is meant
   to show how to forward any MIDI message between the 3 different MIDI
   libraries.  A "real" application might do something more interesting,
   like translate or modify the MIDI messages....

   MIDI receive (6N138 optocoupler) input circuit and series resistor
   outputs need to be connected to Serial1-Serial6.  A USB host cable
   is needed on Teensy 3.6's second USB port, and obviously USB hubs
   are needed to connect up to 10 USB MIDI devices.  That's a *LOT* of
   extra hardware to connect to a Teensy!

   You must select MIDIx16 from the "Tools > USB Type" menu

   This example code is in the public domain.
*/

#include <MIDI.h>        // access to serial (5 pin DIN) MIDI
#include <USBHost_t36.h> // access to USB MIDI devices (plugged into 2nd USB port)

// Create the Serial MIDI ports
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI1);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI2);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial3, MIDI3);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial4, MIDI4);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial5, MIDI5);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial6, MIDI6);
//midi::MidiInterface &SerialMidiList[6] = {MIDI1, MIDI2, MIDI3, MIDI4, MIDI5, MIDI6};

// Create the ports for USB devices plugged into Teensy's 2nd USB port (via hubs)
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);
MIDIDevice midi01(myusb);
MIDIDevice midi02(myusb);
MIDIDevice midi03(myusb);
MIDIDevice midi04(myusb);
MIDIDevice midi05(myusb);
MIDIDevice midi06(myusb);
MIDIDevice midi07(myusb);
MIDIDevice midi08(myusb);
MIDIDevice midi09(myusb);
MIDIDevice midi10(myusb);
MIDIDevice * midilist[10] = {
  &midi01, &midi02, &midi03, &midi04, &midi05, &midi06, &midi07, &midi08, &midi09, &midi10
};

// A variable to know how long the LED has been turned on
elapsedMillis ledOnMillis;


void setup() {
  Serial.begin(115200);
  pinMode(13, OUTPUT); // LED pin
  digitalWrite(13, LOW);
  MIDI1.begin(MIDI_CHANNEL_OMNI);
  MIDI2.begin(MIDI_CHANNEL_OMNI);
  MIDI3.begin(MIDI_CHANNEL_OMNI);
  MIDI4.begin(MIDI_CHANNEL_OMNI);
  MIDI5.begin(MIDI_CHANNEL_OMNI);
  MIDI6.begin(MIDI_CHANNEL_OMNI);
  // Wait 1.5 seconds before turning on USB Host.  If connected USB devices
  // use too much power, Teensy at least completes USB enumeration, which
  // makes isolating the power issue easier.
  delay(1500);
  Serial.println("Interface_16x16 Example");
  delay(10);
  myusb.begin();
}


void loop() {
  bool activity = false;

  // First read messages from the 6 Serial MIDI IN ports
  if (MIDI1.read()) {
    sendToComputer(MIDI1.getType(), MIDI1.getData1(), MIDI1.getData2(), MIDI1.getChannel(), MIDI1.getSysExArray(), 0);
    activity = true;
  }
  if (MIDI2.read()) {
    sendToComputer(MIDI2.getType(), MIDI2.getData1(), MIDI2.getData2(), MIDI2.getChannel(), MIDI2.getSysExArray(), 1);
    activity = true;
  }
  if (MIDI3.read()) {
    sendToComputer(MIDI3.getType(), MIDI3.getData1(), MIDI3.getData2(), MIDI3.getChannel(), MIDI3.getSysExArray(), 2);
    activity = true;
  }
  if (MIDI4.read()) {
    sendToComputer(MIDI4.getType(), MIDI4.getData1(), MIDI4.getData2(), MIDI4.getChannel(), MIDI4.getSysExArray(), 3);
    activity = true;
  }
  if (MIDI5.read()) {
    sendToComputer(MIDI5.getType(), MIDI5.getData1(), MIDI5.getData2(), MIDI5.getChannel(), MIDI5.getSysExArray(), 4);
    activity = true;
  }
  if (MIDI6.read()) {
    sendToComputer(MIDI6.getType(), MIDI6.getData1(), MIDI6.getData2(), MIDI6.getChannel(), MIDI6.getSysExArray(), 5);
    activity = true;
  }

  // Next read messages arriving from the (up to) 10 USB devices plugged into the USB Host port
  for (int port=0; port < 10; port++) {
    if (midilist[port]->read()) {
      uint8_t type =       midilist[port]->getType();
      uint8_t data1 =      midilist[port]->getData1();
      uint8_t data2 =      midilist[port]->getData2();
      uint8_t channel =    midilist[port]->getChannel();
      const uint8_t *sys = midilist[port]->getSysExArray();
      sendToComputer(type, data1, data2, channel, sys, 6 + port);
      activity = true;
    }
  }

  // Finally, read any messages the PC sends to Teensy, and forward them
  // to either Serial MIDI or to USB devices on the USB host port.
  if (usbMIDI.read()) {
    // get the USB MIDI message, defined by these 5 numbers (except SysEX)
    byte type = usbMIDI.getType();
    byte channel = usbMIDI.getChannel();
    byte data1 = usbMIDI.getData1();
    byte data2 = usbMIDI.getData2();
    byte cable = usbMIDI.getCable();

    // forward this message to 1 of the 3 Serial MIDI OUT ports
    if (type != usbMIDI.SystemExclusive) {
      // Normal messages, first we must convert usbMIDI's type (an ordinary
      // byte) to the MIDI library's special MidiType.
      midi::MidiType mtype = (midi::MidiType)type;

      // Then simply give the data to the MIDI library send()
      switch (cable) {
        case  0: MIDI1.send(mtype, data1, data2, channel); break;
        case  1: MIDI2.send(mtype, data1, data2, channel); break;
        case  2: MIDI3.send(mtype, data1, data2, channel); break;
        case  3: MIDI4.send(mtype, data1, data2, channel); break;
        case  4: MIDI5.send(mtype, data1, data2, channel); break;
        case  5: MIDI6.send(mtype, data1, data2, channel); break;
        default: // cases 6-15
          midilist[cable - 6]->send(type, data1, data2, channel);
      }

    } else {
      // SysEx messages are special.  The message length is given in data1 & data2
      unsigned int SysExLength = data1 + data2 * 256;
      switch (cable) {
        case 0: MIDI1.sendSysEx(SysExLength, usbMIDI.getSysExArray(), true); break;
        case 1: MIDI2.sendSysEx(SysExLength, usbMIDI.getSysExArray(), true); break;
        case 2: MIDI3.sendSysEx(SysExLength, usbMIDI.getSysExArray(), true); break;
        case 3: MIDI4.sendSysEx(SysExLength, usbMIDI.getSysExArray(), true); break;
        case 4: MIDI5.sendSysEx(SysExLength, usbMIDI.getSysExArray(), true); break;
        case 5: MIDI6.sendSysEx(SysExLength, usbMIDI.getSysExArray(), true); break;
        default: // cases 6-15
          midilist[cable - 6]->sendSysEx(SysExLength, usbMIDI.getSysExArray(), true);
      }
    }
    activity = true;
  }

  // blink the LED when any activity has happened
  if (activity) {
    digitalWriteFast(13, HIGH); // LED on
    ledOnMillis = 0;
  }
  if (ledOnMillis > 15) {
    digitalWriteFast(13, LOW);  // LED off
  }

}


void sendToComputer(byte type, byte data1, byte data2, byte channel, const uint8_t *sysexarray, byte cable)
{
  if (type != midi::SystemExclusive) {
    usbMIDI.send(type, data1, data2, channel, cable);
  } else {
    unsigned int SysExLength = data1 + data2 * 256;
    usbMIDI.sendSysEx(SysExLength, sysexarray, true, cable);
  }
}
