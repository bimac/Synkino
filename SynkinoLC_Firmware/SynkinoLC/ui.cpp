#include "ui.h"
#include "buzzer.h"

UI::UI(void) {}

int8_t UI::encDir() const& {
  return encDir_;
}

void UI::reverseEncoder(bool doInverse) {
  encDir_ = (doInverse) ? -1 : 1;
}

void UI::insertPaddedInt(char* target, uint16_t number, uint8_t base, uint8_t width) {
  char buffer[width+1];
  itoa(number, buffer, base);
  memcpy(&target[width-strlen(buffer)], &buffer, strlen(buffer));
}

void UI::waitForBttnRelease() {
  while (!enc.getButton()) {
    yield();
  }
}

void UI::drawBusyBee(u8g2_uint_t x, u8g2_uint_t y) {
  u8g2->clearBuffer();
  u8g2->drawXBMP(x, y, busybee_xbm_width, busybee_xbm_height, busybee_xbm_bits);
  u8g2->setFont(FONT10);
  u8g2->drawStr(8,50,"Loading...");
  u8g2->sendBuffer();
}

uint8_t UI::userInterfaceMessage(const char *title1, const char *title2, const char *title3, const char *buttons) {
  u8g2->setFont(FONT08);
  u8g2->setFontRefHeightAll();
  uint8_t out = u8g2->userInterfaceMessage(title1, title2, title3, buttons);
  u8g2->setFont(FONT10);
  u8g2->setFontRefHeightText();
  return out;
}

bool UI::showError(const char* errorMsg1) {
  return showError(errorMsg1, "");
}

bool UI::showError(const char* errorMsg1, const char* errorMsg2) {
  PRINT("ERROR: ");
  PRINT(errorMsg1);
  PRINT(" ");
  PRINTLN(errorMsg2);
  buzzer.playError();
  userInterfaceMessage("ERROR", errorMsg1, errorMsg2, " Okay ");
  return false;
}

void UI::drawCenteredStr(u8g2_uint_t y, const char *s) {
  u8g2_uint_t x = (128 - u8g2->getStrWidth(s)) / 2;
  u8g2->drawStr(x, y, s);
}

void UI::drawLeftAlignedStr(u8g2_uint_t y, const char *s) {
  u8g2->drawStr(0, y, s);
}

void UI::drawRightAlignedStr(u8g2_uint_t y, const char *s) {
  u8g2_uint_t x = 128 - u8g2->getStrWidth(s);
  u8g2->drawStr(x, y, s);
}

void UI::drawCenteredStr(u8g2_uint_t y, const char *s, const uint8_t *font_8x8) {
  u8g2->setFont(font_8x8);
  u8g2_uint_t x = (128 - u8g2->getStrWidth(s)) / 2;
  u8g2->drawStr(x, y, s);
}

void UI::drawLeftAlignedStr(u8g2_uint_t y, const char *s, const uint8_t *font_8x8) {
  u8g2->setFont(font_8x8);
  u8g2->drawStr(0, y, s);
}

void UI::drawRightAlignedStr(u8g2_uint_t y, const char *s, const uint8_t *font_8x8) {
  u8g2->setFont(font_8x8);
  u8g2_uint_t x = 128 - u8g2->getStrWidth(s);
  u8g2->drawStr(x, y, s);
}

void UI::editCharArray(char* name, uint8_t charLim, const char* prompt) {
  bool isIncrement, pressed;
  bool longPressed = false, firstUse = true, inputFinished = false;
  bool thisCursor, prevCursor = false;
  char thisChar, prevChar = 0;
  unsigned long tPress = 0;

  if (strlen(name) >= charLim) {      // if character limit reached ...
    thisChar = name[charLim - 1];     // ... select name's last character ...
    name[charLim - 1] = '\0';         // ... and remove it from the array
  } else if (strlen(name) > 0)        // else if name is not empty ...
    thisChar = 127;                   // ... start with "delete"
  else                                // otherwise ...
    thisChar = 'A';                   // ... begin with letter 'A'.
  enc.setValue(thisChar);
  uint8_t charIdx = strlen(name);

  auto between = [&](uint8_t lower, uint8_t upper) {    // bool: is encoder between two character blocks?
    return thisChar > lower && thisChar < upper;
  };
  auto jumpTo  = [&](uint8_t lower, uint8_t higher) {   // void: jump to next lower or next higher character block
    thisChar = (isIncrement) ? higher : lower;
    enc.setValue(thisChar);
  };

  while (!enc.buttonChanged())                          // wait for initial change of button state
    yield();
  pressed = !enc.getButton();

  while (!inputFinished) {
    yield();                                            // call yield

    if (enc.valueChanged() || !prevChar) {              // NEW ENCODER VALUE
      buzzer.playClick();                               // play feedback sound
      thisChar = enc.getValue();                        // get char from encoder
      isIncrement = thisChar > prevChar;
      if (thisChar < ' ' || thisChar > 127)             // roll-over
        jumpTo('z', 'A');
      else if (between('Z', 'a'))                       // Ord : Chr : Ascii
        jumpTo('Z', 'a');                               // ----:-----:-------
      else if (between('z', 127))                       //  1  : A-Z : 65- 90
        jumpTo('9', ' ');                               //  2  : a-z : 97-122
      else if (between(' ', '0'))                       //  3  : Spc :     32
        jumpTo(' ', '0');                               //  4  : 0-9 : 48- 57
      else if (between('9', 'A'))                       //  5  : Del :    127
        jumpTo(127, 127);

    } else if (enc.buttonChanged()) {                   // NEW BUTTON STATE
      pressed = !enc.getButton();                       // get new state of encoder button

      if (pressed) {                                    // BUTTON PRESSED
        buzzer.playPress();                             // play feedback sound
        tPress = millis();                              // save timestamp

      } else {                                          // BUTTON RELEASED
        tPress = 0;                                     // reset timestamp

        if (longPressed) {                              // BUTTON RELEASED (LONG PRESS)
          inputFinished = true;                         // end loop

        } else if (thisChar == 127) {                   // BUTTON RELEASED (SHORT PRESS, DELETE)
          if (charIdx > 0)
            charIdx--;                                  // decrement charIdx
          name[charIdx] = '\0';                         // remove last char from array

        } else if (charIdx < charLim) {                 // BUTTON RELEASED (SHORT PRESS, ANY OTHER CHAR)
          name[charIdx] = thisChar;                     // add char to array
          charIdx++;                                    //
          if (firstUse) {
            if (between(64, 91) && charIdx == 1) {      // change to lower case
              thisChar += 32;
              enc.setValue(thisChar);
            }
            firstUse = false;
          }
        }
      }

    } else if (pressed && charIdx>0 && !longPressed && millis() - tPress > 1500) { // DETECT LONG PRESS
      longPressed = true;
      buzzer.playConfirm();
    }

    thisCursor = millis() % 600 < 400;
    if (prevChar==thisChar && thisCursor==prevCursor)           // only draw if necessary
      continue;
    u8g2->clearBuffer();

    drawCenteredStr(14, prompt, FONT08);                        // draw heading & user input
    u8g2->setFont(FONT10);
    u8g2->setCursor(15, 35);
    u8g2->print(name);
    if (thisCursor && (!pressed || !charIdx)) {
      if (thisChar == 32)
        u8g2->print("_");
      else if (thisChar == 127) {
        u8g2->setFont(u8g2_font_m2icon_9_tf);
        u8g2->print("a");
      } else
        u8g2->print(thisChar);
    }

    if (longPressed)                                            // Draw footer
      drawCenteredStr(55, "[Saved!]", FONT08);
    else if (pressed && charIdx > 0)
      drawCenteredStr(55, "[Keep pressed to Save]", FONT08);
    else {
      if (thisChar == 32)
        drawCenteredStr(55, "[Space]", FONT08);
      else if (thisChar == 127)
        drawCenteredStr(55, "[Delete last]", FONT08);
      else
        drawCenteredStr(55, (firstUse || !charIdx)
          ? "[Turn and push Knob]"
          : "[Long Press to Finish]", FONT08);
    }

    u8g2->setFont(FONT10);
    u8g2->sendBuffer();

    prevCursor = thisCursor;
    prevChar   = thisChar;
  }
}

void UI::drawSplash() {
  for (int8_t y = -logo_xbm_height; y <= logo_xbm_y; y++) {
    u8g2->clearBuffer();
    u8g2->drawXBMP(logo_xbm_x, y, logo_xbm_width, logo_xbm_height, logo_xbm_bits);
    u8g2->sendBuffer();
    delay(10);
  }
  u8g2->drawXBMP(logolc_xbm_x, logolc_xbm_y, logolc_xbm_width, logolc_xbm_height, logolc_xbm_bits);
  u8g2->drawXBMP(ifma_xbm_x, ifma_xbm_y, ifma_xbm_width, ifma_xbm_height, ifma_xbm_bits);
  u8g2->sendBuffer();
  buzzer.playHello();
  u8g2->setFont(FONT10);
  u8g2->setFontRefHeightText();
}
