const char *uCVersion = "SynkinoLC v1.0\n";

#include <Arduino.h>
#include <Adafruit_VS1053.h>
#include <EncoderTool.h>
#include <SD.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <EEPROM.h>

// Add patches.053 to flash? Not possible with TeensyLC.
#ifdef INC_PATCHES
  #include <incbin.h>
  INCBIN(Patch, "patches.053");
#endif

#include "menus.h"  // menu definitions, positions of menu items
#include "pins.h"   // pin definitions
#include "states.h" // state definitions
#include "time.h"   // useful time constants and macros
#include "vars.h"   // initialization of constants and vars
#include "xbm.h"    // XBM graphics
#include "notes.h"  // buzzer frequencies

// Initialize Objects
Adafruit_VS1053_FilePlayer musicPlayer(VS1053_RST, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARD_CS);
//U8G2_SSD1306_128X64_NONAME_1_4W_HW_SPI u8g2(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
U8G2_SH1106_128X64_NONAME_1_4W_HW_SPI u8g2(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
using namespace EncoderTool;
PolledEncoder enc;
IntervalTimer encTimer;
IntervalTimer dimmingTimer;

#define DISPLAY_DIM_AFTER   5000
#define DISPLAY_BLANK_AFTER 600000

#define FONT10 u8g2_font_helvR10_tr
#define FONT08 u8g2_font_helvR08_tr

void setup(void) {

  Serial.begin(9600);
  Serial.printf("Welcome to %s\n",uCVersion);

  // set HW SPI pins (in case we go for alternative pins)
  SPI.setMOSI(SPI_MOSI);
  SPI.setMISO(SPI_MISO);
  SPI.setSCK(SPI_SCK);

  // set pin mode
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(CARD_DET, INPUT_PULLUP);

  // initialize u8g2 / display boot splash
  u8g2.begin();
  u8g2.firstPage();
  do {
    u8g2.drawXBMP(logo_xbm_x, logo_xbm_y, logo_xbm_width, logo_xbm_height, logo_xbm_bits);
  } while ( u8g2.nextPage() );
  delay(500);
  u8g2.firstPage();
  do {
    u8g2.drawXBMP(logo_xbm_x, logo_xbm_y, logo_xbm_width, logo_xbm_height, logo_xbm_bits);
    u8g2.drawXBMP(logolc_xbm_x, logolc_xbm_y, logolc_xbm_width, logolc_xbm_height, logolc_xbm_bits);
    u8g2.drawXBMP(ifma_xbm_x, ifma_xbm_y, ifma_xbm_width, ifma_xbm_height, ifma_xbm_bits);
  } while ( u8g2.nextPage() );

  u8g2.sendBuffer();
  delay(2000);
  u8g2.setFont(FONT10);

  // initialize encoder
  Serial.println("Initializing encoder ...");
  enc.begin(ENCODER_A, ENCODER_B, ENCODER_BTN, CountMode::half, INPUT_PULLUP);
  enc.attachCallback([](int position, int delta) { lastActivityMillies = millis(); });
  enc.attachButtonCallback([](int state) { lastActivityMillies = millis(); });
  encTimer.begin([]() { enc.tick(); }, 200);

  // initialize SD
  Serial.println("Initializing SD card ...");
  while (!digitalRead(CARD_DET))
    showError("ERROR", "Please insert", "SD card");
  if (!SD.begin(CARD_CS))
    showError("ERROR", "Could not initialize", "SD card");

  // initialize VS1053
  Serial.println("Initializing VS1053 ...");
  if (!musicPlayer.begin())
    showError("ERROR", "Could not initialize", "VS1053B Breakout");
  if (!patchVS1053())
    showError("ERROR", "Could not apply", "patches.053");
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);
  musicPlayer.setVolume(20, 20);
  musicPlayer.startPlayingFile("/track001.mp3");

  // initialize timer for dimming the display
  dimmingTimer.begin(dimDisplay, 1000000);

  myState = MAIN_MENU;
  restoreLastProjectorUsed();

  // Say "Hello!"
  playHello();
  Serial.println("Startup complete.\n");
}

void loop(void) {
  switch (myState) {
  case MAIN_MENU:

    mainMenuSelection = u8g2.userInterfaceSelectionList("Main Menu", MENU_ITEM_SELECT_TRACK, main_menu);
    switch (mainMenuSelection) {
    case MENU_ITEM_PROJECTOR:

      projectorActionMenuSelection = u8g2.userInterfaceSelectionList("Projector", MENU_ITEM_SELECT, projector_action_menu);
      switch (projectorActionMenuSelection) {
      case MENU_ITEM_NEW:
        byte projectorCount;
        projectorCount = EEPROM.read(0);
        if (projectorCount < 8) {
          gatherProjectorData();
          saveProjector(NEW);
        } else
          showError("ERROR", "Too many Projectors.","Only 8 allowed.");
        break;
      case MENU_ITEM_SELECT:
        makeProjectorSelectionMenu();
        projectorSelectionMenuSelection = u8g2.userInterfaceSelectionList("Select Projector", lastProjectorUsed, projectorSelection_menu);
        loadProjectorConfig(projectorSelectionMenuSelection);
        break;
      case MENU_ITEM_EDIT:
        makeProjectorSelectionMenu();
        projectorSelectionMenuSelection = u8g2.userInterfaceSelectionList("Edit Projector", lastProjectorUsed, projectorSelection_menu);
        loadProjectorConfig(projectorSelectionMenuSelection);
        gatherProjectorData();
        saveProjector(projectorSelectionMenuSelection);
        break;
      case MENU_ITEM_DELETE:
        byte currentProjectorCount = makeProjectorSelectionMenu();
        projectorSelectionMenuSelection = u8g2.userInterfaceSelectionList("Delete Projector", currentProjectorCount, projectorSelection_menu);
        deleteProjector(projectorSelectionMenuSelection);
      }
      break;

    case MENU_ITEM_SELECT_TRACK:
      myState = SELECT_TRACK;
      break;

    case MENU_ITEM_EXTRAS:
      extrasMenuSelection = u8g2.userInterfaceSelectionList("Extras", MENU_ITEM_DEL_EEPROM, extras_menu);
      switch (extrasMenuSelection) {
      case MENU_ITEM_DEL_EEPROM:
        if (userInterfaceMessage("Delete EEPROM", "Are you sure?", "", " Cancel \n Yes ") == 2) {
          for (int i = 0; i < EEPROM.length(); i++)
            EEPROM.write(i, 0);
        }
        break;
      case MENU_ITEM_DUMP:
        e2reader();
        break;
      case MENU_ITEM_VERSION:
        showError("About Synkino", uCVersion, "");
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
    break;
  case SELECT_TRACK:
    static int trackChosen;
    trackChosen = selectTrackScreen();
    if (trackChosen != 0) {
      //tellAudioPlayer(CMD_LOAD_TRACK, trackChosen);
      myState = MAIN_MENU;//WAIT_FOR_LOADING;
    } else {
      myState = MAIN_MENU;
    }
    break;
  case WAIT_FOR_LOADING:
    // drawBusyBee(90, 10);
    // if ((fps != 0) && (trackLoaded != 0)) {
    //   drawWaitForPlayingMenu(trackChosen, fps);
    //   myState = TRACK_LOADED;
    //   delay(300); // avoid hearing first bytes in Audio FIFO
    //   digitalWrite(AUDIO_EN, HIGH);
    // } // Todo: Timeout Handler?
    break;
  case TRACK_LOADED:
    // if (startMarkHit != 0) {
    //   myState = SYNC_PLAY;
    // }
    // // Now check for Sync Pos Adjustments
    // if (digitalRead(ENCODER_BTN) == 0) {
    //   waitForBttnRelease();
    //   trackLoadedMenuSelection = u8g2.userInterfaceSelectionList("Playback", MENU_ITEM_EXIT, trackLoaded_menu);
    //   waitForBttnRelease();
    //   switch (trackLoadedMenuSelection) {
    //   case MENU_ITEM_MANUALSTART:
    //     startMarkHit = 1;
    //     tellAudioPlayer(CMD_SET_STARTMARK, 0);
    //     myState = SYNC_PLAY;
    //     break;
    //   case MENU_ITEM_STOP:
    //     tellAudioPlayer(CMD_RESET, 0);
    //     myState = MAIN_MENU;
    //     break;
    //   case MENU_ITEM_EXIT:
    //     drawWaitForPlayingMenu(trackChosen, fps);
    //     myState = TRACK_LOADED;
    //     break;
    //   default:
    //     break;
    //   }
    // }
    break;
  case SYNC_PLAY:
    // drawPlayingMenu(trackChosen, fps);
    // if (digitalRead(ENCODER_BTN) == 0) {
    //   waitForBttnRelease();
    //   handleFrameCorrectionOffsetInput(); // Take Correction Input
    //   waitForBttnRelease();
    //   tellAudioPlayer(CMD_SYNC_OFFSET, newSyncOffset);
    // }
    break;
  default:
    break;
  }
}

void deleteProjector(byte thisProjector) {
  Projector aProjector;
  EEPROM.get((projectorSelectionMenuSelection - 1) * sizeof(aProjector) + 2, aProjector);
  char name[maxProjectorNameLength + 2] = "\"";
  strcat(name, aProjector.name);
  strcat(name, "\"");
  if (userInterfaceMessage("Delete projector", name, "Are you sure?", " Cancel \n Yes ") == 1)
    return;

  Serial.printf("Deleting projector %s.",name);
  byte projectorCount;
  projectorCount = EEPROM.read(0);
  if (thisProjector != projectorCount) {
    for (byte i = thisProjector; i < projectorCount; i++) {
      EEPROM.get(i * sizeof(aProjector) + 2, aProjector);
      EEPROM.put((i - 1) * sizeof(aProjector) + 2, aProjector);
      e2reader();
    }
  }
  EEPROM.write(0, projectorCount - 1);
  e2reader();

  if (thisProjector == lastProjectorUsed)
    loadProjectorConfig(1);
}

void saveProjector(byte thisProjector) {
  byte projectorCount;
  projectorCount = EEPROM.read(0);

  Projector aProjector;
  aProjector.shutterBladeCount = shutterBladesMenuSelection;
  aProjector.startmarkOffset = newStartmarkOffset;
  aProjector.p = new_p;
  aProjector.i = new_i;
  aProjector.d = new_d;
  strcpy(aProjector.name, newProjectorName);

  if (thisProjector == NEW) { // We have a NEW projector here
    EEPROM.write(0, projectorCount + 1);
    EEPROM.write(1, projectorCount + 1);
    aProjector.index = projectorCount + 1;
    EEPROM.put((projectorCount * sizeof(aProjector) + 2), aProjector);
    e2reader();

  } else {
    aProjector.index = thisProjector;
    EEPROM.write(1, thisProjector);
    EEPROM.put(((thisProjector - 1) * sizeof(aProjector) + 2), aProjector);
    e2reader();

    // tellAudioPlayer(CMD_SET_SHUTTERBLADES, aProjector.shutterBladeCount);
    // tellAudioPlayer(CMD_SET_STARTMARK, aProjector.startmarkOffset);
    // tellAudioPlayer(CMD_SET_P, aProjector.p);
    // tellAudioPlayer(CMD_SET_I, aProjector.i);
    // tellAudioPlayer(CMD_SET_D, aProjector.d);
  }
}

void gatherProjectorData() {
  handleProjectorNameInput();
  shutterBladesMenuSelection = u8g2.userInterfaceSelectionList("# Shutter Blades", shutterBladesMenuSelection, shutterblade_menu);
  u8g2.userInterfaceInputValue("Start Mark Offset:", "", &newStartmarkOffset, 1, 255, 3, " Frames");
  u8g2.userInterfaceInputValue("Proportional:", "", &new_p, 0, 99, 2, "");
  u8g2.userInterfaceInputValue("Integral:",     "", &new_i, 0, 99, 2, "");
  u8g2.userInterfaceInputValue("Derivative:",   "", &new_d, 0, 99, 2, "");
}

void loadProjectorConfig(uint8_t projNo) {
  Projector aProjector;
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
  // e2reader();
}

byte makeProjectorSelectionMenu() {
  projectorSelection_menu[0] = 0; // Empty this out and be prepared for an empty list
  byte projectorCount;
  projectorCount = EEPROM.read(0);
  lastProjectorUsed = EEPROM.read(1);

  Projector aProjector;
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
  while (!enc.getButton()) {}
}

uint16_t selectTrackScreen() {
  static uint16_t trackNo = 1;
  enc.setValue(1);
  enc.setLimits(0,999);
  while (enc.getButton()) {
    if (enc.valueChanged()) {
      trackNo = enc.getValue();
      playClick();
      u8g2.firstPage();
      if (trackNo == 0) {
        do {
          u8g2.setFont(FONT10);
          u8g2.drawStr(18,35,"< Main Menu");
        } while ( u8g2.nextPage() );
      } else {
        do {
          u8g2.setFont(u8g2_font_inb46_mn);
          u8g2.setCursor(9, 55);
          if (trackNo <  10) u8g2.print("0");
          if (trackNo < 100) u8g2.print("0");
          u8g2.print(trackNo);
        } while ( u8g2.nextPage() );
      }
    }
  }
  waitForBttnRelease();
  enc.setLimits(-999,999);
  u8g2.setFont(FONT10);
  return trackNo;
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
    if (strlen(newProjectorName) == maxProjectorNameLength) {
      const char lastChar = newProjectorName[maxProjectorNameLength - 1];
      newProjectorName[maxProjectorNameLength - 1] = '\0';
      enc.setValue(lastChar);
    } else {
      enc.setValue(127); // to start with "Delete"
    }
    charIndex = strlen(newProjectorName);
    firstUse = false;
    for (byte i = charIndex + 1; i < maxProjectorNameLength; i++) {
      newProjectorName[i] = 0;
    }
  } else {
    for (byte i = 0; i < maxProjectorNameLength; i++) {
      newProjectorName[i] = 0;
    }
    enc.setValue('A');
  }
  oldEncPosition = enc.getValue();

  while (charIndex < maxProjectorNameLength && !inputFinished) {

    while (enc.getButton()) {
      newEncPosition = enc.getValue();

      if (newEncPosition != oldEncPosition) {
        // Correct encoder value if inbetween Ascii blocks
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

        playClick();
        oldEncPosition = newEncPosition;
        stopCursorBlinking = true;
        tonePlayedYet = false;
      }

      newEncPosition = newEncPosition;

      lastMillis = millis();
      handleStringInputGraphically(GET_NAME, newEncPosition, lastMillis, firstUse, stopCursorBlinking);
      stopCursorBlinking = false;
    }

    lastMillis = millis();
    while (!enc.getButton() && !inputFinished) {
      if (!tonePlayedYet) {
        tone(BUZZER, 4000, 5);
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
      Serial.println(charIndex);
      newProjectorName[charIndex] = 0;

    } else if (!inputFinished) {
      newProjectorName[charIndex] = newEncPosition;
      charIndex++;
      if (firstUse) enc.setValue('a');
      firstUse = false;
    }
  }
  while (digitalRead(ENCODER_BTN) == 0) {}
  //inputFinished = false;
  newProjectorName[charIndex] = '\0';
}

bool handleStringInputGraphically(byte action, char localChar, unsigned long lastMillis, bool firstUse, bool charChanged) {
  u8g2.firstPage();
  do {
    u8g2.setFont(FONT10);
    u8g2.setCursor(15, 35);
    u8g2.print(newProjectorName);

    if (action == GET_NAME) {
      if ((lastMillis % 600 < 400) || charChanged) {
        if (localChar == 32)
          u8g2.print("_");
        else if (localChar == 127) { // Delete
          u8g2.setFont(u8g2_font_m2icon_9_tf);
          u8g2.print("a"); // https://github.com/olikraus/u8g2/wiki/fntpic/u8g2_font_m2icon_9_tf.png
          u8g2.setFont(FONT10);
        } else {
          u8g2.print(localChar);
        }
      }

      u8g2.setFont(FONT08);
      if (localChar == 32) {
        u8g2.drawStr(45, 55, "[Space]");
      } else if (localChar == 127) {
        u8g2.drawStr(34, 55, "[Delete last]");
      } else {
        if (firstUse) {
          u8g2.drawStr(16, 55, "[Turn and push Knob]");
        } else {
          u8g2.drawStr(14, 55, "[Long Press to Finish]");
        }
      }
    }

    else if (action == JUST_PRESSED) {
      u8g2.setFont(FONT08);
      u8g2.drawStr(11, 55, "[Keep pressed to Save]");
      if (millis() - lastMillis > 1500) {
        playConfirm();
        return true;
      }
    }

    else if (action == LONG_PRESSED) {
      u8g2.setFont(FONT08);
      u8g2.drawStr(41, 55, "[Saved!]");
    }
    u8g2.drawStr(19, 14, "Set Projector Name");
    u8g2.setFont(FONT10);
  } while (u8g2.nextPage());

  return false;
}

// This overwrites the weak function in u8x8_debounce.c
uint8_t u8x8_GetMenuEvent(u8x8_t *u8x8) {
    int encVal = enc.getValue();      // get encoder value
    enc.setValue(0);                  // reset encoder

    if (encVal < 0) {
      dimDisplay();                   // wake up display
      playClick();
      return U8X8_MSG_GPIO_MENU_UP;

    } else if (encVal > 0) {
      dimDisplay();                   // wake up display
      playClick();
      return U8X8_MSG_GPIO_MENU_DOWN;

    } else if (!enc.getButton()) {
      dimDisplay();                   // wake up display
      tone(BUZZER, 4000, 5);
      while (!enc.getButton()) {}
      return U8X8_MSG_GPIO_MENU_SELECT;

    } else
      return 0;
}

void e2reader() {
  char buffer[16];
  char valuePrint[4];
  byte value;
  unsigned int address;
  uint8_t trailingSpace = 2;

  for (address = 0; address <= 127; address++) {
    // read a byte from the current address of the EEPROM
    value = EEPROM.read(address);

  for (uint8_t address = 0; address <= 127; address += 16){
    EEPROM.get(address, buffer);                                      // get data from EEPROM
    Serial.printf("\n 0x%05X:  ", address);                           // print address
    for (uint8_t i = 1; i <= 16; i++)
      Serial.printf((i % 8 > 0) ? "%02X " : "%02X   ", buffer[i-1]);  // print HEX values
    printASCII(buffer);
  }
  for (int i = trailingSpace; i > 0; i--)

  Serial.println("");                                                 // final new line
}

void printASCII(char *buffer) {
  for (int i = 0; i < 16; i++) {
    if (i == 8)
      Serial.print(" ");
    if (buffer[i] > 31 and buffer[i] < 127)
      Serial.print(buffer[i]);
    else
      Serial.print(F("."));
  }
}

void showError(const char* errorHeader, const char* errorMsg1, const char* errorMsg2) {
  if (strcmp(errorHeader, "ERROR")==0) {
    tone(BUZZER, NOTE_G2, 250);
    delay(250);
    tone(BUZZER, NOTE_C2, 500);
    delay(500);
  }
  userInterfaceMessage(errorHeader, errorMsg1, errorMsg2, " Okay ");
}

void restoreLastProjectorUsed() {
  lastProjectorUsed = EEPROM.read(1);
  if (lastProjectorUsed > 8 || lastProjectorUsed == 0) {    // If EEPROM contains 00 or FF here, contents doesn't look legit
    for (int i = 0; i < EEPROM.length(); i++)
      EEPROM.write(i, 0);
    lastProjectorUsed = EEPROM.read(1);
    gatherProjectorData();
    saveProjector(NEW);
  } else
    loadProjectorConfig(lastProjectorUsed);
}

void playClick() {
  tone(BUZZER, 2200, 3);
}

void playConfirm() {
  tone(BUZZER, NOTE_C7, 50);
  delay(50);
  tone(BUZZER, NOTE_G7, 50);
  delay(50);
  tone(BUZZER, NOTE_C8, 50);
}

void playHello() {
  tone(BUZZER, NOTE_C7, 25);
  delay(25);
  tone(BUZZER, NOTE_D7, 25);
  delay(25);
  tone(BUZZER, NOTE_E7, 25);
  delay(25);
  tone(BUZZER, NOTE_F7, 25);
}

bool patchVS1053() {
  if (SD.exists("/patches.053")) {
    // If patches.053 is found on SD we'll always try to load it from there ...
    File file = SD.open("/patches.053", O_READ);
    uint16_t size = file.size();
    uint8_t patch[size];
    Serial.printf("Applying \"patches.053\" (%d bytes) from SD card ... ", size);
    bool success = file.read(&patch, size) == size;
    file.close();
    if (success) {
      musicPlayer.applyPatch(reinterpret_cast<uint16_t*>(patch), size/2);
      Serial.println("done");
      return success;
    } else
      Serial.println("error reading file.");
  }
  else {
    // ... if not, we'll load it from flash memory
    #ifdef INC_PATCHES
    Serial.printf("Applying \"patches.053\" (%d bytes) from flash ... ", gPatchSize);
    musicPlayer.applyPatch(reinterpret_cast<const uint16_t*>(gPatchData), gPatchSize/2);
    Serial.printf("done\n\n");
    return true;
    #endif
  }
  return false;
}

uint8_t userInterfaceMessage(const char *title1, const char *title2, const char *title3, const char *buttons) {
  u8g2.setFont(FONT08);
  u8g2.setFontRefHeightAll();
  uint8_t out = u8g2.userInterfaceMessage(title1, title2, title3, buttons);
  u8g2.setFont(FONT10);
  u8g2.setFontRefHeightText();
  return out;
}

void dimDisplay() {
  static uint8_t state = 99;
  if (state == 0 && millis() - lastActivityMillies > DISPLAY_DIM_AFTER) {
    u8g2.setContrast(1);
    state = 1;
    return;
  } else if (state == 1 && millis() - lastActivityMillies > DISPLAY_BLANK_AFTER) {
    u8g2.clearDisplay();
    state = 2;
    digitalWriteFast(LED_BUILTIN, HIGH);
    return;
  } else if (state > 0 && millis() - lastActivityMillies <= DISPLAY_DIM_AFTER) {
    u8g2.setContrast(255);
    digitalWriteFast(LED_BUILTIN, LOW);
    state = 0;
  }
}
