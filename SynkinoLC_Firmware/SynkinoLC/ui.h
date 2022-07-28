#pragma once

#include <U8g2lib.h>
#include <EncoderTool.h>
using namespace EncoderTool;

#include "buzzer.h"
#include "serialdebug.h"
#include "xbm.h"

#define FONT10 u8g2_font_helvR10_tr
#define FONT08 u8g2_font_helvR08_tr

extern Buzzer buzzer;
extern U8G2* u8g2;
extern PolledEncoder enc;
extern int8_t encDir;

uint8_t u8x8_GetMenuEvent(u8x8_t);

class UI {
  public:
    UI(void);
    static void insertPaddedInt(char*, uint16_t, uint8_t);
    static void zeroPad(char*, uint16_t, uint8_t, uint8_t);
    static void waitForBttnRelease();
    static void drawBusyBee(u8g2_uint_t x, u8g2_uint_t y);
    static uint8_t userInterfaceMessage(const char *, const char *, const char *, const char *);
    static bool showError(const char*);
    static bool showError(const char*, const char*);
    static void editCharArray(char*, uint8_t, const char*);
    static void drawCenteredStr(u8g2_uint_t, const char*);
    static void drawCenteredStr(u8g2_uint_t, const char*, const uint8_t*);
    static void drawLeftAlignedStr(u8g2_uint_t, const char*);
    static void drawLeftAlignedStr(u8g2_uint_t, const char*, const uint8_t*);
    static void drawRightAlignedStr(u8g2_uint_t, const char*);
    static void drawRightAlignedStr(u8g2_uint_t, const char*, const uint8_t*);
    static void drawSplash();
    int8_t encDir() const&;
    void reverseEncoder(bool);
  private:
    int8_t encDir_ = 1;
};
