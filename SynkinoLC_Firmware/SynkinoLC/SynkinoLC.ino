const char *uCVersion = "SynkinoLC v1.0\n";

#include <Adafruit_VS1053.h>
#include <Arduino.h>
#include <Bounce2.h>
#include <EncoderTool.h>
#include <SD.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <EEPROM.h>

#include "menus.h"  // menu definitions, positions of menu items
#include "pins.h"   // pin definitions
#include "states.h" // state definitions
#include "time.h"   // useful time constants and macros
#include "vars.h"   // initialization of constants and vars
#include "xbm.h"    // XBM graphics

// Initialize Objects
Adafruit_VS1053_FilePlayer musicPlayer(VS1053_RST, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARD_CS);
U8G2_SSD1306_128X64_NONAME_1_4W_HW_SPI u8g2(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
using namespace EncoderTool;
Encoder enc;
Bounce2::Button button = Bounce2::Button();

void setup(void) {

  Serial.begin(115200);

  // set HW SPI pins (in case we go for alternative pins)
  SPI.setMOSI(SPI_MOSI);
  SPI.setMISO(SPI_MISO);
  SPI.setSCK(SPI_SCK);

  // set pin mode
  pinMode(BUZZER, OUTPUT);
  // pinMode(ENCODER_A, INPUT_PULLUP);
  // pinMode(ENCODER_B, INPUT_PULLUP);
  // pinMode(ENCODER_BTN, INPUT_PULLUP);

  u8g2.begin();
  u8g2.setFont(u8g2_font_helvR10_tr);
  u8g2.firstPage();
  do {
    u8g2.drawXBMP(logo_xbm_x, logo_xbm_y, logo_xbm_width, logo_xbm_height, logo_xbm_bits);
  } while (u8g2.nextPage());
  delay(2000);

  myState = MAIN_MENU;

  // restoreLastProjectorUsed();

  button.attach(ENCODER_BTN, INPUT_PULLUP);
  button.setPressedState(LOW);
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

    switch (mainMenuSelection) {
    case MENU_ITEM_PROJECTOR:

      projectorActionMenuSelection = u8g2.userInterfaceSelectionList("Projector", MENU_ITEM_SELECT, projector_action_menu);
      waitForBttnRelease();

      if (projectorActionMenuSelection == MENU_ITEM_NEW) {
        byte projectorCount;
        projectorCount = EEPROM.read(0);
        Serial.print("P count ");
        Serial.println(projectorCount);
        if (projectorCount < 8) {
          gatherProjectorData();
          saveProjector(NEW);
        } else {
          showError("ERROR", "Too many Projectors.","Only 8 allowed.");
        }
      } else if (projectorActionMenuSelection == MENU_ITEM_SELECT) {
        makeProjectorSelectionMenu();
        projectorSelectionMenuSelection = u8g2.userInterfaceSelectionList("Select Projector", lastProjectorUsed, projectorSelection_menu);
        waitForBttnRelease();
        loadProjectorConfig(projectorSelectionMenuSelection);
      } else if (projectorActionMenuSelection == MENU_ITEM_EDIT) {
        makeProjectorSelectionMenu();
        projectorSelectionMenuSelection = u8g2.userInterfaceSelectionList("Edit Projector", lastProjectorUsed, projectorSelection_menu);
        waitForBttnRelease();
        loadProjectorConfig(projectorSelectionMenuSelection);
        gatherProjectorData();
        saveProjector(projectorSelectionMenuSelection);
      } else if (projectorActionMenuSelection == MENU_ITEM_DELETE) {
        byte currentProjectorCount = makeProjectorSelectionMenu();
        projectorSelectionMenuSelection = u8g2.userInterfaceSelectionList("Delete Projector", currentProjectorCount, projectorSelection_menu);
        waitForBttnRelease();
        Serial.print("DelMenu-Ausw: ");
        Serial.println(projectorSelectionMenuSelection);
        deleteProjector(projectorSelectionMenuSelection);
      } else if (projectorActionMenuSelection == MENU_ITEM_EXIT) {
        // nothing to do here
      }
      break;
    case MENU_ITEM_SELECT_TRACK:
      myState = SELECT_TRACK;
      break;
    case MENU_ITEM_POWER_OFF:
      // shutdownSelf();
      break;
    case MENU_ITEM_EXTRAS:
      extrasMenuSelection = u8g2.userInterfaceSelectionList("Extras", MENU_ITEM_DEL_EEPROM, extras_menu);
      waitForBttnRelease();
      switch (extrasMenuSelection) {
      case MENU_ITEM_DEL_EEPROM:
        u8g2.setFont(u8g2_font_helvR08_tr);
        u8g2.setFontRefHeightAll(); /* this will add some extra space for the text inside the buttons */
        byte choice;
        choice = u8g2.userInterfaceMessage("Delete EEPROM", "Are you sure?", "", " Cancel \n Yes ");
        waitForBttnRelease();
        if (choice == 2) {
          for (int i = 0; i < EEPROM.length(); i++) {
            EEPROM.write(i, 0);
          }
        }
        u8g2.setFont(u8g2_font_helvR10_tr);
        u8g2.setFontRefHeightText();
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
    // static int trackChosen;
    // trackChosen = selectTrackScreen();
    // if (trackChosen != 0) {
    //   tellAudioPlayer(CMD_LOAD_TRACK, trackChosen);
    //   myState = WAIT_FOR_LOADING;
    // } else {
      myState = MAIN_MENU;
    // }
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
  byte projectorCount;
  projectorCount = EEPROM.read(0);
  Projector aProjector;
  if (thisProjector == projectorCount) {
    Serial.print("Zeroing out (in theory): #");
    Serial.println(thisProjector);
  } else {
    for (byte i = thisProjector; i < projectorCount; i++) {
      EEPROM.get(i * sizeof(aProjector) + 2, aProjector);
      EEPROM.put((i - 1) * sizeof(aProjector) + 2, aProjector);
      e2reader();
    }
  }
  EEPROM.write(0, projectorCount - 1);
  e2reader();

  //  Serial.println(lastProjectorUsed);
  if (thisProjector == lastProjectorUsed) {
    loadProjectorConfig(1); // config 1
                            //    Serial.println("Switching active Projector to #1.");
  }
}

void saveProjector(byte thisProjector) {
  byte projectorCount;
  projectorCount = EEPROM.read(0);
  //  Serial.println(projectorCount);

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
  int prevEncPos = enc.getValue();
  handleProjectorNameInput();
  enc.setValue(prevEncPos);
  handleShutterbladeInput();
  handleStartmarkInput();
  handlePIDinput();
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
  do
    button.update();
  while (button.isPressed());
}

void handleProjectorNameInput() {
  char localChar;
  unsigned long newEncPosition;
  unsigned long oldEncPosition;
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

    while (!button.isPressed()) {
      button.update();
      //enc.tick();

      newEncPosition = enc.getValue();

      if (newEncPosition == oldEncPosition) {} // (do nothing)
      else {
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

      localChar = newEncPosition;

      lastMillis = millis();
      handleStringInputGraphically(GET_NAME, localChar, lastMillis, firstUse, stopCursorBlinking);
      stopCursorBlinking = false;
    }

    lastMillis = millis();
    while (button.isPressed() && !inputFinished) {
      button.update();
      if (!tonePlayedYet) {
        tone(BUZZER, 4000, 5);
        tonePlayedYet = true;
      }
      delay(50);
      inputFinished = handleStringInputGraphically(JUST_PRESSED, localChar, lastMillis, firstUse, false);
    }
    while (button.isPressed() && inputFinished) {
      button.update();
      handleStringInputGraphically(LONG_PRESSED, localChar, 0, firstUse, false);
    }

    if (localChar == 127 && !inputFinished) {   // Delete
      if (charIndex > 0) {
        charIndex--;
      }
      Serial.println(charIndex);
      newProjectorName[charIndex] = 0;

    } else if (!inputFinished) {
      newProjectorName[charIndex] = localChar;
      charIndex++;
      if (firstUse) enc.setValue('a');
      firstUse = false;
    }
  }
  while (digitalRead(ENCODER_BTN) == 0) {}
  //inputFinished = false;
  newProjectorName[charIndex] = '\0';
}

void handleShutterbladeInput() {
  // myEnc.write(16000);
  shutterBladesMenuSelection = u8g2.userInterfaceSelectionList("# Shutter Blades", shutterBladesMenuSelection, shutterblade_menu);
  waitForBttnRelease();
}

void handleStartmarkInput() {
  u8g2.userInterfaceInputValue("Start Mark Offset:", "", &newStartmarkOffset, 1, 255, 3, " Frames");
  waitForBttnRelease();
}

void handlePIDinput() {
  u8g2.userInterfaceInputValue("Proportional:", "", &new_p, 0, 99, 2, "");
  waitForBttnRelease();
  u8g2.userInterfaceInputValue("Integral:", "", &new_i, 0, 99, 2, "");
  waitForBttnRelease();
  u8g2.userInterfaceInputValue("Derivative:", "", &new_d, 0, 99, 2, "");
  waitForBttnRelease();
}

bool handleStringInputGraphically(byte action, char localChar, unsigned long lastMillis, bool firstUse, bool charChanged) {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_helvR10_tr);
    u8g2.setCursor(15, 35);
    u8g2.print(newProjectorName);

    if (action == GET_NAME) {
      if ((lastMillis % 600 < 400) || charChanged) {
        if (localChar == 32)
          u8g2.print("_");
        else if (localChar == 127) { // Delete
          u8g2.setFont(u8g2_font_m2icon_9_tf);
          u8g2.print("a"); // https://github.com/olikraus/u8g2/wiki/fntpic/u8g2_font_m2icon_9_tf.png
          u8g2.setFont(u8g2_font_helvR10_tr);
        } else {
          u8g2.print(localChar);
        }
      }

      u8g2.setFont(u8g2_font_helvR08_tr);
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
      u8g2.setFont(u8g2_font_helvR08_tr);
      u8g2.drawStr(11, 55, "[Keep pressed to Save]");
      if (millis() - lastMillis > 1500) {
        playConfirm();
        return true;
      }
    }

    else if (action == LONG_PRESSED) {
      u8g2.setFont(u8g2_font_helvR08_tr);
      u8g2.drawStr(41, 55, "[Saved!]");
    }
    u8g2.drawStr(19, 14, "Set Projector Name");
    u8g2.setFont(u8g2_font_helvR10_tr);
  } while (u8g2.nextPage());

  return false;
}

// This overwrites the weak function in u8x8_debounce.c
uint8_t u8x8_GetMenuEvent(u8x8_t *u8x8) {

  // Update encoder & bounce library
  button.update();
  //enc.tick();

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

void e2reader() {
  char buffer[16];
  char valuePrint[4];
  byte value;
  unsigned int address;
  uint8_t trailingSpace = 2;

  for (address = 0; address <= 127; address++) {
    // read a byte from the current address of the EEPROM
    value = EEPROM.read(address);

    // add space between two sets of 8 bytes
    if (address % 8 == 0)
      Serial.print(F("  "));

    // newline and address for every 16 bytes
    if (address % 16 == 0) {
      // print the buffer
      if (address > 0 && address % 16 == 0)
        printASCII(buffer);

      sprintf(buffer, "\n 0x%05X: ", address);
      Serial.print(buffer);

      // clear the buffer for the next data block
      memset(buffer, 32, 16);
    }

    // save the value in temporary storage
    buffer[address % 16] = value;

    // print the formatted value
    sprintf(valuePrint, " %02X", value);
    Serial.print(valuePrint);
  }

  if (address % 16 > 0) {
    if (address % 16 < 9)
      trailingSpace += 2;

    trailingSpace += (16 - address % 16) * 3;
  }

  for (int i = trailingSpace; i > 0; i--)
    Serial.print(F(" "));

  // last line of data and a new line
  printASCII(buffer);
  Serial.println();
}

void printASCII(char * buffer) {
  for(int i = 0; i < 16; i++){
    if(i == 8)
      Serial.print(" ");

    if(buffer[i] > 31 and buffer[i] < 127){
      Serial.print(buffer[i]);
    }else{
      Serial.print(F("."));
    }
  }
}

void showError(char *errorHeader, char *errorMsg1, char *errorMsg2) {
  u8g2.setFont(u8g2_font_helvR08_tr);
  u8g2.setFontRefHeightAll(); // this will add some extra space for the text inside the buttons
  u8g2.userInterfaceMessage(errorHeader, errorMsg1, errorMsg2, " Okay ");
  waitForBttnRelease();
  u8g2.setFont(u8g2_font_helvR10_tr);
  u8g2.setFontRefHeightText();
}

void playClick() {
  tone(BUZZER, 2200, 3);
}

void playConfirm() {
  tone(BUZZER, 2093,  50); // C7
  delay(50);
  tone(BUZZER, 3136,  50); // G7
  delay(50);
  tone(BUZZER, 4186,  50); // C8
}

void playGoodBye() {
  delay(250);
  tone(BUZZER, 4186, 200); // C8
  delay(200);
  tone(BUZZER, 3136, 200); // G7
  delay(200);
  tone(BUZZER, 2093, 500); // C7
}
