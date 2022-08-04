#include "audio.h"
#include "projector.h"
#include "serialdebug.h"
#include "pins.h"
#include "ui.h"

#include "TeensyTimerTool.h"
using namespace TeensyTimerTool;
#include <QuickPID.h>

// some synonyms for backward compatibility
#define SCI_MODE            VS1053_REG_MODE
#define SCI_STATUS          VS1053_REG_STATUS
#define SCI_BASS            VS1053_REG_BASS
#define SCI_CLOCKF          VS1053_REG_CLOCKF
#define SCI_DECODE_TIME     VS1053_REG_DECODETIME
#define SCI_AUDATA          VS1053_REG_AUDATA
#define SCI_WRAM            VS1053_REG_WRAM
#define SCI_WRAMADDR        VS1053_REG_WRAMADDR
#define SCI_HDAT0           VS1053_REG_HDAT0
#define SCI_HDAT1           VS1053_REG_HDAT1

// extra parameters in X memory (section 10.11)
#define PARA_CHIPID_0       0x1E00
#define PARA_CHIPID_1       0x1E01
#define PARA_VERSION        0x1E02
#define PARA_CONFIG1        0x1E03
#define PARA_PLAYSPEED      0x1E04
#define PARA_BYTERATE       0x1E05
#define PARA_ENDFILLBYTE    0x1E06
#define PARA_MONOOUTPUT     0x1E09
#define PARA_POSITIONMSEC_0 0x1E27
#define PARA_POSITIONMSEC_1 0x1E28
#define PARA_RESYNC         0x1E29

// state labels
#define CHECK_FOR_LEADER         0
#define OFFER_MANUAL_START       1
#define WAIT_FOR_STARTMARK       2
#define START                    3
#define PLAYING                  4
#define SHUTDOWN               250
#define QUIT                   255

// Add patches.053 to flash?
#ifdef INC_PATCHES
  #include <incbin.h>
  INCBIN(Patch, "patches.053");
#endif

extern UI ui;
extern Projector projector;

bool runPID = false;
PeriodicTimer pidTimer(TCK);

// PID
volatile unsigned long totalImpCounter = 0;
int32_t syncOffsetImps = 0;
bool sampleCountRegisterValid = true;
unsigned long sampleCountBaseLine = 0;

unsigned int impToSamplerateFactor;
int deltaToFramesDivider;
unsigned int impToAudioSecondsDivider;

float Setpoint, Input, Output;
QuickPID myPID(&Input, &Output, &Setpoint, 8.0, 3.0, 1.0,
               myPID.pMode::pOnMeas,
               myPID.dMode::dOnMeas,
               myPID.iAwMode::iAwClamp,
               myPID.Action::direct);

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

  // the DREQ interrupt seems to mess with the timing of the impulse detection
  // which is also using an interrupt. This can be remidied by lowering the
  // priority of the DREQ interrupt
  #if defined(__MKL26Z64__)                       // Teensy LC  [MKL26Z64]
    NVIC_SET_PRIORITY(IRQ_PORTCD, 192);
    // This could be a problem. Interrupt priorities for ports C and D cannot be
    // changed independently from one another as they share an IRQ number. Thus,
    // we currently can't prioritize IMPULSE over DREQ:
    //
    //    Pin 10 = DREQ      -> Port C4 / IRQ 31
    //    Pin 02 = IMPULSE   -> Port D0 / IRQ 31
    //
    // Possible solution 1: swap IMPULSE and STARTMARK?
    //
    //    Pin 03 = STARTMARK -> Port A1 / IRQ 30
    //    Should also be OK for Teensy 3.2 - see below
    //
    // Possible solution 2: use timer instead of DREQ?
    //
    //    Meh.
    //
    // For reference, see schematics at https://www.pjrc.com/teensy/schematic.html

  #elif defined(__MK20DX256__)                    // Teensy 3.2 [MK20DX256]
    NVIC_SET_PRIORITY(IRQ_PORTC, 144);
    //    Pin 10 = DREQ      -> Port C4  / IRQ 89
    //    Pin 02 = IMPULSE   -> Port D0  / IRQ 90
    //    Pin 03 = STARTMARK -> Port A12 / IRQ 87

  #elif defined(ARDUINO_TEENSY40)                 // Teensy 4.0 [IMXRT1052]
    // ?
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

  if ((micros()-lastMicros) > 2000) {             // poor man's debounce - no periods below 2ms (500Hz)
    totalImpCounter++;
    lastMicros = thisMicros;
    digitalToggleFast(LED_BUILTIN);               // toggle the LED
  }
  interrupts();
}

bool Audio::selectTrack() {
  EEPROMstruct pConf = projector.config();        // get projector configuration
  uint8_t state = CHECK_FOR_LEADER;

  // 1. Indicate state of start mark detector by means of LED
  leaderISR();
  attachInterrupt(STARTMARK, leaderISR, CHANGE);

  // 2. Pick a file and run a few checks
  _trackNum = selectTrackScreen();                // pick a track number
  if (_trackNum==0)
    return true;                                  // back to main-menu
  if (!loadTrack())                               // try to load track
    return ui.showError("File not found.");       // back to track selection
  if (!connected())                               // check if audio cable is plugged in
    return ui.showError("Please connect audio device.");
  if (!digitalReadFast(STARTMARK)) {              // check for leader
    if (ui.userInterfaceMessage("Can't detect film leader.",
                                "Use manual start?", "",
                                " Cancel \n Yes ") == 2) {
      state = OFFER_MANUAL_START;
      detachInterrupt(STARTMARK);
    } else
      return true;                                // back to main-menu
  }

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
  delay(500);                                     // wait for pause
  setVolume(4,4);                                 // raise volume back up for playback
  clearSampleCounter();

  impToSamplerateFactor = _fsPhysical / _fps / pConf.shutterBladeCount;
  deltaToFramesDivider = _fsPhysical / _fps;
  impToAudioSecondsDivider = _fps * pConf.shutterBladeCount;
  // pausePlaying(false);
  // while (true) {
  //   delay(100);
  //   PRINTF("%d kbps\n", getBitrate());
  // }

  // 4. Prepare PID
  myPID.SetMode(myPID.Control::timer);
  myPID.SetTunings(pConf.p, pConf.i, pConf.d);
  switch (_fsPhysical) {
    case 22050: myPID.SetOutputLimits(-400000, 600000); break;  // 17% - 227%
    case 32000: myPID.SetOutputLimits(-400000, 285000); break;  // 17% - 159%
    case 44100: myPID.SetOutputLimits(-400000,  77000); break;  // 17% - 116%
    case 48000: myPID.SetOutputLimits(-400000,  29000); break;  // 17% - 107%
    default:    myPID.SetOutputLimits(-400000,  77000);         // lets assume 44.2 kHz here
  }

  // 5. Run state machine
  PRINTLN("Waiting for start mark ...");

  while (state != QUIT) {
    yield();

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
      totalImpCounter = 0;
      detachInterrupt(STARTMARK);
      attachInterrupt(IMPULSE, countISR, RISING); // start impulse counter
      state = START;
      break;

    case START:
      if (totalImpCounter < pConf.startmarkOffset)
        continue;
      totalImpCounter = 0;
      pausePlaying(false);
      clearSampleCounter();
      sampleCountBaseLine = read32BitsFromSCI(0x1800);
      PRINTLN("Start!");
      pidTimer.begin([]() { runPID = true; }, 10_Hz);
      state = PLAYING;
      break;

    case PLAYING:
      if (runPID) {
        speedControlPID();
        runPID = false;
      }
      break;

    case SHUTDOWN:
      stopPlaying();
      PRINTLN("Stopped Playback.");
      state = QUIT;
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
  // static unsigned long lastMillis = millis();
  // if (millis() - lastMillis < 10)
  //   return;

  // static unsigned long prevTotalImpCounter;
  static unsigned long previousActualSampleCount;
  // if (totalImpCounter + syncOffsetImps != prevTotalImpCounter) {

    //if (sampleCountRegisterValid) {
      unsigned long actualSampleCount = read32BitsFromSCI(0x1800) - sampleCountBaseLine;
      previousActualSampleCount = actualSampleCount;
      if (actualSampleCount < previousActualSampleCount)
        actualSampleCount = previousActualSampleCount;
      long desiredSampleCount = (totalImpCounter + syncOffsetImps) * impToSamplerateFactor;
      long delta = (actualSampleCount - desiredSampleCount);

      Input = (float) average(delta);
      myPID.Compute();
      adjustSamplerate((long) Output);

      // prevTotalImpCounter = totalImpCounter + syncOffsetImps;

      int frameOffset = (long) Input / deltaToFramesDivider;
      //PRINTF("Delta:%d,Average:%f,FrameOffset: %d\n", delta, Input, frameOffset);
      //PRINTF("Input:%0.3f,Output:%0.3f\n", Input, Output);
      //PRINTF("P:%0.5f,I:%0.5f,D:%0.5f\n",myPID.GetPterm(),myPID.GetIterm(),myPID.GetDterm());

    //}
    // if (musicPlayer.isPlaying() == 0) // and/or musicPlayer.getState() == 4
    //   resetAudio();
  // }
}

int32_t Audio::average(int32_t input) {
  #define numReadings 6
  static int32_t readings[numReadings] = {0};
  static int32_t total = 0;
  static uint8_t readIdx = 0;

  readIdx = (readIdx + 1) % numReadings;  // update array index
  total = total - readings[readIdx];      // subtract previous reading from running total
  readings[readIdx] = input;              // store new reading to array
  total += input;                         // add new reading to running total
  int32_t average = total / numReadings;  // calculate the average

  return average;
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
  // test if audio device is plugged into 3.5mm jack
  pinMode(PIN_TIPSW, OUTPUT);
  pinMode(PIN_LOUT,  INPUT);
  digitalWrite(PIN_TIPSW, HIGH);
  bool out = !digitalRead(PIN_LOUT);
  digitalWrite(PIN_TIPSW, LOW);
  pinMode(PIN_TIPSW, INPUT_DISABLE);
  pinMode(PIN_LOUT,  INPUT_DISABLE);
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

uint16_t Audio::getBitrate() {
  return (readWRAM(PARA_BYTERATE)>>7);
}

const char Audio::getRevision() {
  char rev = 0;
  for (uint8_t i = 0; i<4; i++) {
    if (GPIO_digitalRead(i))
      bitSet(rev, i);
  }
  return rev + 65;
}

uint16_t Audio::readWRAM(uint16_t addressbyte) {
  // adapted from https://github.com/mpflaga/Arduino_Library-vs1053_for_SdFat

   unsigned short int tmp1,tmp2;

   //Set SPI bus for write
   //spiInit();
   //SPI.setClockDivider(spi_Read_Rate);

   sciWrite(VS1053_REG_WRAMADDR, addressbyte);
   tmp1 = sciRead(SCI_WRAM);

   sciWrite(VS1053_REG_WRAMADDR, addressbyte);
   tmp2 = sciRead(SCI_WRAM);

   if(tmp1==tmp2) return tmp1;
   sciWrite(VS1053_REG_WRAMADDR, addressbyte);
   tmp2 = sciRead(SCI_WRAM);

   if(tmp1==tmp2) return tmp1;
   sciWrite(VS1053_REG_WRAMADDR, addressbyte);
   tmp2 = sciRead(SCI_WRAM);

   if(tmp1==tmp2) return tmp1;
   return tmp1;
 }
