#pragma once

const int myAddress = 0x07;     // Our i2c address here

// const byte encType = 15;    // 15 dents
const byte encType = 30; // 30 dents


const byte maxProjectorNameLength = 12;
const byte maxProjectorCount = 8;
char newProjectorName[maxProjectorNameLength + 1];
char projectorSelection_menu[maxProjectorNameLength * maxProjectorCount + maxProjectorCount];
//char errorMsg[24];

uint8_t mainMenuSelection               = MENU_ITEM_SELECT_TRACK;
uint8_t projectorActionMenuSelection    = MENU_ITEM_SELECT;
uint8_t projectorSelectionMenuSelection = 0;
uint8_t projectorConfigMenuSelection    = MENU_ITEM_NAME;
uint8_t shutterBladesMenuSelection      = MENU_ITEM_TWO;
uint8_t extrasMenuSelection             = 1;
uint8_t trackLoadedMenuSelection        = MENU_ITEM_EXIT;


volatile bool haveI2Cdata = false;
volatile uint8_t i2cCommand;
volatile long i2cParameter;

uint8_t myState = MAIN_MENU;

uint8_t fps = 0;
uint8_t fileType = 0;
uint8_t trackLoaded = 0;
uint8_t startMarkHit = 0;
uint8_t projectorPaused = 0;

unsigned long totalSeconds = 0;
uint8_t hours   = 0;
uint8_t minutes = 0;
uint8_t seconds = 0;

volatile unsigned long lastActivityMillies = millis();

int oosyncFrames = 0;


byte   new_p = 8;
byte   new_i = 3;
byte   new_d = 1;
byte   newStartmarkOffset = 1;
int8_t newFrameCorrectionOffset = 0;
int16_t newSyncOffset;

byte lastProjectorUsed = 1;

/* Below is the EEPROM struct to save Projector Configs.
 *  Before the structs start, there are two header bytes:
 *  EEPROM.read(0, projectorCount);
 *  EEPROM.read(1, lastProjectorUsed);
 */
struct Projector {          // 19 Bytes per Projector
  byte  index;
  byte  shutterBladeCount;
  byte  startmarkOffset;
  byte  p;
  byte  i;
  byte  d;
  char  name[maxProjectorNameLength + 1];
};
