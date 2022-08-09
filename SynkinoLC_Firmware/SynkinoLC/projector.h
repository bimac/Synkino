#pragma once

#include <EEPROM.h>
#include "ui.h"
#include "serialdebug.h"

// A few macros for handling of the EEPROM storage
#define EEPROM_HEADER_BYTES        2
#define EEPROM_IDX_COUNT           0
#define EEPROM_IDX_LAST            1
#define MAX_PROJECTOR_NAME_LENGTH  12
#define EEPROM_BYTES_PER_PROJECTOR (MAX_PROJECTOR_NAME_LENGTH + 5)
#if defined(__MKL26Z64__)
  #define EEPROM_SIZE              128 // Teensy LC has only 128 bytes of EEPROM!
#else
  #define EEPROM_SIZE              256 // Lets stick with uint8 addresses for now
#endif
#define MAX_PROJECTOR_COUNT        ((EEPROM_SIZE - EEPROM_HEADER_BYTES) / EEPROM_BYTES_PER_PROJECTOR)
#define EEPROM_BYTES_REQUIRED      (EEPROM_BYTES_PER_PROJECTOR * MAX_PROJECTOR_COUNT + EEPROM_HEADER_BYTES)

struct EEPROMstruct {
  uint8_t shutterBladeCount = 2;
  uint8_t startmarkOffset = 1;
  uint8_t p = 8;
  uint8_t i = 3;
  uint8_t d = 1;
  char name[MAX_PROJECTOR_NAME_LENGTH + 1] = {0};
};

class Projector {
  public:
    Projector(void);
    bool create(void);                   // create projector
    bool edit();                         // modify projector
    bool edit(uint8_t);                  // modify specific projector
    bool load();                         // load projector
    bool load(uint8_t);                  // load specific projector
    void loadLast(void);                 // load last used projector
    void remove();                       // delete projector
    void remove(uint8_t);                // delete specific projector
    uint8_t select(const char*);         // select projector
    uint8_t count(void);                 // get projector count
    uint8_t lastUsed(void);              // get last used projector
    void e2dump(void);                   // dump EEPROM to serial
    void e2delete(void);                 // delete EEPROM
    EEPROMstruct config(void) const&;    // return current projector's configuration

  private:
    EEPROMstruct config_;
    void count(uint8_t);                 // set projector count
    void lastUsed(uint8_t);              // set last used projector
    EEPROMstruct e2load(uint8_t);        // load projector struct from EEPROM
    void e2save(uint8_t, EEPROMstruct&); // save projector struct to EEPROM
    void editName(char*, const char*);
};
