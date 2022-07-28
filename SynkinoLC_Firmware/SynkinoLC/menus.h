#pragma once

// ---- Define the various Menu Screens --------------------------------------------
//
#define MAIN_MENU             1
#define PROJECTOR_ACTION_MENU 2
#define TRACK_SELECTION_MENU  3
#define SETTINGS_MENU         4
#define PLAYING_MENU          5
#define SHUTTER_MENU          6
#define STARTMARK_MENU        7
#define PID_MENU              8

// ---- Define the various Menu Item Positions -------------------------------------
//
#define MENU_ITEM_PROJECTOR       1
#define MENU_ITEM_SELECT_TRACK    2
#define MENU_ITEM_EXTRAS          3

#define MENU_ITEM_NEW             1
#define MENU_ITEM_SELECT          2
#define MENU_ITEM_EDIT            3
#define MENU_ITEM_DELETE          4

#define MENU_ITEM_ONE             1
#define MENU_ITEM_TWO             2
#define MENU_ITEM_THREE           3
#define MENU_ITEM_FOUR            4
#define MENU_ITEM_FIVE            5
#define MENU_ITEM_FRAMES          1
#define MENU_ITEM_P               1
#define MENU_ITEM_I               2
#define MENU_ITEM_D               3

#define MENU_ITEM_MANUALSTART     1
#define MENU_ITEM_STOP            2
#define MENU_ITEM_EXIT            3

#define MENU_ITEM_VERSION         1
#define MENU_ITEM_TEST_IMPULSE    2
#define MENU_ITEM_DEL_EEPROM      3
#define MENU_ITEM_DUMP            4


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
