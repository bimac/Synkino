#include "audio.h"
#include "projector.h"
#include "serialdebug.h"
#include "pins.h"
#include "ui.h"

#include "TeensyTimerTool.h"
using namespace TeensyTimerTool;
#include <QuickPID.h>

#include <EncoderTool.h>
using namespace EncoderTool;
extern PolledEncoder enc;

// Add patches.053 to flash?
#ifdef INC_PATCHES
  #include <incbin.h>
  INCBIN(Patch, "patches.053");
#endif

// macros for time conversion
#define SECS_PER_MIN            (60UL)
#define SECS_PER_HOUR           (3600UL)
#define SECS_PER_DAY            (SECS_PER_HOUR * 24L)
#define numberOfSeconds(_time_) ( _time_ % SECS_PER_MIN)
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN )
#define numberOfHours(_time_)   ((_time_ % SECS_PER_DAY) / SECS_PER_HOUR)

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
#define WAIT_FOR_OFFSET          3
#define START                    4
#define PLAYING                  5
#define PAUSE                    6
#define PAUSED                   7
#define RESUME                   8
#define SHUTDOWN               254
#define QUIT                   255

extern UI ui;
extern Projector projector;

bool runPID = false;
PeriodicTimer pidTimer(TCK);
volatile uint32_t totalImpCounter = 0;

// Constructor
Audio::Audio() : Adafruit_VS1053_FilePlayer{VS1053_RST, VS1053_CS, VS1053_DCS, VS1053_DREQ, VS1053_SDCS} {
  pinMode(VS1053_SDCD, INPUT_PULLUP);
}

uint8_t Audio::begin() {
  PRINTLN("Initializing SD card ...");
  if (!SD.begin(VS1053_SDCS))                     // initialize SD library and card
    return 1;

  PRINTLN("Initializing VS1053B ...");
  if (!Adafruit_VS1053_FilePlayer::begin())       // initialize base class
    return 2;
  useInterrupt(VS1053_FILEPLAYER_PIN_INT);        // use VS1053 DREQ interrupt

  if (!loadPatch())                               // load & apply patch
    return 3;
  enableResampler();                              // enable 15/16 resampler



  // the DREQ interrupt seems to mess with the timing of the impulse detection
  // which is also using an interrupt. This can be remidied by lowering the
  // priority of the DREQ interrupt
  #if defined(__MKL26Z64__)                       // Teensy LC  [MKL26Z64]
    NVIC_SET_PRIORITY(IRQ_PORTCD, 192);
    // This could be a problem. Interrupt priorities for ports C and D cannot be
    // changed independently from one another as they share an IRQ number. Thus,
    // we currently can't prioritize IMPULSE over DREQ on Teensy LC:
    //
    //    Pin 02 = IMPULSE   -> Port D0 / IRQ 31
    //    Pin 03 = STARTMARK -> Port A1 / IRQ 30
    //    Pin 10 = DREQ      -> Port C4 / IRQ 31
    //
    // Possible solution: swap IMPULSE and STARTMARK?
    // Should also be OK for Teensy 3.2 - see below.
    //
    // For reference, see schematics at https://www.pjrc.com/teensy/schematic.html

  #elif defined(__MK20DX256__)                    // Teensy 3.2 [MK20DX256]
    NVIC_SET_PRIORITY(IRQ_PORTC, 144);
    //    Pin 02 = IMPULSE   -> Port D0  / IRQ 90
    //    Pin 03 = STARTMARK -> Port A12 / IRQ 87
    //    Pin 10 = DREQ      -> Port C4  / IRQ 89

  #elif defined(ARDUINO_TEENSY40)                 // Teensy 4.0 [IMXRT1052]
    // Might not work either.
    // See https://forum.pjrc.com/threads/59828-Teensy-4-Set-interrupt-priority-on-given-pins
  #endif

  return 0;                                       // return false (no error)
}

bool Audio::SDinserted() {
  return digitalReadFast(VS1053_SDCD);
}

void Audio::leaderISR() {
  digitalWriteFast(LED_BUILTIN, digitalReadFast(STARTMARK));
}

void Audio::countISR() {
  noInterrupts();
  static unsigned long lastMicros = 0;
  unsigned long thisMicros = micros();

  if ((thisMicros - lastMicros) > 2000) {         // poor man's debounce - no periods below 2ms (500Hz)
    totalImpCounter++;
    lastMicros = thisMicros;
    digitalToggleFast(LED_BUILTIN);               // toggle the LED
  }
  interrupts();
}

bool Audio::selectTrack() {
  EEPROMstruct pConf = projector.config();        // get projector configuration
  uint8_t state = CHECK_FOR_LEADER;
  bool showOffsetCorrectionInput = false;

  // 1. Indicate presence of film leader using LED
  leaderISR();
  attachInterrupt(STARTMARK, leaderISR, CHANGE);

  // 2. Pick a file and run a few checks
  _trackNum = selectTrackScreen();                            // pick a track number
  if (_trackNum==0)
    return true;                                              // back to main-menu
  if (!loadTrack())                                           // try to load track
    return ui.showError("File not found.");                   // back to track selection
  if (!connected())                                           // check if audio is plugged in
    return ui.showError("Please connect audio device.");
  if (!digitalReadFast(STARTMARK))                            // check for leader
    state = OFFER_MANUAL_START;

  // 3. Busy bee is working hard ...
  u8g2->clearBuffer();
  u8g2->setFont(FONT10);
  u8g2->drawStr(8,50,"Loading...");
  PeriodicTimer beeTimer(TCK);
  beeTimer.begin([]() { ui.drawBusyBee(90, 10); }, 30_Hz);

  // 4. Cue file
  PRINT("Loading \"");
  PRINT(_filename);
  PRINTLN("\"");
  setVolume(254,254);                 // mute
  clearSampleCounter();
  startPlayingFile(_filename);        // start playback
  while (getSamplingRate()==8000) {}  // wait for correct data
  _fsPhysical = getSamplingRate();    // get physical sampling rate
  pausePlaying(true);                 // and pause again
  PRINT("Sampling rate: ");
  PRINT(_fsPhysical);
  PRINTLN(" Hz");
  delay(500);                         // wait for pause
  setVolume(4,4);                     // raise volume back up for playback

  // 5. Define some conversion factors
  impToSamplerateFactor    = _fsPhysical / _fps / pConf.shutterBladeCount;
  deltaToFramesDivider     = _fsPhysical / _fps;
  impToAudioSecondsDivider = _fps * pConf.shutterBladeCount;

  // 6. Reset variables
  _frameOffset             = 0;
  sampleCountBaseLine      = 0;
  lastSampleCounterHaltPos = 0;
  syncOffsetImps           = 0;
  Setpoint                 = 0;
  Input                    = 0;
  Output                   = 0;
  while (average(0) != 0) {}

  // 7. Prepare PID
  myPID.SetMode(myPID.Control::timer);
  myPID.SetProportionalMode(myPID.pMode::pOnMeas);
  myPID.SetDerivativeMode(myPID.dMode::dOnMeas);
  myPID.SetAntiWindupMode(myPID.iAwMode::iAwClamp);
  myPID.SetTunings(pConf.p, pConf.i, pConf.d);
  switch (_fsPhysical) {
    case 22050: myPID.SetOutputLimits(-400000, 600000); break;  // 17% - 227%
    case 32000: myPID.SetOutputLimits(-400000, 285000); break;  // 17% - 159%
    case 44100: myPID.SetOutputLimits(-400000,  77000); break;  // 17% - 116%
    case 48000: myPID.SetOutputLimits(-400000,  29000); break;  // 17% - 107%
    default:    myPID.SetOutputLimits(-400000,  77000);         // lets assume 44.2 kHz here
  }

  // 8. Run state machine
  beeTimer.stop();
  while (state != QUIT) {
    yield();

    switch (state) {
    case CHECK_FOR_LEADER:
      if (digitalReadFast(STARTMARK)) {
        PRINT("Waiting for start mark ... ");
        drawWaitForPlayingMenu();
        state = WAIT_FOR_STARTMARK;
      } else
        state = OFFER_MANUAL_START;
      break;

    case OFFER_MANUAL_START:
      if (ui.userInterfaceMessage("Can't detect film leader.",
                                  "Trigger manual start?", "",
                                  " Cancel \n OK ") == 2) {
        state = START;
        detachInterrupt(STARTMARK);
        attachInterrupt(IMPULSE, countISR, RISING); // start impulse counter
      } else
        state = SHUTDOWN; // back to main-menu
      break;

    case WAIT_FOR_STARTMARK:
      if (digitalReadFast(STARTMARK))
        continue; // there is still leader
      PRINTLN("Hit!");
      PRINT("Waiting for ");
      PRINT(pConf.startmarkOffset);
      PRINTLN(" frames ...");
      detachInterrupt(STARTMARK);
      digitalWriteFast(LED_BUILTIN, LOW);
      totalImpCounter = 0;
      attachInterrupt(IMPULSE, countISR, RISING); // start impulse counter
      state = WAIT_FOR_OFFSET;
      break;

    case WAIT_FOR_OFFSET:
      if ((totalImpCounter/pConf.shutterBladeCount) < pConf.startmarkOffset)
        continue;
      state = START;
      break;

    case START:
      totalImpCounter = 0;
      pausePlaying(false);
      PRINTLN("Starting playback.");
      sampleCountBaseLine = getSampleCount();
      pidTimer.begin([]() { runPID = true; }, 10_Hz);
      buzzer.play(1000,42); // play 2-pop ;-)
      enc.setValue(0);
      enc.buttonChanged();
      state = PLAYING;
      break;

    case PLAYING:
      if (runPID) {
        speedControlPID();
        runPID = false;
        state  = handlePause();
      }

      if (enc.buttonChanged() && enc.getButton()) {
        showOffsetCorrectionInput = !showOffsetCorrectionInput;
        if (showOffsetCorrectionInput)
          enc.setValue(syncOffsetImps / pConf.shutterBladeCount);
        else {
          PRINT("Sync-offset set to ");
          PRINT(syncOffsetImps / pConf.shutterBladeCount);
          PRINTLN(" frames.");
        }
      }

      if (showOffsetCorrectionInput)
        handleFrameCorrectionOffsetInput();
      else
        drawPlayingMenu();

      if (stopped())
        state = SHUTDOWN;
      break;

    case PAUSE:
      lastSampleCounterHaltPos = getSampleCount();
      pausePlaying(true);
      PRINT("Pausing playback at sample ");
      PRINTLN(lastSampleCounterHaltPos);
      myPID.SetMode(myPID.Control::manual);
      pidTimer.stop();
      state = PAUSED;
      break;

    case PAUSED:
      drawPlayingMenu();
      state = handlePause();
      break;

    case RESUME:
      myPID.SetMode(myPID.Control::timer);
      pidTimer.start();
      restoreSampleCounter(lastSampleCounterHaltPos);
      pausePlaying(false);
      PRINTLN("Resuming playback.");
      state = PLAYING;
      break;

    case SHUTDOWN:
      stopPlaying();
      myPID.SetMode(myPID.Control::manual);
      pidTimer.stop();
      PRINTLN("Stopped playback.");
      detachInterrupt(IMPULSE);
      state = QUIT;
    }
  }
  u8g2->setFont(FONT10);
  return true;
}

uint8_t Audio::handlePause() {
  static uint16_t pauseDetectedPeriod = (1000 / _fps * 3);
  static uint32_t prevTotalImpCounter = 0;
  static uint32_t lastImpMillis;

  bool impsChanged = (totalImpCounter + syncOffsetImps) != prevTotalImpCounter;

  if (paused()) {
    if (impsChanged)
      return RESUME;
    else
      return PAUSED;
  } else {
    if (impsChanged && !paused()) {
      prevTotalImpCounter = totalImpCounter + syncOffsetImps;
      lastImpMillis = millis();
    } else if ((millis() - lastImpMillis) >= pauseDetectedPeriod) {
      return PAUSE;
    }
    return PLAYING;
  }
}

void Audio::speedControlPID() {
  uint32_t actualSampleCount = getSampleCount() - sampleCountBaseLine;
  int32_t desiredSampleCount = (totalImpCounter + syncOffsetImps) * impToSamplerateFactor;
  long delta = (actualSampleCount - desiredSampleCount);

  Input = average(delta);
  myPID.Compute();
  adjustSamplerate(Output);

  _frameOffset = Input / deltaToFramesDivider;

  //This puts nifty CSV to the Console, to graph PID results.
  //PRINTF("Delta:%d,Average:%f,FrameOffset: %d\n", delta, Input, _frameOffset);
  //PRINTF("Input:%0.3f,Output:%0.3f\n", Input, Output);
  //PRINTF("P:%0.5f,I:%0.5f,D:%0.5f\n", myPID.GetPterm(), myPID.GetIterm(), myPID.GetDterm());
}

void Audio::drawPlayingMenuConstants() {
  u8g2->setFont(FONT08);
  u8g2->drawStr(0, 8, projector.config().name);
  char buffer[9] = "Film 000";
  ui.insertPaddedInt(&buffer[5], _trackNum, 10, 3);
  ui.drawRightAlignedStr(8, buffer);
  itoa(_fps, buffer, 10);
  strcat(buffer, " fps");
  ui.drawRightAlignedStr(62, buffer);
  u8g2->setFont(FONT10);
}

void Audio::drawPlayingMenuStatus() {
  if (paused())
    u8g2->drawXBMP(60, 54, pause_xbm_width, pause_xbm_height, pause_xbm_bits);
  else
    u8g2->drawXBMP(60, 54, play_xbm_width, play_xbm_height, play_xbm_bits);
}

void Audio::drawWaitForPlayingMenu() {
  u8g2->clearBuffer();
  drawPlayingMenuConstants();
  ui.drawCenteredStr(28, "Waiting for");
  ui.drawCenteredStr(46, "Film to Start");
  drawPlayingMenuStatus();
  u8g2->sendBuffer();
}

void Audio::drawPlayingMenu() {
  // limit display refresh-rate to 25 Hz
  uint32_t currentMillis = millis();
  static uint32_t prevMillis = -1;
  if ((currentMillis-prevMillis) < 40)
    return;
  prevMillis = currentMillis;

  // clear screen buffer & draw constants
  u8g2->clearBuffer();
  drawPlayingMenuConstants();

  // draw time-code
  uint32_t audioSecs = (totalImpCounter + syncOffsetImps) / impToAudioSecondsDivider;
  uint8_t hh = numberOfHours(audioSecs);
  uint8_t mm = numberOfMinutes(audioSecs);
  uint8_t ss = numberOfSeconds(audioSecs);
  u8g2->setFont(u8g2_font_inb24_mn);
  u8g2->drawStr(20,36,":");
  u8g2->drawStr(71,36,":");
  u8g2->setCursor(4,40);
  u8g2->print(hh);
  u8g2->setCursor(35,40);
  if (mm < 10) u8g2->print("0");
  u8g2->print(mm);
  u8g2->setCursor(85,40);
  if (ss < 10) u8g2->print("0");
  u8g2->print(ss);

  drawPlayingMenuStatus();

  // draw sync status
  if (_frameOffset == 0)
    u8g2->drawXBMP(2, 54, sync_xbm_width, sync_xbm_height, sync_xbm_bits);
  else {
    if (currentMillis % 700 > 350)
      u8g2->drawXBMP(2, 54, sync_xbm_width, sync_xbm_height, sync_xbm_bits);
    u8g2->setFont(FONT08);
    u8g2->setCursor(24,62);
    if (_frameOffset > 0)
      u8g2->print("+");
    u8g2->print(_frameOffset);
  }
  u8g2->sendBuffer();
}


uint8_t Audio::drawTrackLoadedMenu(uint8_t preselect) {
  const char *trackLoaded_menu =
    "Manual Start\n"
    "Stop\n"
    "Exit";
  uint8_t selection = u8g2->userInterfaceSelectionList("Playback", preselect, trackLoaded_menu);
  return selection;
}


void Audio::handleFrameCorrectionOffsetInput() {
  // limit display refresh-rate to 25 Hz
  uint32_t currentMillis = millis();
  static uint32_t prevMillis = -1;
  if ((currentMillis-prevMillis) < 40)
    return;
  prevMillis = currentMillis;

  EEPROMstruct pConf = projector.config();
  int32_t newSyncOffset = enc.getValue();
  syncOffsetImps = newSyncOffset * pConf.shutterBladeCount;

  // clear screen buffer
  u8g2->clearBuffer();

  // Draw Header & Footer
  u8g2->setFont(FONT10);
  if (newSyncOffset == 0) {
    u8g2->drawStr(6,12,"Adjust Sync Offset");
  } else if (newSyncOffset < 0) {
    u8g2->drawStr(11,12,"Delay Sound by");
  } else if (newSyncOffset > 0) {
    u8g2->drawStr(6,12,"Advance Sound by");
  }
  u8g2->drawStr(40,64,"Frames");

  // Draw number of offset frames
  u8g2->setFont(u8g2_font_inb24_mn);
  if (newSyncOffset >= 0 && newSyncOffset <= 9)
    u8g2->setCursor(55, 46);
  else if (newSyncOffset >= -9 && newSyncOffset <= -1)
    u8g2->setCursor(45, 46);
  else if (newSyncOffset >= 10 && newSyncOffset <= 99)
    u8g2->setCursor(45, 46);
  else if (newSyncOffset >= -99 && newSyncOffset <= -10)
    u8g2->setCursor(35, 46);
  else if (newSyncOffset >= 100 && newSyncOffset <= 999)
    u8g2->setCursor(35, 46);
  else if (newSyncOffset >= -999 && newSyncOffset <= -100)
    u8g2->setCursor(25, 46);
  u8g2->print(newSyncOffset);

  u8g2->sendBuffer();
  u8g2->setFont(FONT10);
}

int32_t Audio::average(int32_t input) {
  #define numReadings 6                       // window size for sliding average
  static int32_t readings[numReadings] = {0};
  static int32_t total = 0;
  static uint8_t readIdx = 0;

  readIdx = (readIdx + 1) % numReadings;      // update array index
  total = total - readings[readIdx];          // subtract previous reading from running total
  readings[readIdx] = input;                  // store new reading to array
  total += input;                             // add new reading to running total
  return total / numReadings;                 // calculate & return the average
}

uint16_t Audio::getSamplingRate() {
  return sciRead(SCI_AUDATA) & 0xfffe;    // Mask the Mono/Stereo Bit
}

bool Audio::loadTrack() {
  strcpy(_filename,"000-00.ogg");
  ui.insertPaddedInt(&_filename[0], _trackNum, 10, 3);
  for (_fps=12; _fps<=25; _fps++) {                     // guess fps
    ui.insertPaddedInt(&_filename[4], _fps, 10, 2);
    if (SD.exists(_filename))                           // file found!
      return true;
  }
  return false;                                         // file not found
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
    PRINT("Applying \"patches.053\" from SD card ... ");
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
      PRINTLN("done.");
      return status;
    } else
      PRINTLN("error reading file.");
  }

  // ... if not, we'll load it from flash memory
  #ifdef INC_PATCHES
    PRINT("Applying \"patches.053\" from flash ... ");
    applyPatch(reinterpret_cast<const uint16_t *>(gPatchData), gPatchSize / 2);
    PRINTLN("done.");
    return true;
  #endif
  return false;
}

void Audio::enableResampler() {
  // See section 1.6 of
  // https://www.vlsi.fi/fileadmin/software/VS10XX/vs1053b-patches.pdf
  sciWrite(SCI_WRAMADDR, 0x1e09);
  sciWrite(SCI_WRAM, 0x0080);
  PRINTLN("15/16 Resampler enabled.");
}

void Audio::adjustSamplerate(signed long ppm2) {
  sciWrite(SCI_WRAMADDR, 0x1e07);
  sciWrite(SCI_WRAM, ppm2);
  sciWrite(SCI_WRAM, ppm2 >> 16);
  sciWrite(SCI_WRAMADDR, 0x5b1c);
  sciWrite(SCI_WRAM, 0);
  sciWrite(SCI_AUDATA, sciRead(SCI_AUDATA));
}

uint32_t Audio::getSampleCount() {
  // See section 1.3 of
  // https://www.vlsi.fi/fileadmin/software/VS10XX/vs1053b-patches.pdf
  return sciRead32(0x1800);
}

void Audio::clearSampleCounter() {
  sciWrite(SCI_WRAMADDR, 0x1800);
  sciWrite(SCI_WRAM, 0);
  sciWrite(SCI_WRAM, 0);
}

void Audio::restoreSampleCounter(uint32_t samplecounter) {
  sciWrite(SCI_WRAMADDR, 0x1800); // MSB
  sciWrite(SCI_WRAM, samplecounter);
  sciWrite(SCI_WRAM, samplecounter >> 16);
}

void Audio::clearErrorCounter() {
  sciWrite(SCI_WRAMADDR, 0x5a82);
  sciWrite(SCI_WRAM, 0);
}

uint32_t Audio::sciRead32(uint16_t addr) {
  // See section 1.3 of
  // https://www.vlsi.fi/fileadmin/software/VS10XX/vs1053b-patches.pdf
  u_int16_t msbV1, lsb, msbV2;
  sciWrite(SCI_WRAMADDR, addr + 1);
  msbV1 = (u_int16_t)sciRead(SCI_WRAM);
  sciWrite(SCI_WRAMADDR, addr);
  lsb = (u_int32_t)sciRead(SCI_WRAM);
  msbV2 = (u_int16_t)sciRead(SCI_WRAM);
  if (lsb < 0x8000U) {
    msbV1 = msbV2;
  }
  return ((u_int32_t)msbV1 << 16) | lsb;
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
  // not quite sure what to make of this ...
  unsigned short int tmp1, tmp2;

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
