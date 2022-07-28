const char *uCVersion = "SynkinoLC v1.0 alpha";
char boardRevision[20] = "Hardware Revision X";
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

#include "TeensyTimerTool.h"
using namespace TeensyTimerTool;

#include <EncoderTool.h>
using namespace EncoderTool;

#include "audio.h"        // all things audio (derived from Adafruit_VS1053_FilePlayer)
#include "buzzer.h"       // the buzzer and some helper methods
#include "projector.h"
#include "ui.h"

#include "serialdebug.h"  // macros for serial debugging
#include "menus.h"        // menu definitions, positions of menu items
#include "pins.h"         // pin definitions
#include "states.h"       // state definitions
#include "timeconst.h"    // useful time constants and macros
#include "vars.h"         // initialization of constants and vars
#include "xbm.h"          // XBM graphics

// Initialize Objects
Audio musicPlayer(VS1053_RST, VS1053_CS, VS1053_DCS, VS1053_DREQ, VS1053_SDCS, VS1053_SDCD);
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
UI ui;

#define DISPLAY_DIM_AFTER   10s
#define DISPLAY_CLEAR_AFTER 5min

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
  ui.drawSplash();
  dimmingTimer.begin([] { dimDisplay(0); });
  dimmingTimer.trigger(DISPLAY_DIM_AFTER);

  // initialize encoder
  PRINTLN("Initializing encoder ...");
  enc.begin(ENC_A, ENC_B, ENC_BTN, CountMode::half, INPUT_PULLUP);        // Adjust CountMode if necessary
  encTimer.begin([]() { enc.tick(); }, 5_kHz);                            // Poll encoder at 5kHz
  enc.attachCallback([](int position, int delta) { dimDisplay(true); });  // Wake up display on encoder input
  enc.attachButtonCallback([](int state) { dimDisplay(true); });          // Wake up display on button input

  // initialize VS1053B breakout
  while (!musicPlayer.SDinserted())
    ui.showError("Please insert", "SD card");
  switch (musicPlayer.begin()) {
    case 1: ui.showError("Could not initialize", "SD card"); break;
    case 2: ui.showError("Could not initialize", "VS1053B Breakout"); break;
    case 3: ui.showError("Could not apply", "patches.053");
  }
  boardRevision[18] = musicPlayer.getRevision();

  projector.loadLast();

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
        projector.create();
        break;
      case MENU_ITEM_SELECT:
        projector.load();
        break;
      case MENU_ITEM_EDIT:
        projector.edit();
        break;
      case MENU_ITEM_DELETE:
        projector.remove();
      }
      break;

    case MENU_ITEM_SELECT_TRACK:
      while (!musicPlayer.selectTrack()) {}
      myState = MAIN_MENU;
      detachInterrupt(STARTMARK);
      digitalWriteFast(LED_BUILTIN, LOW);
      break;

    case MENU_ITEM_EXTRAS:

      // only offer e2dump() if SERIALDEBUG flag is set and we're actually connected to a serial monitor
      const char* myMenu = extras_menu;
      #ifdef SERIALDEBUG
      myMenu = (Serial) ? extras_menu_serial : extras_menu;
      #endif
      extrasMenuSelection = u8g2->userInterfaceSelectionList("Extras", MENU_ITEM_VERSION, myMenu);

      switch (extrasMenuSelection) {
      case MENU_ITEM_VERSION:
        ui.userInterfaceMessage(uCVersion, boardRevision, uC, " Nice! ");
        break;

      case MENU_ITEM_TEST_IMPULSE:
        attachInterrupt(IMPULSE, PULSE_ISR, CHANGE);
        ui.userInterfaceMessage("Test Impulse", "", "", "Done");
        detachInterrupt(IMPULSE);
        break;

      case MENU_ITEM_DEL_EEPROM:
        if (ui.userInterfaceMessage("Delete EEPROM", "Are you sure?", "", " Cancel \n Yes ") == 2)
          projector.e2delete();
        break;

      #ifdef SERIALDEBUG
      case MENU_ITEM_DUMP:
        if (myMenu==extras_menu_serial)
          projector.e2dump();
      #endif
      }
    }
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

  // ui.waitForBttnRelease();
  // oldPosition = 0;
  // myEnc.write(parentMenuEncPosition);
  // u8g2.setFont(u8g2_font_helvR10_tr);   // Only until we go to the PLAYING_MENU here
}


// void updateFpsDependencies(uint8_t fps) {
//   sollfps = fps;                                // redundant? sollfps is global, fps is local. Horrible.
//   pauseDetectedPeriod = (1000 / fps * 3);
//   sampleCountRegisterValid = true;           // It takes 8 KiB until the Ogg Sample Counter is valid for sure
//   impToSamplerateFactor = physicalSamplingrate / fps / shutterBlades / 2;
//   deltaToFramesDivider = physicalSamplingrate / fps;
//   impToAudioSecondsDivider = sollfps * shutterBlades * 2;
// }

void drawPlayingMenuConstants(int trackNo, byte fps) {
  u8g2->setFont(FONT08);
  u8g2->drawStr(0, 8, projector.config().name);
  char buffer[9] = "Film 000";
  ui.insertPaddedInt(&buffer[5], trackNo, 3);
  ui.drawRightAlignedStr(8, buffer);
  itoa(fps, buffer, 10);
  strcat(buffer, " fps");
  ui.drawRightAlignedStr(62, buffer);
  u8g2->setFont(FONT10);
}

void drawWaitForPlayingMenu(int trackNo, byte fps) {
  u8g2->clearBuffer();
  drawPlayingMenuConstants(trackNo, fps);
  ui.drawCenteredStr(28, "Waiting for");
  ui.drawCenteredStr(46, "Film to Start");
  u8g2->drawXBMP(60, 54, pause_xbm_width, pause_xbm_height, pause_xbm_bits);
  u8g2->sendBuffer();
}

void drawPlayingMenu(int trackNo, byte fps) {
  u8g2->clearBuffer();
  drawPlayingMenuConstants(trackNo, fps);
  unsigned long currentmillis = millis();
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
  } else if (myState == MAIN_MENU) {
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

// This overwrites the weak function in u8x8_debounce.c
uint8_t u8x8_GetMenuEvent(u8x8_t *u8x8) {
    yield();

    if (enc.valueChanged()) {
      buzzer.playClick();                         // feedback sound
      int encVal = ui.encDir() * enc.getValue();  // get encoder value
      enc.setValue(0);                            // reset encoder

      if (encVal < 0)
        return U8X8_MSG_GPIO_MENU_UP;
      else
        return U8X8_MSG_GPIO_MENU_DOWN;

    } if (!enc.getButton()) {
      buzzer.playPress();                   // feedback sound
      while (!enc.getButton())              // wait for release
        yield();
      return U8X8_MSG_GPIO_MENU_SELECT;

    } else
      return 0;
}
