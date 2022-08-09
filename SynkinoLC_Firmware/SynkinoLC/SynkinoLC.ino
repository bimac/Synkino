const char *uCVersion = "SynkinoLC v1.0 alpha";
char boardRevision[20] = "Hardware Revision X";   // will be corrected during init ofVS1053B

// hardware specific settings
#if defined(__MKL26Z64__)               // Teensy LC
  const char *uC = "Teensy LC";
#elif defined(__MK20DX256__)            // Teensy 3.1 / 3.2
  const char *uC = "Teensy 3.1 / 3.2";
#elif defined(ARDUINO_TEENSY40)         // Teensyt 4.0
  const char *uC = "Teensy 4.0";
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
#include "projector.h"    // management of the projector configuration & EEPROM storage
#include "ui.h"           // some UI methods of general use
#include "serialdebug.h"  // macros for serial debugging
#include "pins.h"         // pin definitions

#include "menus.h"        // menu definitions, positions of menu items
//#include "states.h"       // state definitions
//#include "timeconst.h"    // useful time constants and macros
#include "vars.h"         // initialization of constants and vars

// Initialize Objects
Audio musicPlayer;
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
uint8_t myState = MENU_MAIN;

void setup(void) {

  // Initialize serial interface (see serialdebug.h)
  #ifdef MYSERIAL
  MYSERIAL.begin(31250);
  #endif

  // Say hi
  PRINT("Welcome to ");
  PRINTLN(uCVersion);
  PRINTLN("");

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
  case MENU_MAIN:
    myState = u8g2->userInterfaceSelectionList("Main Menu", 2, main_menu) * 10;
    break;

  case MENU_PROJECTOR:
    myState += u8g2->userInterfaceSelectionList("Projector", 2, projector_action_menu);
    break;

  case MENU_PROJECTOR_NEW:
    projector.create();
    myState = MENU_MAIN;
    break;

  case MENU_PROJECTOR_SELECT:
    projector.load();
    myState = MENU_MAIN;
    break;

  case MENU_PROJECTOR_EDIT:
    projector.edit();
    myState = MENU_MAIN;
    break;

  case MENU_PROJECTOR_DELETE:
    projector.remove();
    myState = MENU_MAIN;
    break;

  case MENU_SELECT_TRACK:
    while (!musicPlayer.selectTrack()) {}
    myState = MENU_MAIN;
    detachInterrupt(STARTMARK);
    digitalWriteFast(LED_BUILTIN, LOW);
    break;

  case MENU_EXTRAS:
    myState += u8g2->userInterfaceSelectionList("Extras", 1, extras_menu);
    break;

  case MENU_EXTRAS_VERSION:
    ui.userInterfaceMessage(uCVersion, boardRevision, uC, " Nice! ");
    myState = MENU_MAIN;
    break;

  case MENU_EXTRAS_IMPULSE:
    attachInterrupt(IMPULSE, PULSE_ISR, CHANGE);
    ui.userInterfaceMessage("Test Impulse", "", "", "Done");
    detachInterrupt(IMPULSE);
    myState = MENU_MAIN;
    break;

  case MENU_EXTRAS_DEL_EEPROM:
    if (ui.userInterfaceMessage("Delete EEPROM", "Are you sure?", "", " Cancel \n Yes ") == 2)
      projector.e2delete();
    myState = MENU_MAIN;
    break;

#if defined(SERIALDEBUG) || defined(HWSERIALDEBUG)
  case MENU_EXTRAS_DUMP_EEPROM:
    projector.e2dump();
    myState = MENU_MAIN;
    break;
  #endif

  default:
    myState = MENU_MAIN;
  }
}

void dimDisplay(bool activity) {
  static uint8_t c = 255;
  if (activity) {                                             // wake up on activity
    dimmingTimer.trigger(DISPLAY_DIM_AFTER);
    if (c < 255) {
      c = 255;
      u8g2->setContrast(c);
      breathe(false);
    }
  } else if (c > 1) {                                         // smooth dimming
    dimmingTimer.trigger((c>3) ? 10ms : DISPLAY_CLEAR_AFTER);
    u8g2->setContrast(c-=2);
  } else if (myState % 10 == 0 && myState != 20) {            // blank screen & breathing LED (only within main menus)
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
