// Simple WAV file player example
//
// Three types of output may be used, by configuring the code below.
//
//   1: Digital I2S - Normally used with the audio shield:
//         http://www.pjrc.com/store/teensy3_audio.html
//
//   2: Digital S/PDIF - Connect pin 22 to a S/PDIF transmitter
//         https://www.oshpark.com/shared_projects/KcDBKHta
//
//   3: Analog DAC - Connect the DAC pin to an amplified speaker
//         http://www.pjrc.com/teensy/gui/?info=AudioOutputAnalog
//
// To configure the output type, first uncomment one of the three
// output objects.  If not using the audio shield, comment out
// the sgtl5000_1 lines in setup(), so it does not wait forever
// trying to configure the SGTL5000 codec chip.
//
// The SD card may connect to different pins, depending on the
// hardware you are using.  Uncomment or configure the SD card
// pins to match your hardware.
//
// Data files to put on your SD card can be downloaded here:
//   http://www.pjrc.com/teensy/td_libs_AudioDataFiles.html
//
// This example code is in the public domain.

// Modified for use with USB mass storage drives 2020 Warren Watson

#include <USBHost_t36.h>
#include <Audio.h>
#include "play_usb_wav.h"  // Should be included in 'Audio.h'
#include <Wire.h>
#include <SPI.h>
//#include <SD.h>
//#include <SerialFlash.h>

// Setup USBHost_t36 and as many HUB ports as needed.
USBHost myusb;
USBHub hub1(myusb);

// Setup MSC for the number of USB Drives you are using. (Two for this example)
// Mutiple  USB drives can be used. Hot plugging is supported. There is a slight
// delay after a USB MSC device is plugged in. This is waiting for initialization
// but after it is initialized ther should be no delay.
USBDrive msDrive1(myusb);

USBFilesystem myFS(myusb);

AudioPlayUSBWav playWav1;
// Use one of these 3 output types: Digital I2S, Digital S/PDIF, or Analog DAC
AudioOutputI2S audioOutput;
//AudioOutputSPDIF       audioOutput;
//AudioOutputAnalog      audioOutput;
AudioConnection patchCord1(playWav1, 0, audioOutput, 0);
AudioConnection patchCord2(playWav1, 1, audioOutput, 1);
AudioControlSGTL5000 sgtl5000_1;

void setup() {
  // Wait for USB Serial
  while (!Serial) {
    yield();
  }
  Serial.println("WaveFilePlayerUSB Started");
  if (CrashReport) {
    Serial.print(CrashReport);
    Serial.println("Press any key to continue");
    while (Serial.read() != -1)
      ;
    while (Serial.read() == -1)
      ;
  }

  // Start USBHost_t36, HUB(s) and USB devices.
  myusb.begin();

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(8);

  // Comment these out if not using the audio adaptor board.
  // This may wait forever if the SDA & SCL pins lack
  // pullup resistors
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.50);

  Serial.println("Waiting for USB Filesystem");
  while (!myFS) {
    myusb.Task();
  }
  Serial.println("Filesystem started");
}

void playFile(File *pfile, const char *filename) {
  Serial.print("Playing file: ");
  Serial.println(filename);

  // Start playing the file.  This sketch continues to
  // run while the file plays.
  playWav1.play(pfile);

  // A brief delay for the library read WAV info
  delay(5);

  // Simply wait for the file to finish playing.
  while (playWav1.isPlaying()) {
    // uncomment these lines if you audio shield
    // has the optional volume pot soldered
    //float vol = analogRead(15);
    //vol = vol / 1024;
    //sgtl5000_1.volume(vol);
  }
}


void loop() {
  File rootFile;
  File wavFile;
  const char *name = nullptr;
  uint8_t name_len;
  bool wav_files_found = false;
  rootFile = myFS.open("/");
  for (;;) {
    wavFile = rootFile.openNextFile();
    if (!wavFile) break;
    name = wavFile.name();
    name_len = strlen(name);
    if ((strcmp(&name[name_len - 4], ".wav") == 0) || (strcmp(&name[name_len - 4], ".WAV") == 0)) {
      wav_files_found = true;
      playFile(&wavFile, name);
      delay(500);
    }
    wavFile.close();
  }
  if (!wav_files_found) {
    Serial.println("There were no *.wav files found in the root directory");
    delay(5000);
  }
  delay(1500);
  rootFile.close();
}
