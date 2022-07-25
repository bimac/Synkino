#pragma once

uint8_t mainMenuSelection               = MENU_ITEM_SELECT_TRACK;
uint8_t projectorActionMenuSelection    = MENU_ITEM_SELECT;
uint8_t projectorSelectionMenuSelection = 0;
uint8_t extrasMenuSelection             = 1;
uint8_t trackLoadedMenuSelection        = MENU_ITEM_EXIT;


volatile bool haveI2Cdata = false;
volatile uint8_t i2cCommand;
volatile long i2cParameter;

uint8_t fps = 0;
uint8_t fileType = 0;
uint8_t trackLoaded = 0;
uint8_t startMarkHit = 0;
uint8_t projectorPaused = 0;

unsigned long totalSeconds = 0;
uint8_t myHours   = 0;
uint8_t myMinutes = 0;
uint8_t mySeconds = 0;

int oosyncFrames = 0;

uint8_t myState = MAIN_MENU;
