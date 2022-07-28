#include "audio.h"
#include "projector.h"
#include "serialdebug.h"
#include "pins.h"
#include "ui.h"
#include <PID_v1.h>

// Add patches.053 to flash?
#ifdef INC_PATCHES
  #include <incbin.h>
  INCBIN(Patch, "patches.053");
#endif

extern UI ui;
extern Projector projector;

// PID
volatile unsigned long totalImpCounter = 0;
double Setpoint, Input, Output;
PID myPID(&Input, &Output, &Setpoint, 8, 3, 1, 0, 0);

// Constructor
Audio::Audio(int8_t rst, int8_t cs, int8_t dcs, int8_t dreq, int8_t cardCS, uint8_t SDCD)
    : Adafruit_VS1053_FilePlayer{ rst, cs, dcs, dreq, cardCS }
    , _SDCD { SDCD }, _SDCS { (uint8_t) cardCS } {
  pinMode(_SDCD, INPUT_PULLUP);
}

uint8_t Audio::begin() {
  PRINTLN("Initializing SD card ...");
  if (!SD.begin(_SDCS))                           // initialize SD library and card
    return 1;

  PRINTLN("Initializing VS1053B ...");
  if (!Adafruit_VS1053_FilePlayer::begin())       // initialize base class
    return 2;

  if (!loadPatch())                               // load & apply patch
    return 3;

  useInterrupt(VS1053_FILEPLAYER_PIN_INT);        // use VS1053 DREQ interrupt

  // decrease priority of DREQ interrupt
  #if defined(__MKL26Z64__)                       // Teensy LC  [MKL26Z64]

  #elif defined(__MK20DX256__)                    // Teensy 3.2 [MK20DX256]
    NVIC_SET_PRIORITY(IRQ_PORTC, 144);
  #elif defined(ARDUINO_TEENSY40)                 // Teensy 4.0 [IMXRT1052]

  #endif

  return 0;                                       // return false (no error)
}

bool Audio::SDinserted() {
  return digitalReadFast(_SDCD);
}

void Audio::leaderISR() {
  digitalWriteFast(LED_BUILTIN, digitalReadFast(STARTMARK));
}

void Audio::countISR() {
  noInterrupts();
  static unsigned long lastMicros = 0;
  unsigned long thisMicros = micros();
  if ((micros()-lastMicros) > 1000) {             // poor man's debounce
    totalImpCounter++;
    lastMicros = thisMicros;
    digitalToggleFast(LED_BUILTIN);               // toggle the LED
  }
  interrupts();
}

bool Audio::selectTrack() {

  // 1. Indicate state of start mark detector via LED
  attachInterrupt(STARTMARK, leaderISR, CHANGE);
  leaderISR();

  // 2. Pick a file and run some checks
  _trackNum = selectTrackScreen();                // pick a track number
  if (_trackNum==0)
    return true;                                  // back to main-menu
  if (!loadTrack())                               // try to load track
    return ui.showError("File not found.");
  if (!connected())                               // check if audio cable is plugged in
    return ui.showError("Please connect audio device.");

  // 3. Cue file and activate resampler
  ui.drawBusyBee(90, 10);
  PRINTF("Loading \"%s\"\n", _filename);
  setVolume(254,254);                             // mute
  startPlayingFile(_filename);                    // start playback
  while (getSamplingRate()==8000) {}              // wait for correct data
  _fsPhysical = getSamplingRate();                // get physical sampling rate
  pausePlaying(true);                             // and pause again
  PRINTF("Sampling rate: %d Hz\n",_fsPhysical);
  enableResampler();
  delay(200);                                     // wait for pause
  setVolume(4,4);                                 // raise volume back up for playback
  clearSampleCounter();

  // 4. Prepare PID
  EEPROMstruct pConf = projector.config();
  myPID.SetMode(AUTOMATIC);
  myPID.SetTunings(pConf.p, pConf.i, pConf.d);
  switch (_fsPhysical) {
    case 22050: myPID.SetOutputLimits(-400000, 600000); break;  // 17% - 227%
    case 32000: myPID.SetOutputLimits(-400000, 285000); break;  // 17% - 159%
    case 44100: myPID.SetOutputLimits(-400000,  77000); break;  // 17% - 116%
    case 48000: myPID.SetOutputLimits(-400000,  29000); break;  // 17% - 107%
    default:    myPID.SetOutputLimits(-400000,  77000);         // lets assume 44.2 kHz here
  }

  // 5. Run state machine
  #define CHECK_FOR_LEADER        0
  #define OFFER_MANUAL_START      1
  #define WAIT_FOR_STARTMARK      2
  #define START                   3
  #define QUIT                  200


  PRINTLN("Waiting for start mark ...");

  uint8_t state = CHECK_FOR_LEADER;
  while (state != QUIT) {

    switch (state) {
    case CHECK_FOR_LEADER:
      if (digitalReadFast(STARTMARK))
        state = WAIT_FOR_STARTMARK;
      else
        state = OFFER_MANUAL_START;
      break;

    case OFFER_MANUAL_START:
      break;

    case WAIT_FOR_STARTMARK:
      if (digitalReadFast(STARTMARK))
        continue; // there is still leader
      PRINTLN("Hit start mark!");
      detachInterrupt(STARTMARK);
      attachInterrupt(IMPULSE, countISR, RISING); // start impulse counter
      totalImpCounter = 0;
      state = START;
      break;

    case START:
      if (totalImpCounter < pConf.startmarkOffset)
        continue;
      PRINTLN("Start!");
      state = QUIT;
      break;
    }
  }
  detachInterrupt(IMPULSE);


  // projector.config().shutterBladeCount * projector.config().startmarkOffset
  clearSampleCounter();
  //sampleCountBaseLine = read32BitsFromSCI(0x1800);


  // pauseDetectedPeriod = (1000 / fps * 3);
  // sampleCountRegisterValid = true;           // It takes 8 KiB until the Ogg Sample Counter is valid for sure
  // impToSamplerateFactor = physicalSamplingrate / fps / shutterBlades / 2;
  // deltaToFramesDivider = physicalSamplingrate / fps;
  // impToAudioSecondsDivider = sollfps * shutterBlades * 2;



  return true;
}

void Audio::speedControlPID() {
//   static unsigned long prevTotalImpCounter;
//   static unsigned long previousActualSampleCount;
//   if (totalImpCounter + syncOffsetImps != prevTotalImpCounter) {

//     if (sampleCountRegisterValid) {
//       long actualSampleCount = Read32BitsFromSCI(0x1800) - sampleCountBaseLine;                 // 8.6ms Latenz here
//       previousActualSampleCount = actualSampleCount;
//       if (actualSampleCount < previousActualSampleCount) {
//         actualSampleCount = previousActualSampleCount;
//       }
//       long desiredSampleCount = (totalImpCounter + syncOffsetImps) * impToSamplerateFactor;
//       long delta = (actualSampleCount - desiredSampleCount);

// /*
// //   This puts nifty CSV to the Console, to graph PID results.
// //      Serial.print(F("Current Sample:\t"));
//       Serial.print(actualSampleCount);
//       Serial.print(F(","));
//       Serial.print(desiredSampleCount);
//       Serial.print(F(","));
// //      Serial.print(F(","));
//       Serial.println(delta);
// //      Serial.print(F(","));
// //      Serial.print(F(" Bitrate: "));
// //      Serial.println(getBitrate());
// // */

//       total = total - readings[readIndex];  // subtract the last reading
//       readings[readIndex] = delta;          // read from the sensor:
//       total = total + readings[readIndex];  // add the reading to the total:
//       readIndex = readIndex + 1;            // advance to the next position in the array:

//       if (readIndex >= numReadings) {       // if we're at the end of the array...
//         readIndex = 0;                      // ...wrap around to the beginning:
//       }

//       average = total / numReadings;        // calculate the average
// //    average = (total >> 4);

//       Input = average;
//       adjustSamplerate((long) Output);
// //    Serial.println((long) Output);

//       prevTotalImpCounter = totalImpCounter + syncOffsetImps;

//       myPID.Compute();  // 9.2ms Latenz here

//       static unsigned int prevFrameOffset;
//       int frameOffset = average / deltaToFramesDivider;
//       if (frameOffset != prevFrameOffset) {
//         tellFrontend(CMD_OOSYNC, frameOffset);
//         prevFrameOffset = frameOffset;
//       }
//       // Below is a hack to send a 0 every so often if everything is in sync – since occasionally signal gets lost on i2c due
//       // to too busy AVRs. Otherwise, the sync icon might not stop blinking in some cases.
//       if ((frameOffset == 0) && (millis() > (lastInSyncMillis + 500))) {
//         tellFrontend(CMD_OOSYNC, 0);
//         lastInSyncMillis = millis();
//       }
//     }
//     if (musicPlayer.isPlaying() == 0) { // and/or musicPlayer.getState() == 4
//       tellFrontend(CMD_DONE_PLAYING, 0);
//       resetAudio();
//     }
//   }
}

uint16_t Audio::getSamplingRate() {
  return read16BitsFromSCI(SCI_AUDATA) & 0xfffe;  // Mask the Mono/Stereo Bit
}

bool Audio::loadTrack() {
  strcpy(_filename,"000-00.ogg");
  ui.insertPaddedInt(&_filename[0],_trackNum,3);
  for (_fps=12; _fps<=25; _fps++) {               // guess fps
    ui.insertPaddedInt(&_filename[4], _fps, 2);
    if (SD.exists(_filename))                     // file found!
      return true;
  }
  return false;                                   // file not found
}

uint16_t Audio::selectTrackScreen() {
  static uint16_t trackNum = 1;
  bool first = true;
  enc.setValue((trackNum > 0) ? trackNum : 1);
  enc.setLimits(0,999);
  while (enc.getButton()) {
    yield();
    if (enc.valueChanged() || first) {
      first   = false;
      trackNum = enc.getValue();
      buzzer.playClick();
      u8g2->clearBuffer();
      if (trackNum == 0) {
        u8g2->setFont(FONT10);
        u8g2->drawStr(18,35,"< Main Menu");
      } else {
        u8g2->setFont(u8g2_font_inb46_mn);
        u8g2->setCursor(9, 55);
        if (trackNum <  10) u8g2->print("0");
        if (trackNum < 100) u8g2->print("0");
        u8g2->print(trackNum);
      }
      u8g2->sendBuffer();
    }
  }
  enc.setLimits(-999,999);
  enc.setValue(0);
  u8g2->setFont(FONT10);
  ui.waitForBttnRelease();
  return trackNum;
}



bool Audio::connected() {
  pinMode(TSH, OUTPUT);
  pinMode(RSH, INPUT);
  digitalWrite(TSH, HIGH);
  bool out = !digitalRead(RSH);
  digitalWrite(TSH, LOW);
  pinMode(TSH, INPUT_DISABLE);
  pinMode(RSH, INPUT_DISABLE);
  return out;
}

bool Audio::loadPatch() {
  if (SD.exists("/patches.053")) {
    // If patches.053 is found on SD we'll always try to load it from there ...
    File file = SD.open("/patches.053", O_READ);
    uint16_t addr, n, val, i = 0;
    bool status = false;
    PRINTF("Applying \"patches.053\" (%d bytes) from SD card ... ", file.size());
    while (file.read(&addr, 2) && file.read(&n, 2)) {
      i += 2;
      if (n & 0x8000U) {
        n &= 0x7FFF;
        status = file.read(&val, 2);
        if (!status)
          break;
        while (n--)
          sciWrite(addr, val);
      } else {
        while (n--) {
          status = file.read(&val, 2);
          if (!status)
            break;
          i++;
          sciWrite(addr, val);
        }
      }
    }
    file.close();
    if (status) {
      PRINTLN("done");
      return status;
    } else
      PRINTLN("error reading file.");
  }

  // ... if not, we'll load it from flash memory
  #ifdef INC_PATCHES
    PRINTF("Applying \"patches.053\" (%d bytes) from flash ... ", gPatchSize);
    applyPatch(reinterpret_cast<const uint16_t *>(gPatchData), gPatchSize / 2);
    PRINTF("done\n\n");
    return true;
  #endif
  return false;
}

void Audio::enableResampler() {
  sciWrite(SCI_WRAMADDR, 0x1e09);
  sciWrite(SCI_WRAM, 0x0080);
  PRINTLN("15/16 resampling enabled.");
}

void Audio::adjustSamplerate(signed long ppm2) {
  sciWrite(SCI_WRAMADDR, 0x1e07);
  sciWrite(SCI_WRAM, ppm2);
  sciWrite(SCI_WRAM, ppm2 >> 16);
  sciWrite(SCI_WRAMADDR, 0x5b1c);
  sciWrite(SCI_WRAM, 0);
  sciWrite(SCI_AUDATA, sciRead(SCI_AUDATA));
}

void Audio::clearSampleCounter() {
  sciWrite(SCI_WRAMADDR, 0x1800);
  sciWrite(SCI_WRAM, 0);
  sciWrite(SCI_WRAM, 0);
}

void Audio::clearErrorCounter() {
  sciWrite(SCI_WRAMADDR, 0x5a82);
  sciWrite(SCI_WRAM, 0);
}

unsigned int Audio::read16BitsFromSCI(unsigned short addr) {
  return (unsigned int)sciRead(addr);
}

unsigned long Audio::read32BitsFromSCI(unsigned short addr) {
  unsigned short msbV1, lsb, msbV2;
  sciWrite(SCI_WRAMADDR, addr+1);
  msbV1 = (unsigned int)sciRead(SCI_WRAM);
  sciWrite(SCI_WRAMADDR, addr);
  lsb   = (unsigned long)sciRead(SCI_WRAM);
  msbV2 = (unsigned int)sciRead(SCI_WRAM);
  if (lsb < 0x8000U) {
    msbV1 = msbV2;
  }
  return ((unsigned long)msbV1 << 16) | lsb;
}

void Audio::restoreSampleCounter(unsigned long samplecounter) {
  sciWrite(SCI_WRAMADDR, 0x1800); // MSB
  sciWrite(SCI_WRAM, samplecounter);
  sciWrite(SCI_WRAM, samplecounter >> 16);
}

// uint16_t Audio::getBitrate() {
//   return (Mp3ReadWRAM(PARA_BYTERATE)>>7);
// }

const char Audio::getRevision() {
  char rev = 0;
  for (uint8_t i = 0; i<4; i++) {
    if (GPIO_digitalRead(i))
      bitSet(rev, i);
  }
  return rev + 65;
}
