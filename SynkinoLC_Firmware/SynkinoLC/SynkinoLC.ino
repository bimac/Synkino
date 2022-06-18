#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include <EncoderTool.h>
#include <Bounce2.h>

#include "xbm.h"    // XBM graphics
#include "pins.h"   // pin definitions
#include "menus.h"  // menu definitions, positions of menu items
#include "states.h" // state definitions
#include "time.h"   // useful time constants and macros
#include "vars.h"   // initialization of constants and vars

// Initialize Objects
Adafruit_VS1053_FilePlayer musicPlayer(VS1053_RST, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARD_CS);
U8G2_SSD1306_128X64_NONAME_1_4W_HW_SPI u8g2(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
using namespace EncoderTool;
PolledEncoder enc;
Bounce button = Bounce();


void setup(void) {

  Serial.begin(115200);

  // set HW SPI pins (in case we go for alternative pins)
  SPI.setMOSI(SPI_MOSI);
  SPI.setMISO(SPI_MISO);
  SPI.setSCK(SPI_SCK);

  // set pin mode
  pinMode(BUZZER, OUTPUT);
  //pinMode(ENCODER_A, INPUT_PULLUP);
  //pinMode(ENCODER_B, INPUT_PULLUP);
  //pinMode(ENCODER_BTN, INPUT_PULLUP);

  u8g2.begin();
  u8g2.setFont(u8g2_font_helvR10_tr);
  u8g2.firstPage();
  do {
    u8g2.drawXBMP(logo_xbm_x, logo_xbm_y, logo_xbm_width, logo_xbm_height, logo_xbm_bits);
  } while (u8g2.nextPage());
  delay(2000);

  myState = MAIN_MENU;

  //restoreLastProjectorUsed();

  button.attach(ENCODER_BTN, INPUT_PULLUP);
  button.interval(25);

  enc.begin(ENCODER_A, ENCODER_B, CountMode::half, INPUT_PULLUP);
  enc.attachCallback([](int position, int delta) { lastActivityMillies = millis(); });

  musicPlayer.begin();
  musicPlayer.setVolume(20, 20);
  SD.begin(CARD_CS);
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);
  musicPlayer.startPlayingFile("/track001.mp3");
}

void loop(void) {
  switch (myState) {
    case MAIN_MENU:
      mainMenuSelection = u8g2.userInterfaceSelectionList("Main Menu", MENU_ITEM_SELECT_TRACK, main_menu);
      waitForBttnRelease();
  }
}

void waitForBttnRelease() {
  do
    button.update();
  while (!button.rose());
}

// This overwrites the weak function in u8x8_debounce.c
uint8_t u8x8_GetMenuEvent(u8x8_t *u8x8) {

  // Update encoder & bounce library
  button.update();
  enc.tick();

  int newEncPosition = enc.getValue();
  enc.setValue(0);

  if (newEncPosition < 0) {
    playClick();
    return U8X8_MSG_GPIO_MENU_UP;
  } else if (newEncPosition > 0) {
    playClick();
    return U8X8_MSG_GPIO_MENU_DOWN;
  } else if (button.fell()) {
    tone(BUZZER, 4000, 5);
    delay(50);
    return U8X8_MSG_GPIO_MENU_SELECT;
  } else {
    return 0;
  }
}

void playClick() {
  tone(BUZZER, 2200, 3);
}
