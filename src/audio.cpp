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

// some extra registers in X memory (section 10.11)
#define VS1053_XMEM_POSITIONMSEC_0 0x1E27
#define VS1053_XMEM_POSITIONMSEC_1 0x1E28
#define VS1053_XMEM_SAMPLECOUNT    0x1800

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
  if (!loadPatch())                               // load & apply patch
    return 3;
  useInterrupt(VS1053_FILEPLAYER_PIN_INT);        // use VS1053 DREQ interrupt

  // the DREQ interrupt seems to mess with the timing of the impulse detection
  // which is also using an interrupt. This can be remedied by lowering the
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
  static unsigned long lastMicros = 0;
  unsigned long thisMicros = micros();

  if ((thisMicros - lastMicros) > 2000) {         // poor man's debounce - no periods below 2ms (500Hz)
    noInterrupts();
    totalImpCounter++;
    lastMicros = thisMicros;
    interrupts();
    digitalToggleFast(LED_BUILTIN);               // toggle the LED
  }
}

bool Audio::selectTrack() {
  EEPROMstruct pConf = projector.config();        // get projector configuration
  uint8_t state = CHECK_FOR_LEADER;
  bool showOffsetCorrectionInput = false;

  // 1. Indicate presence of film leader using LED
  leaderISR();
  attachInterrupt(STARTMARK, leaderISR, CHANGE);

  // 2. Pick a file and run a few checks
  if (_trackNum != 999) {
    _trackNum = selectTrackScreen();                          // pick a track number
    if (_trackNum == 0 )
      return true;                                            // back to main-menu
  }
  if (!loadTrack(_trackNum))                                  // try to load track
    return ui.showError("File not found.");                   // back to track selection
  if (!connected()) {                                         // check if audio is plugged in
    ui.showError("Please connect audio device.");
    return _trackNum == 999;
  }
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
  enableResampler(_fsPhysical > 24000);     // enable 15/16 resampler if necessary
  delay(500);                               // wait for things to settle ...
  setVolume(4,4);                           // raise volume back up for playback

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

  // Adafruit VS1035 breakout uses a 12.288 Mhz XTALI, upper limit for sample
  // rate at 48 kHz.  Using the 15/16 resampler we have more headroom for
  // increasing the playback rate.  The valid range for CLOCKF register is:
  // -187000 to 511999S
  //
  //   ORIGINAL    15/16     RANGE FOR CLOCKF   REL CHANGE
  //   --------   --------   ----------------   ----------
  //    8000 Hz    7500 Hz   -187000 - 511999   63% - 200%
  //   11025 Hz   10336 Hz   -187000 - 511999   63% - 200%
  //   12000 Hz   11250 Hz   -187000 - 511999   63% - 200%
  //   16000 Hz   15000 Hz   -187000 - 511999   63% - 200%
  //   22050 Hz   20672 Hz   -187000 - 511999   63% - 200%
  //   24000 Hz   22500 Hz   -187000 - 511999   63% - 200%
  //   32000 Hz   30000 Hz   -187000 - 307200   63% - 160%
  //   44100 Hz   41344 Hz   -187000 -  82427   63% - 116%
  //   48000 Hz   45000 Hz   -187000 -  34133   63% - 107%
  //
  // See section 1.5 of
  // https://www.vlsi.fi/fileadmin/software/VS10XX/vs1053b-patches.pdf

  switch (_fsPhysical) {
    case 32000: myPID.SetOutputLimits(-187000, 307200); break;
    case 44100: myPID.SetOutputLimits(-187000,  82430); break;
    case 48000: myPID.SetOutputLimits(-187000,  34133); break;
    default:    myPID.SetOutputLimits(-187000, 511999); break;
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
      PRINTLN("Pausing playback.");
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
  //PRINTF("Input:%7.0f,Output:%7.0f,FrameOffset:%4d\n", (float) delta/10, Output, _frameOffset);
  //PRINTF("Input:%0.3f,Output:%0.3f\n", Input, Output);
  //PRINTF("P:%0.5f,I:%0.5f,D:%0.5f\n", myPID.GetPterm(), myPID.GetIterm(), myPID.GetDterm());
  //PRINTF("SteamBufferFill:%4d,AudioBufferFill:%4d,AudioBufferUnderflow:%2d\n",StreamBufferFillWords(),AudioBufferFillWords(),AudioBufferUnderflow());
}

void Audio::drawPlayingMenuConstants() {
  u8g2->setFont(FONT08);
  u8g2->drawStr(0, 8, projector.config().name);
  char buffer[9];
  strcpy(buffer, (_isLoop) ? "Loop 000" : "Film 000");
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
  uint16_t sr = sciRead(VS1053_REG_AUDATA) & 0xfffe; // Mask the Mono/Stereo Bit
  return (sr == 11024) ? 11025 : sr;          // return (corrected) sample rate
}

bool Audio::loadTrack(uint16_t trackNum) {
  for (bool isLoop : { false, true })  {
    strcpy(_filename, (isLoop) ? "000-00-L.ogg" : "000-00.ogg");
    ui.insertPaddedInt(&_filename[0], trackNum, 10, 3);
    for (_fps=12; _fps<=25; _fps++) {                             // guess fps
      ui.insertPaddedInt(&_filename[4], _fps, 10, 2);
      if (SD.exists(_filename)) {                                 // file found!
        _trackNum = trackNum;
        _isLoop = isLoop;
        return true;
      }
    }
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

void Audio::enableResampler(bool enable) {
  // See section 1.6 of
  // https://www.vlsi.fi/fileadmin/software/VS10XX/vs1053b-patches.pdf
  sciWrite(VS1053_REG_WRAMADDR, 0x1e09);
  if (enable) {
    sciWrite(VS1053_REG_WRAM, 0x0080);
    PRINTLN("15/16 Resampler enabled.");
  } else {
    sciWrite(VS1053_REG_WRAM, 0x0080);
    PRINTLN("15/16 Resampler disabled.");
  }
}

void Audio::adjustSamplerate(int32_t ppm2) {
  sciWrite(VS1053_REG_WRAMADDR, 0x1e07);
  sciWrite(VS1053_REG_WRAM, ppm2);
  sciWrite(VS1053_REG_WRAM, ppm2 >> 16);
  sciWrite(VS1053_REG_WRAMADDR, 0x5b1c);
  sciWrite(VS1053_REG_WRAM, 0);
  sciWrite(VS1053_REG_AUDATA, sciRead(VS1053_REG_AUDATA));
}

uint32_t Audio::getSampleCount() {
  // See section 1.3 of
  // https://www.vlsi.fi/fileadmin/software/VS10XX/vs1053b-patches.pdf
  return sciRead32(VS1053_XMEM_SAMPLECOUNT);
}

void Audio::clearSampleCounter() {
  sciWrite(VS1053_REG_WRAMADDR, VS1053_XMEM_SAMPLECOUNT);
  sciWrite(VS1053_REG_WRAM, 0);
  sciWrite(VS1053_REG_WRAM, 0);
}

void Audio::restoreSampleCounter(uint32_t samplecounter) {
  sciWrite(VS1053_REG_WRAMADDR, VS1053_XMEM_SAMPLECOUNT); // MSB
  sciWrite(VS1053_REG_WRAM, samplecounter);
  sciWrite(VS1053_REG_WRAM, samplecounter >> 16);
}

void Audio::clearErrorCounter() {
  sciWrite(VS1053_REG_WRAMADDR, 0x5a82);
  sciWrite(VS1053_REG_WRAM, 0);
}

uint32_t Audio::sciRead32(uint16_t addr) {
  // See section 1.3 of
  // https://www.vlsi.fi/fileadmin/software/VS10XX/vs1053b-patches.pdf
  uint16_t msbV1, lsb, msbV2;
  sciWrite(VS1053_REG_WRAMADDR, addr + 1);
  msbV1 = (uint16_t)sciRead(VS1053_REG_WRAM);
  sciWrite(VS1053_REG_WRAMADDR, addr);
  lsb = (u_int32_t)sciRead(VS1053_REG_WRAM);
  msbV2 = (uint16_t)sciRead(VS1053_REG_WRAM);
  if (lsb < 0x8000U) {
    msbV1 = msbV2;
  }
  return ((u_int32_t)msbV1 << 16) | lsb;
}

uint16_t Audio::getBitrate() {
  return sciRead(VS1053_REG_HDAT0) << 3;
}

const char Audio::getRevision() {
  char rev = 0;
  for (uint8_t i = 0; i<4; i++) {
    if (GPIO_digitalRead(i))
      bitSet(rev, i);
  }
  return rev + 65;
}

bool Audio::isOgg() {
  return sciRead(VS1053_REG_HDAT1) == 0x4F67;
}

size_t Audio::findInFile(File *file, const char *target, uint8_t length, size_t startPos) {
  char buf[length];
  for (uint64_t i = startPos; i <= file->size() - length; i++) {
    file->seek(i);
    file->read(buf, length);
    if (!strncmp(buf, target, length)) {
      file->seek(i);
      return file->position();
    }
  }
  return UINT32_MAX;
}

size_t Audio::firstAudioPage(File *file) {
  const char vorbisID[7] = {5, 'v', 'o', 'r', 'b', 'i', 's'}; // search-string for Vorbis setup header
  size_t pos = findInFile(file, vorbisID, 7, 0);              // find position of Vorbis setup header
  return findInFile(file, "OggS", 4, pos);                    // audio starts on the next Ogg page
}

int64_t Audio::granulePos(oggPage *og) {
  unsigned char *page = og->header;
  int64_t granulepos = page[13] & (0xff);
  granulepos = (granulepos << 8) | (page[12] & 0xff);
  granulepos = (granulepos << 8) | (page[11] & 0xff);
  granulepos = (granulepos << 8) | (page[10] & 0xff);
  granulepos = (granulepos << 8) | (page[ 9] & 0xff);
  granulepos = (granulepos << 8) | (page[ 8] & 0xff);
  granulepos = (granulepos << 8) | (page[ 7] & 0xff);
  granulepos = (granulepos << 8) | (page[ 6] & 0xff);
  return (granulepos);
}

int16_t Audio::StreamBufferFillWords(void) {
  uint16_t wrp, rdp;
  int16_t res;
  /* For FLAC files, stream buffer is larger */
  int16_t bufSize = (sciRead(VS1053_REG_HDAT1) == 0x664C) ? 0x1800 : 0x400;
  sciWrite(VS1053_REG_WRAMADDR, 0x5A7D);
  wrp = sciRead(VS1053_REG_WRAM);
  rdp = sciRead(VS1053_REG_WRAM);
  res = wrp-rdp;
  if (res < 0) {
    return res + bufSize;
  }
  return res;
}

int16_t Audio::StreamBufferFreeWords(void) {
  /* For FLAC files, stream buffer is larger */
  int16_t bufSize = (sciRead(VS1053_REG_HDAT1) == 0x664C) ? 0x1800 : 0x400;
  int16_t res = bufSize - StreamBufferFillWords();
  if (res < 2) {
    return 0;
  }
  return res-2;
}

int16_t Audio::AudioBufferFillWords(void) {
  uint16_t wrp, rdp;
  sciWrite(VS1053_REG_WRAMADDR, 0x5A80);
  wrp = sciRead(VS1053_REG_WRAM);
  rdp = sciRead(VS1053_REG_WRAM);
  return (wrp-rdp) & 4095;
}

int16_t Audio::AudioBufferFreeWords(void) {
  int16_t res = 4096 - AudioBufferFillWords();
  if (res < 2) {
    return 0;
  }
  return res-2;
}

uint16_t Audio::AudioBufferUnderflow(void) {
  uint16_t uFlow;
  sciWrite(VS1053_REG_WRAMADDR, 0x5A82);
  uFlow = sciRead(VS1053_REG_WRAM);
  if (uFlow) {
    sciWrite(VS1053_REG_WRAMADDR, 0x5A82);
    sciWrite(VS1053_REG_WRAM, 0); /* Clear */
  }
  return uFlow;
}
