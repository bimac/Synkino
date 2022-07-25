#include "projector.h"

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

bool Projector::load() {
  uint8_t idx = select("Edit Projector");
  return load(idx);
}

bool Projector::load(uint8_t idx) {
  if (idx > count() || idx == 0)             // EEPROM not initialized?
    return false;
  config_ = e2load(idx);                     // load configuration from EEPROM
  lastUsed(idx);                             // update lastUsed

  // Print details to serial
  PRINTF("\nUsing projector #%d:\n",idx);
  PRINTF("   Name:              %s\n",config_.name);
  PRINTF("   Shutter Blades:    %d\n",config_.shutterBladeCount);
  PRINTF("   Start Mark Offset: %d frames\n",config_.startmarkOffset);
  PRINTF("   Proportional:      %d\n",config_.p);
  PRINTF("   Integral:          %d\n",config_.i);
  PRINTF("   Derivative:        %d\n\n",config_.d);
  return true;
}

void Projector::loadLast(void) {
  if (load(lastUsed()))
    return;
  e2delete(); // EEPROM invalid - reinitialize
}

EEPROMstruct Projector::config(void) const& {
  return config_;
}


void Projector::remove() {
  uint8_t idx = select("Delete Projector");
  remove(idx);
}

void Projector::remove(uint8_t idx) {
  uint8_t c = count();
  if (idx>c)
    return;

  EEPROMstruct aProjector = e2load(idx);
  char name[sizeof(aProjector.name) + 3] = "\"";\
  strcat(name,aProjector.name);
  strcat(name,"\"");
  if (ui.userInterfaceMessage("Delete projector", name, "Are you sure?", " Cancel \n Yes ") == 1)
    return;
  PRINTF("Deleting projector %s.\n",name);

  for (uint8_t i = idx; i < c; i++) {       // shift projectors up in EEPROM
    aProjector = e2load(i+1);
    e2save(i, aProjector);
  }
  count(--c);                               // decrement projector count
  e2dump();                                 // dump contents of EEPROM

  if (idx == lastUsed())                    // load first projector
    load(1);
}

bool Projector::create() {
  if (count() >= MAX_PROJECTOR_COUNT) {
    char buf[20] = "Only ";
    itoa(MAX_PROJECTOR_COUNT, &buf[strlen(buf)], 10);
    strcat(buf, " allowed.");
    return ui.showError("Too many Projectors.", buf);
  }
  return edit(0);
}

bool Projector::edit() {
  uint8_t idx = select("Edit Projector");
  return edit(idx);
}

bool Projector::edit(uint8_t idx) {
  EEPROMstruct aProjector;        // start with default projector struct

  if (idx == 0) {                 // we have a NEW projector here
    idx = count() + 1;            // set new idx
    count(idx);                   // update EEPROM: projector count
  } else
    aProjector = e2load(idx);     // load data from EEPROM

  ui.editCharArray(aProjector.name, MAX_PROJECTOR_NAME_LENGTH, "Set Projector Name");
  ui.reverseEncoder(true);
  u8g2->userInterfaceInputValue("# Shutter Blades:",  "", &aProjector.shutterBladeCount, 1,   4, 1, "");
  u8g2->userInterfaceInputValue("Start Mark Offset:", "", &aProjector.startmarkOffset,   1, 255, 3, " Frames");
  u8g2->userInterfaceInputValue("Proportional:",      "", &aProjector.p,                 0,  99, 2, "");
  u8g2->userInterfaceInputValue("Integral:",          "", &aProjector.i,                 0,  99, 2, "");
  u8g2->userInterfaceInputValue("Derivative:",        "", &aProjector.d,                 0,  99, 2, "");
  ui.reverseEncoder(false);

  e2save(idx,aProjector);         // store projector data to EEPROM
  return load(idx);               // use this projector
}

uint8_t Projector::select(const char* prompt) {
  uint8_t c = count();
  char menu[MAX_PROJECTOR_NAME_LENGTH * MAX_PROJECTOR_COUNT + MAX_PROJECTOR_COUNT] = {0};
  EEPROMstruct aProjector;
  for (uint8_t i = 1; i <= c; i++) {
    aProjector = e2load(i);
    strcat(menu, aProjector.name);
    if (i < c)
      strcat(menu, "\n");
  }
  return u8g2->userInterfaceSelectionList(prompt, lastUsed(), menu);
}

EEPROMstruct Projector::e2load(uint8_t idx) {
  EEPROMstruct aProjector;
  EEPROM.get((idx - 1) * sizeof(EEPROMstruct) + EEPROM_HEADER_BYTES, aProjector);
  return aProjector;
}

void Projector::e2save(uint8_t idx, EEPROMstruct &data) {
  EEPROM.put((idx - 1) * sizeof(EEPROMstruct) + EEPROM_HEADER_BYTES, data);
}


void Projector::e2dump(void) {
  #ifdef SERIALDEBUG
  char buffer[16];
  for (uint16_t address = 0; address <= EEPROM_BYTES_REQUIRED; address += 16) {
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
  create();
}
