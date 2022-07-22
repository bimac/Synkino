const char *uCVersion = "SynkinoLC v1.0 alpha";
char boardRevision[20] = "Hardware Revision  ";
#if defined(__MKL26Z64__)
  const char *uC = "with Teensy LC";
#elif defined(__MK20DX256__)
  const char *uC = "with Teensy 3.1 / 3.2";
#elif defined(ARDUINO_TEENSY40)
  const char *uC = "with Teensy 4.0";
#endif

#include <Arduino.h>
#include <U8g2lib.h>
#include <EEPROM.h>
#include <PID_v1.h>

#include "TeensyTimerTool.h"
using namespace TeensyTimerTool;

#include <EncoderTool.h>
using namespace EncoderTool;

#include "vs1053b.h"
#include "buzzer.h"
#include "projector.h"

#include "serialdebug.h"  // macros for serial debugging
#include "menus.h"        // menu definitions, positions of menu items
#include "pins.h"         // pin definitions
#include "states.h"       // state definitions
#include "timeconst.h"    // useful time constants and macros
#include "vars.h"         // initialization of constants and vars
#include "xbm.h"          // XBM graphics

// Initialize Objects
VS1053B musicPlayer(VS1053_RST, VS1053_CS, VS1053_DCS, VS1053_DREQ, VS1053_SDCS, VS1053_SDCD);
U8G2* u8g2;
PolledEncoder enc;
PeriodicTimer encTimer(TCK);
#if defined(__MKL26Z64__)
  OneShotTimer dimmingTimer(TCK);
#else
  OneShotTimer dimmingTimer(TCK64);
#endif
Buzzer buzzer(PIN_BUZZER);
Projector projector;

double Setpoint, Input, Output;
double Kp=8, Ki=3, Kd=1;  // PonM WINNER für 16 Readings, but with fixed int overflow
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, P_ON_M, DIRECT);

#define DISPLAY_DIM_AFTER   10s
#define DISPLAY_CLEAR_AFTER 5min

#define FONT10 u8g2_font_helvR10_tr
#define FONT08 u8g2_font_helvR08_tr

void setup(void) {

  #ifdef SERIALDEBUG
  Serial.begin(9600);
  #endif

  PRINTF("Welcome to %s\n\n",uCVersion);

  // set pin mode
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(OLED_DET, INPUT_PULLDOWN);
  pinMode(IMPULSE, INPUT_PULLDOWN);
  pinMode(STARTMARK, INPUT_PULLDOWN);

  // set HW SPI pins
  SPI.setMOSI(SPI_MOSI);
  SPI.setMISO(SPI_MISO);
  SPI.setSCK(SPI_SCK);

  // initialize display / show boot splash
  PRINTLN("Initializing display ...");
  if (digitalReadFast(OLED_DET))
    u8g2 = new U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI(U8G2_R2, OLED_CS, OLED_DC, OLED_RST);
  else
    u8g2 = new U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
  u8g2->begin();
  for (int8_t y = -logo_xbm_height; y <= logo_xbm_y; y++) {
    u8g2->clearBuffer();
    u8g2->drawXBMP(logo_xbm_x, y, logo_xbm_width, logo_xbm_height, logo_xbm_bits);
    u8g2->sendBuffer();
    //delay(10);
  }
  u8g2->drawXBMP(logolc_xbm_x, logolc_xbm_y, logolc_xbm_width, logolc_xbm_height, logolc_xbm_bits);
  u8g2->drawXBMP(ifma_xbm_x, ifma_xbm_y, ifma_xbm_width, ifma_xbm_height, ifma_xbm_bits);
  u8g2->sendBuffer();
  buzzer.playHello();
  dimmingTimer.begin([] { dimDisplay(0); });
  dimmingTimer.trigger(DISPLAY_DIM_AFTER);
  u8g2->setFont(FONT10);
  u8g2->setFontRefHeightText();

  // initialize encoder
  PRINTLN("Initializing encoder ...");
  enc.begin(ENC_A, ENC_B, ENC_BTN, CountMode::half, INPUT_PULLUP);        // Adjust CountMode if necessary
  encTimer.begin([]() { enc.tick(); }, 5_kHz);                            // Poll encoder at 5kHz
  enc.attachCallback([](int position, int delta) { dimDisplay(true); });  // Wake up display on encoder input
  enc.attachButtonCallback([](int state) { dimDisplay(true); });          // Wake up display on button input

  // initialize VS1053B breakout
  while (!musicPlayer.SDinserted())
    showError("Please insert", "SD card");
  switch (musicPlayer.begin()) {
    case 1: showError("Could not initialize", "SD card"); break;
    case 2: showError("Could not initialize", "VS1053B Breakout"); break;
    case 3: showError("Could not apply", "patches.053");
  }
  boardRevision[18] = musicPlayer.getRevision();
  myState = MAIN_MENU;
  restoreLastProjectorUsed();

  PRINTLN("Startup complete.\n");
}

void PULSE_ISR() {
  if (digitalReadFast(IMPULSE)) {
    digitalWriteFast(LED_BUILTIN, HIGH);
    buzzer.play(4400);
  } else {
    digitalWriteFast(LED_BUILTIN, LOW);
    buzzer.quiet();
  }
}

void loop(void) {
  switch (myState) {
  case MAIN_MENU:

    mainMenuSelection = u8g2->userInterfaceSelectionList("Main Menu", MENU_ITEM_SELECT_TRACK, main_menu);
    switch (mainMenuSelection) {
    case MENU_ITEM_PROJECTOR:

      projectorActionMenuSelection = u8g2->userInterfaceSelectionList("Projector", MENU_ITEM_SELECT, projector_action_menu);
      switch (projectorActionMenuSelection) {
      case MENU_ITEM_NEW:
        byte projectorCount;
        projectorCount = EEPROM.read(0);
        if (projectorCount < 8) {
          gatherProjectorData();
          saveProjector(NEW);
        } else
          showError("Too many Projectors.","Only 8 allowed.");
        break;
      case MENU_ITEM_SELECT:
        makeProjectorSelectionMenu();
        projectorSelectionMenuSelection = u8g2->userInterfaceSelectionList("Select Projector", lastProjectorUsed, projectorSelection_menu);
        loadProjectorConfig(projectorSelectionMenuSelection);
        break;
      case MENU_ITEM_EDIT:
        makeProjectorSelectionMenu();
        projectorSelectionMenuSelection = u8g2->userInterfaceSelectionList("Edit Projector", lastProjectorUsed, projectorSelection_menu);
        loadProjectorConfig(projectorSelectionMenuSelection);
        gatherProjectorData();
        saveProjector(projectorSelectionMenuSelection);
        break;
      case MENU_ITEM_DELETE:
        byte currentProjectorCount = makeProjectorSelectionMenu();
        projectorSelectionMenuSelection = u8g2->userInterfaceSelectionList("Delete Projector", currentProjectorCount, projectorSelection_menu);
        projector.remove(projectorSelectionMenuSelection);
        break;
      }
      break;

    case MENU_ITEM_SELECT_TRACK:
      myState = SELECT_TRACK;
      break;

    case MENU_ITEM_EXTRAS:
      extrasMenuSelection = u8g2->userInterfaceSelectionList("Extras", MENU_ITEM_VERSION, extras_menu);
      switch (extrasMenuSelection) {
      case MENU_ITEM_VERSION:
        userInterfaceMessage(uCVersion, boardRevision, uC, " Nice! ");
        break;
      case MENU_ITEM_TEST_IMPULSE:
        attachInterrupt(digitalPinToInterrupt(IMPULSE), PULSE_ISR, CHANGE);
        userInterfaceMessage("Test Impulse", "", "", "Done");
        detachInterrupt(digitalPinToInterrupt(IMPULSE));
        break;
      case MENU_ITEM_DEL_EEPROM:
        if (userInterfaceMessage("Delete EEPROM", "Are you sure?", "", " Cancel \n Yes ") == 2)
          projector.e2delete();
        break;
      case MENU_ITEM_DUMP:
        projector.e2dump();
        break;
      }
      break;
    }
    break;
  case SELECT_TRACK:
    static int trackChosen;
    trackChosen = selectTrackScreen();
    if (trackChosen != 0) {
      if (loadTrackByNo(trackChosen))
        myState = WAIT_FOR_LOADING;
      else
        myState = SELECT_TRACK;
    } else {
      myState = MAIN_MENU;
    }
    break;
  case WAIT_FOR_LOADING:
    drawBusyBee(90, 10);
    if ((fps != 0) && (trackLoaded != 0)) {
      drawWaitForPlayingMenu(trackChosen, fps);
      myState = TRACK_LOADED;
    }
    break;
  case TRACK_LOADED:
    if (startMarkHit != 0) {
      myState = SYNC_PLAY;
    }
    // Now check for Sync Pos Adjustments
    if (!enc.getButton()) {
      waitForBttnRelease();
      trackLoadedMenuSelection = u8g2->userInterfaceSelectionList("Playback", MENU_ITEM_EXIT, trackLoaded_menu);
      switch (trackLoadedMenuSelection) {
      case MENU_ITEM_MANUALSTART:
        startMarkHit = 1;
        //tellAudioPlayer(CMD_SET_STARTMARK, 0);
        myState = SYNC_PLAY;
        break;
      case MENU_ITEM_STOP:
        //tellAudioPlayer(CMD_RESET, 0);
        myState = MAIN_MENU;
        break;
      case MENU_ITEM_EXIT:
        drawWaitForPlayingMenu(trackChosen, fps);
        myState = TRACK_LOADED;
        break;
      }
    }
    break;
  case SYNC_PLAY:
    drawPlayingMenu(trackChosen, fps);
    if (!enc.getButton()) {
      waitForBttnRelease();
      //handleFrameCorrectionOffsetInput(); // Take Correction Input
      //waitForBttnRelease();
      //tellAudioPlayer(CMD_SYNC_OFFSET, newSyncOffset);
    }
    break;
  }
}

void saveProjector(byte thisProjector) {
  byte projectorCount;
  projectorCount = EEPROM.read(0);

  EEPROM_Projector aProjector;
  aProjector.shutterBladeCount = shutterBladesMenuSelection;
  aProjector.startmarkOffset = newStartmarkOffset;
  aProjector.p = new_p;
  aProjector.i = new_i;
  aProjector.d = new_d;
  strcpy(aProjector.name, newProjectorName);

  if (thisProjector == NEW) { // We have a NEW projector here
    EEPROM.update(0, projectorCount + 1);
    EEPROM.update(1, projectorCount + 1);
    aProjector.index = projectorCount + 1;
    EEPROM.put((projectorCount * sizeof(aProjector) + 2), aProjector);
    projector.e2dump();

  } else {
    aProjector.index = thisProjector;
    EEPROM.update(1, thisProjector);
    EEPROM.put(((thisProjector - 1) * sizeof(aProjector) + 2), aProjector);
    projector.e2dump();

    // tellAudioPlayer(CMD_SET_SHUTTERBLADES, aProjector.shutterBladeCount);
    // tellAudioPlayer(CMD_SET_STARTMARK, aProjector.startmarkOffset);
    // tellAudioPlayer(CMD_SET_P, aProjector.p);
    // tellAudioPlayer(CMD_SET_I, aProjector.i);
    // tellAudioPlayer(CMD_SET_D, aProjector.d);
  }
}

void gatherProjectorData() {
  handleProjectorNameInput();
  shutterBladesMenuSelection = u8g2->userInterfaceSelectionList("# Shutter Blades", shutterBladesMenuSelection, shutterblade_menu);
  u8g2->userInterfaceInputValue("Start Mark Offset:", "", &newStartmarkOffset, 1, 255, 3, " Frames");
  u8g2->userInterfaceInputValue("Proportional:", "", &new_p, 0, 99, 2, "");
  u8g2->userInterfaceInputValue("Integral:",     "", &new_i, 0, 99, 2, "");
  u8g2->userInterfaceInputValue("Derivative:",   "", &new_d, 0, 99, 2, "");
}

void loadProjectorConfig(uint8_t projNo) {
  EEPROM_Projector aProjector;
  EEPROM.get((projNo - 1) * sizeof(aProjector) + 2, aProjector);
  shutterBladesMenuSelection = aProjector.shutterBladeCount;
  newStartmarkOffset = aProjector.startmarkOffset;
  new_p = aProjector.p;
  new_i = aProjector.i;
  new_d = aProjector.d;

  strcpy(newProjectorName, aProjector.name);

  // tellAudioPlayer(CMD_SET_SHUTTERBLADES, aProjector.shutterBladeCount);
  // tellAudioPlayer(CMD_SET_STARTMARK, aProjector.startmarkOffset);
  // tellAudioPlayer(CMD_SET_P, aProjector.p);
  // tellAudioPlayer(CMD_SET_I, aProjector.i);
  // tellAudioPlayer(CMD_SET_D, aProjector.d);

  EEPROM.write(1, aProjector.index);
  // projector.e2dump();
}

byte makeProjectorSelectionMenu() {
  projectorSelection_menu[0] = 0; // Empty this out and be prepared for an empty list
  byte projectorCount;
  projectorCount = EEPROM.read(0);
  lastProjectorUsed = EEPROM.read(1);

  EEPROM_Projector aProjector;
  for (byte i = 0; i < projectorCount; i++) {
    EEPROM.get(i * sizeof(aProjector) + 2, aProjector);
    strcat(projectorSelection_menu, aProjector.name);
    if (i < (projectorCount - 1)) {           // no \n for the last menu entry
      strcat(projectorSelection_menu, "\n");
    }
  }
  return projectorCount;
}

void waitForBttnRelease() {
  while (!enc.getButton()) {
    yield();
  }
}

void handleFrameCorrectionOffsetInput() {
  // int16_t parentMenuEncPosition;
  // parentMenuEncPosition = myEnc.read();
  // int16_t newEncPosition;
  // int16_t oldPosition;

  // myEnc.write(16000 + (newSyncOffset << 1));  // this becomes 0 further down after shifting and modulo
  // while (digitalRead(ENCODER_BTN) == 1) {     // adjust ### as long as button not pressed
  //   newEncPosition = myEnc.read();
  //   if (encType == 30) {
  //     newSyncOffset = ((newEncPosition >> 1) - 8000);
  //   } else {
  //     newSyncOffset = ((newEncPosition >> 2) - 4000);
  //   }
  //   if (newEncPosition != oldPosition) {
  //     oldPosition = newEncPosition;
  //     Serial.println(newSyncOffset);
  //     u8g2.firstPage();
  //     do {

  //       // Draw Header & Footer
  //       u8g2.setFont(u8g2_font_helvR10_tr);
  //       if (newSyncOffset == 0) {
  //         u8g2.drawStr(6,12,"Adjust Sync Offset");
  //       } else if (newSyncOffset < 0) {
  //         u8g2.drawStr(11,12,"Delay Sound by");
  //       } else if (newSyncOffset > 0) {
  //         u8g2.drawStr(6,12,"Advance Sound by");
  //       }
  //       u8g2.drawStr(40,64,"Frames");

  //       u8g2.setFont(u8g2_font_inb24_mn);
  //       if (newSyncOffset >= 0 && newSyncOffset <= 9) {
  //         u8g2.setCursor(55, 46);
  //       } else if (newSyncOffset >= -9 && newSyncOffset <= -1) {
  //         u8g2.setCursor(45, 46);
  //       } else if (newSyncOffset >= 10 && newSyncOffset <= 99) {
  //         u8g2.setCursor(45, 46);
  //       } else if (newSyncOffset >= -99 && newSyncOffset <= -10) {
  //         u8g2.setCursor(35, 46);
  //       } else if (newSyncOffset >= 100 && newSyncOffset <= 999) {
  //         u8g2.setCursor(35, 46);
  //       } else if (newSyncOffset >= -999 && newSyncOffset <= -100) {
  //         u8g2.setCursor(25, 46);
  //       }

  //       u8g2.print(newSyncOffset);
  //     } while ( u8g2.nextPage() );
  //   }
  // }

  // waitForBttnRelease();
  // oldPosition = 0;
  // myEnc.write(parentMenuEncPosition);
  // u8g2.setFont(u8g2_font_helvR10_tr);   // Only until we go to the PLAYING_MENU here
}

uint16_t selectTrackScreen() {
  static uint16_t trackNo = 1;
  bool first = true;
  enc.setValue((trackNo > 0) ? trackNo : 1);
  enc.setLimits(0,999);
  while (enc.getButton()) {
    yield();
    if (enc.valueChanged() || first) {
      first   = false;
      trackNo = enc.getValue();
      buzzer.playClick();
      u8g2->clearBuffer();
      if (trackNo == 0) {
        u8g2->setFont(FONT10);
        u8g2->drawStr(18,35,"< Main Menu");
      } else {
        u8g2->setFont(u8g2_font_inb46_mn);
        u8g2->setCursor(9, 55);
        if (trackNo <  10) u8g2->print("0");
        if (trackNo < 100) u8g2->print("0");
        u8g2->print(trackNo);
      }
      u8g2->sendBuffer();
    }
  }
  waitForBttnRelease();
  enc.setLimits(-999,999);
  enc.setValue(0);
  u8g2->setFont(FONT10);
  return trackNo;
}

// void updateFpsDependencies(uint8_t fps) {
//   sollfps = fps;                                // redundant? sollfps is global, fps is local. Horrible.
//   pauseDetectedPeriod = (1000 / fps * 3);
//   sampleCountRegisterValid = true;           // It takes 8 KiB until the Ogg Sample Counter is valid for sure
//   impToSamplerateFactor = physicalSamplingrate / fps / shutterBlades / 2;
//   deltaToFramesDivider = physicalSamplingrate / fps;
//   impToAudioSecondsDivider = sollfps * shutterBlades * 2;
// }

uint8_t loadTrackByNo(uint16_t trackNo) {

  // Yes, its ugly - but avoiding sprintf saves some space
  char trackName[11] = "000-00.ogg";
  char buffer[4];
  itoa(trackNo, buffer, 10);
  memcpy(&trackName[3-strlen(buffer)], &buffer, strlen(buffer));
  for (uint8_t fpsGuess = 12; fpsGuess <= 25; fpsGuess++) {
    itoa(fpsGuess, buffer, 10);
    memcpy(&trackName[4], &buffer, 2);
    if (SD.exists(trackName)) {
      fps = fpsGuess;
      //updateFpsDependencies(fpsGuess);
      //tellFrontend(CMD_FOUND_FPS, fpsGuess);
      break;
    }
  }

  if (!SD.exists(trackName))
    return showError("File not found.", "");
  if (!audioConnected())
    return showError("No audio device connected.", "");

  musicPlayer.setVolume(0,0);
  PRINTF("Loading \"%s\"\n", trackName);
  if (!musicPlayer.startPlayingFile(trackName))
    return showError("Cannot open file.", "");

  musicPlayer.sciWrite(SCI_DECODE_TIME, 0); // Reset the Decode and bitrate from previous play back
  delay(100);

  musicPlayer.enableResampler();

  float physicalSamplingrate = musicPlayer.read16BitsFromSCI(SCI_AUDATA) & 0xfffe;  // Mask the Mono/Stereo Bit
  
//       updateFpsDependencies(sollfps);   // TODO: do it again, this time to adjust for sampling rates. Maybe betteer as a second function?

  PRINTF("Physical samplingrate: %0.1f Hz",physicalSamplingrate);

//   //    PRINT(F("HDAT1 (4F67) :"));                    // Would determine file type, see 9.6.9 in data sheet
//   //    PRINTLN(Read16BitsFromSCI(SCI_HDAT1), HEX);

  musicPlayer.pausePlaying(true);

  PRINTLN("Waiting for start mark ...");

  musicPlayer.setVolume(255,255);

  musicPlayer.clearSampleCounter();

//       myState = TRACK_LOADED;
  trackLoaded = 1;

  return true;
}

void drawWaitForPlayingMenu(int trackNo, byte fps) {
  u8g2->clearBuffer();
  u8g2->setFont(FONT08);
  u8g2->setCursor(0,8);
  u8g2->print(newProjectorName);
  u8g2->setCursor(90,8);
  u8g2->print("Film ");
  if (trackNo < 10)  u8g2->print("0");
  if (trackNo < 100) u8g2->print("0");
  u8g2->print(trackNo);
  u8g2->setCursor(98,62);
  u8g2->print(fps);
  u8g2->print(" fps");
  u8g2->setFont(FONT10);
  u8g2->drawStr(30,28,"Waiting for");
  u8g2->drawStr(25,46,"Film to Start");
  u8g2->drawXBMP(60, 54, pause_xbm_width, pause_xbm_height, pause_xbm_bits);
  u8g2->sendBuffer();
}

void drawPlayingMenu(int trackNo, byte fps) {
  unsigned long currentmillis = millis();
  u8g2->clearBuffer();
  u8g2->setFont(FONT08);
  u8g2->setCursor(0,8);
  u8g2->print(newProjectorName);
  u8g2->setCursor(90,8);
  u8g2->print("Film ");
  if (trackNo < 10)  u8g2->print("0");
  if (trackNo < 100) u8g2->print("0");
  u8g2->print(trackNo);
  u8g2->setCursor(98,62);
  u8g2->print(fps);
  u8g2->print(" fps");
  u8g2->setFont(u8g2_font_inb24_mn);
  u8g2->drawStr(20,36,":");
  u8g2->drawStr(71,36,":");
  u8g2->setCursor(4,40);
  u8g2->print(myHours);
  u8g2->setCursor(35,40);
  if (myMinutes < 10) u8g2->print("0");
  u8g2->print(myMinutes);
  u8g2->setCursor(85,40);
  if (mySeconds < 10) u8g2->print("0");
  u8g2->print(mySeconds);

  if (projectorPaused == 1) {
    u8g2->drawXBMP(60, 54, pause_xbm_width, pause_xbm_height, pause_xbm_bits);
  } else {
    u8g2->drawXBMP(60, 54, play_xbm_width, play_xbm_height, play_xbm_bits);
  }

  if (oosyncFrames == 0) {
    u8g2->drawXBMP(2, 54, sync_xbm_width, sync_xbm_height, sync_xbm_bits);
  } else {
    if (currentmillis % 700 > 350) {
      u8g2->drawXBMP(2, 54, sync_xbm_width, sync_xbm_height, sync_xbm_bits);
    }
    u8g2->setFont(FONT08);
    u8g2->setCursor(24,62);
    if (oosyncFrames > 0) u8g2->print("+");
    u8g2->print(oosyncFrames);
  }
  u8g2->sendBuffer();
}

void drawBusyBee(byte x, byte y) {
  u8g2->clearBuffer();
  u8g2->drawXBMP(x+random(-1,1), y+random(-1,1), busybee_xbm_width, busybee_xbm_height, busybee_xbm_bits);
  u8g2->setFont(FONT10);
  u8g2->drawStr(8,50,"Loading...");
  u8g2->sendBuffer();
}

void handleProjectorNameInput() {
  uint8_t newEncPosition = 0;
  uint8_t oldEncPosition = 0;
  byte charIndex = 0;
  bool inputFinished = false;
  unsigned long lastMillis;
  bool firstUse = true;
  bool tonePlayedYet = false;
  bool stopCursorBlinking = false;
  bool isIncrement;

  if (projectorActionMenuSelection == MENU_ITEM_EDIT) {
    if (strlen(newProjectorName) == MAX_PROJECTOR_NAME_LENGTH) {
      const char lastChar = newProjectorName[MAX_PROJECTOR_NAME_LENGTH - 1];
      newProjectorName[MAX_PROJECTOR_NAME_LENGTH - 1] = '\0';
      enc.setValue(lastChar);
    } else {
      enc.setValue(127); // to start with "Delete"
    }
    charIndex = strlen(newProjectorName);
    firstUse = false;
    for (byte i = charIndex + 1; i < MAX_PROJECTOR_NAME_LENGTH; i++) {
      newProjectorName[i] = 0;
    }
  } else {
    for (byte i = 0; i < MAX_PROJECTOR_NAME_LENGTH; i++) {
      newProjectorName[i] = 0;
    }
    enc.setValue('A');
  }

  oldEncPosition = enc.getValue();
  while (charIndex < MAX_PROJECTOR_NAME_LENGTH && !inputFinished) {
    while (enc.getButton()) {
      if (enc.valueChanged() || !newEncPosition) {
        buzzer.playClick();

        newEncPosition = enc.getValue();

        //if (newEncPosition <  32 || newEncPosition > 127 )

        isIncrement = newEncPosition > oldEncPosition;
        if (newEncPosition <  32 || newEncPosition > 127 )        // roll-over
          newEncPosition = (isIncrement) ?  65 : 122;
        else if (newEncPosition >  32 && newEncPosition <  48 )   // Chr :  Ascii
          newEncPosition = (isIncrement) ?  48 :  32;             // ----:---------
        else if (newEncPosition >  57 && newEncPosition <  65 )   // A-Z : 65 -  90
          newEncPosition = 127;                                   // a-z : 97 - 122
        else if (newEncPosition >  90 && newEncPosition <  97 )   // Spc :       32
          newEncPosition = (isIncrement) ?  97 :  90;             // 0-9 : 48 -  57
        else if (newEncPosition > 122 && newEncPosition < 127 )   // Del :      127
          newEncPosition = (isIncrement) ?  32 :  57;
        enc.setValue(newEncPosition);

        oldEncPosition = newEncPosition;
        stopCursorBlinking = true;
        tonePlayedYet = false;
      }
      lastMillis = millis();
      handleStringInputGraphically(GET_NAME, newEncPosition, lastMillis, firstUse, stopCursorBlinking);
      stopCursorBlinking = false;
    }

    lastMillis = millis();
    while (!enc.getButton() && !inputFinished) {
      if (!tonePlayedYet) {
        buzzer.playPress();
        tonePlayedYet = true;
      }
      delay(50);
      inputFinished = handleStringInputGraphically(JUST_PRESSED, newEncPosition, lastMillis, firstUse, false);
    }
    while (!enc.getButton() && inputFinished) {
      handleStringInputGraphically(LONG_PRESSED, newEncPosition, 0, firstUse, false);
    }

    if (newEncPosition == 127 && !inputFinished) {   // Delete
      if (charIndex > 0) {
        charIndex--;
      }
      PRINTLN(charIndex);
      newProjectorName[charIndex] = 0;

    } else if (!inputFinished) {
      newProjectorName[charIndex] = newEncPosition;
      charIndex++;
      if (firstUse) enc.setValue('a');
      firstUse = false;
    }
  }
  while (digitalRead(ENC_BTN) == 0) {
    yield();
  }
  newProjectorName[charIndex] = '\0';
}

bool handleStringInputGraphically(byte action, char localChar, unsigned long lastMillis, bool firstUse, bool charChanged) {
  static char lastLocalChar = 0;
  static bool lastCursor = true;
  bool cursor = millis() % 600 < 400;
  yield();

  // Avoid unneccessary calls to u8g2
  if (localChar==lastLocalChar && cursor==lastCursor)
    return false;
  lastLocalChar = localChar;
  lastCursor = cursor;

  // Draw heading
  u8g2->clearBuffer();
  drawStrCentered(14, "Set Projector Name", FONT08);

  // Draw user input
  u8g2->setFont(FONT10);
  u8g2->setCursor(15, 35);
  u8g2->print(newProjectorName);
  if (action==GET_NAME && cursor) {
    if (localChar == 32)
      u8g2->print("_");
    else if (localChar == 127) {
      u8g2->setFont(u8g2_font_m2icon_9_tf);
      u8g2->print("a"); // https://github.com/olikraus/u8g2/wiki/fntpic/u8g2_font_m2icon_9_tf.png
    } else
      u8g2->print(localChar);
  }

  // Draw footer
  if (action==GET_NAME) {
    if (localChar == 32)
      drawStrCentered(55, "[Space]", FONT08);
    else if (localChar == 127)
      drawStrCentered(55, "[Delete last]", FONT08);
    else
      drawStrCentered(55, (firstUse) ? "[Turn and push Knob]" : "[Long Press to Finish]", FONT08);
  } else if (action == JUST_PRESSED) {
    drawStrCentered(55, "[Keep pressed to Save]", FONT08);
    if (millis() - lastMillis > 1500) {
      buzzer.playConfirm();
      u8g2->setFont(FONT10);
      u8g2->sendBuffer();
      return true;
    }
  } else if (action == LONG_PRESSED)
    drawStrCentered(55, "[Saved!]", FONT08);

  u8g2->setFont(FONT10);
  u8g2->sendBuffer();
  return false;
}

// Draw a string that is horizontally centered
void drawStrCentered(u8g2_uint_t y, const char *s, const uint8_t *font_8x8) {
  u8g2->setFont(font_8x8);
  u8g2_uint_t x = (128-u8g2->getStrWidth(s))/2;
  u8g2->drawStr(x, y, s);
}

// This overwrites the weak function in u8x8_debounce.c
uint8_t u8x8_GetMenuEvent(u8x8_t *u8x8) {
    yield();

    if (enc.valueChanged()) {
      buzzer.playClick();               // feedback sound
      int encVal = enc.getValue();      // get encoder value
      enc.setValue(0);                  // reset encoder

      if (encVal < 0)
        return U8X8_MSG_GPIO_MENU_UP;
      else
        return U8X8_MSG_GPIO_MENU_DOWN;

    } if (!enc.getButton()) {
      buzzer.playPress();               // feedback sound
      while (!enc.getButton())          // wait for release
        yield();
      return U8X8_MSG_GPIO_MENU_SELECT;

    } else
      return 0;
}

bool showError(const char* errorMsg1, const char* errorMsg2) {
  PRINTF("ERROR: %s %s\n", errorMsg1, errorMsg2);
  buzzer.playError();
  userInterfaceMessage("ERROR", errorMsg1, errorMsg2, " Okay ");
  return false;
}

uint8_t userInterfaceMessage(const char *title1, const char *title2, const char *title3, const char *buttons) {
  u8g2->setFont(FONT08);
  u8g2->setFontRefHeightAll();
  uint8_t out = u8g2->userInterfaceMessage(title1, title2, title3, buttons);
  u8g2->setFont(FONT10);
  u8g2->setFontRefHeightText();
  return out;
}

void restoreLastProjectorUsed() {
  lastProjectorUsed = projector.lastUsed();
  if (lastProjectorUsed > 8 || lastProjectorUsed == 0) {    // If EEPROM contains 00 or FF here, contents doesn't look legit
    projector.e2delete();
    lastProjectorUsed = EEPROM.read(1);
    gatherProjectorData();
    saveProjector(NEW);
  } else
    loadProjectorConfig(lastProjectorUsed);
}

void dimDisplay(bool activity) {
  static uint8_t c = 255;

  if (activity) {
    dimmingTimer.trigger(DISPLAY_DIM_AFTER);
    if (c < 255) {
      c = 255;
      u8g2->setContrast(c);
      breathe(false);
    }
  } else if (c > 0) {
    dimmingTimer.trigger((c > 1) ? 4ms : DISPLAY_CLEAR_AFTER);
    u8g2->setContrast(--c);
  } else {
    u8g2->clearDisplay();
    breathe(true);
  }
}

void breathe(bool doBreathe) {
  static PeriodicTimer tOn(TCK);
  static OneShotTimer  tOff(TCK);
  static int8_t ii = 0;

  if (doBreathe) {
    tOff.begin([] {
      digitalWriteFast(LED_BUILTIN, LOW);
    });
    tOn.begin([] {
      digitalWriteFast(LED_BUILTIN, HIGH);
      tOff.trigger(150*abs(++ii)+1);
    }, 50_Hz);
  } else {
    tOn.stop();
    tOff.stop();
    digitalWriteFast(LED_BUILTIN, LOW);
  }
}

bool audioConnected() {
  pinMode(TSH, OUTPUT);
  pinMode(RSH, INPUT);
  digitalWrite(TSH, HIGH);
  bool out = !digitalRead(RSH);
  digitalWrite(TSH, LOW);
  pinMode(TSH, INPUT_DISABLE);
  pinMode(RSH, INPUT_DISABLE);
  return out;
}
