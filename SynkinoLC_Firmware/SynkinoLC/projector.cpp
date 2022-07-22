#include "projector.h"
#include "serialdebug.h"

Projector::Projector(void) {}

uint8_t Projector::count(void) {
  return EEPROM.read(EEPROM_IDX_COUNT);
}

void Projector::count(uint8_t n) {
  EEPROM.update(EEPROM_IDX_COUNT, n);
}

uint8_t Projector::lastUsed(void) {
  return EEPROM.read(EEPROM_IDX_LAST);
}

void Projector::lastUsed(uint8_t n) {
  EEPROM.update(EEPROM_IDX_LAST, n);
}

EEPROMstruct Projector::e2load(uint8_t idx) {
  EEPROMstruct aProjector;
  EEPROM.get((idx - 1) * sizeof(EEPROMstruct) + EEPROM_HEADER_BYTES, aProjector);
  return aProjector;
}

void Projector::e2save(uint8_t idx, EEPROMstruct &data) {
  EEPROM.put((idx - 1) * sizeof(EEPROMstruct) + EEPROM_HEADER_BYTES, data);
}

void Projector::remove(uint8_t idx) {
  EEPROMstruct aProjector = e2load(idx);
  char name[sizeof(aProjector.name) + 3] = "\"";\
  strcat(name,aProjector.name);
  strcat(name,"\"");
  if (userInterfaceMessage("Delete projector", name, "Are you sure?", " Cancel \n Yes ") == 1)
    return;
  PRINTF("Deleting projector %s.\n",name);

  uint8_t projectorCount = count();
  for (uint8_t i = idx; i < projectorCount; i++) {
    aProjector = e2load(i+1);
    e2save(i, aProjector);
  }
  count(projectorCount - 1);
  e2dump();

  // if (idx == lastUsed())
  //   loadProjectorConfig(1);
}

void Projector::e2dump(void) {
  #ifdef SERIALDEBUG
  char buffer[16];
  for (uint8_t address = 0; address <= 127; address += 16){
    EEPROM.get(address, buffer);                                // get data from EEPROM
    PRINTF("\n 0x%05X:  ", address);                            // print address
    for (uint8_t i = 1; i <= 16; i++)
      PRINTF((i % 8 > 0) ? "%02X " : "%02X   ", buffer[i-1]);   // print HEX values
    for (int i = 0; i < 16; i++) {
      if (i == 8)
        PRINT(" ");
      if (buffer[i] > 31 and buffer[i] < 127)
        PRINT(buffer[i]);                                       // print ASCII values
      else
        PRINT(".");
    }
  }
  PRINTLN("\n");                                                // print final new line
  #endif
}

void Projector::e2delete(void) {
  PRINTLN("Deleting contents of EEPROM ...");
  for (int i = 0; i < EEPROM.length(); i++)
    EEPROM.update(i, 0);
  e2dump();
}
