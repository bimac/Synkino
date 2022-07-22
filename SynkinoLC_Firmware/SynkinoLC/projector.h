#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <U8g2lib.h>

// const byte maxProjectorNameLength = 12;
// const byte maxProjectorCount = 8;
// char newProjectorName[maxProjectorNameLength + 1];
// char projectorSelection_menu[maxProjectorNameLength * maxProjectorCount + maxProjectorCount];

#define EEPROM_HEADER_BYTES        2
#define EEPROM_IDX_COUNT           0
#define EEPROM_IDX_LAST            1
#define MAX_PROJECTOR_NAME_LENGTH 12
#define MAX_PROJECTOR_COUNT        8

extern U8G2* u8g2;
extern uint8_t userInterfaceMessage(const char*, const char*, const char*, const char*);

struct EEPROMstruct {
  byte index;
  byte shutterBladeCount;
  byte startmarkOffset;
  byte p;
  byte i;
  byte d;
  char name[MAX_PROJECTOR_NAME_LENGTH + 1];
};

class Projector {
  public:
    Projector();
    void remove(uint8_t);                // delete projector
    uint8_t count(void);                 // get projector count
    uint8_t lastUsed(void);              // get last used projector

    void e2dump(void);                   // dump EEPROM to serial
    void e2delete(void);                 // delete EEPROM
    // int p() const { return p_; }
    // int i() const { return i_; }
    // int d() const { return d_; }

  private:
    uint8_t p_;
    uint8_t i_;
    uint8_t d_;
    void count(uint8_t);    // set projector count
    void lastUsed(uint8_t); // set last used projector
    EEPROMstruct e2load(uint8_t);        // load projector struct from EEPROM
    void e2save(uint8_t, EEPROMstruct&); // save projector struct to EEPROM
};
