#pragma once

#define SPI_MOSI       11
#define SPI_MISO       12
#define SPI_SCK        14  // Alternative pin to bypass the LED on pin 13

#define OLED_DC         4
#define OLED_RST        5
#define OLED_CS         6
#define OLED_DET        7

#define PIN_LOUT        9   // left channel of 3.5mm audio jack (tip position)
#define PIN_TIPSW       8   // switch of 3.5mm audio jack (tip position)

#define VS1053_RST     21   // reset pin
#define VS1053_CS      19   // chip select pin
#define VS1053_DCS     15   // data/command select
#define VS1053_DREQ    10   // data request
#define VS1053_SDCS    18   // SD card chip select
#define VS1053_SDCD    16   // SD card detection

#define ENC_A          23
#define ENC_B          22
#define ENC_BTN        20

#define PIN_BUZZER     17

#define IMPULSE         2
#define STARTMARK       3
