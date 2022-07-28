#pragma once

// ---- Define the various Menu Item Positions -------------------------------------
//
#define MENU_MAIN                  0
#define MENU_PROJECTOR            10
#define MENU_PROJECTOR_NEW        11
#define MENU_PROJECTOR_SELECT     12
#define MENU_PROJECTOR_EDIT       13
#define MENU_PROJECTOR_DELETE     14
#define MENU_SELECT_TRACK         20
#define MENU_EXTRAS               30
#define MENU_EXTRAS_VERSION       31
#define MENU_EXTRAS_IMPULSE       32
#define MENU_EXTRAS_DEL_EEPROM    33
#define MENU_EXTRAS_DUMP_EEPROM   34
#define MENU_ITEM_MANUALSTART     1
#define MENU_ITEM_STOP            2
#define MENU_ITEM_EXIT            3


// ---- Define structure of menus --------------------------------------------------
//
const char *main_menu =
  "Projector\n"
  "Select Track\n"
  "Extras";

const char *projector_action_menu =
  "New\n"
  "Select\n"
  "Edit\n"
  "Delete\n"
  "Exit";

const char *trackLoaded_menu =
  "Manual Start\n"
  "Stop\n"
  "Exit";

const char *extras_menu =
  "Version\n"
  "Test Impulse\n"
  "Delete EEPROM\n"
  "Exit";

#ifdef SERIALDEBUG
const char *extras_menu_serial =
  "Version\n"
  "Test Impulse\n"
  "Delete EEPROM\n"
  "Dump EEPROM\n"
  "Exit";
#endif
