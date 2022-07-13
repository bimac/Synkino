#pragma once

#define SPI_MOSI       11
#define SPI_MISO       12
#define SPI_SCK        14  // Alternative pin to bypass the LED

#define OLED_DC         7
#define OLED_CS         5
#define OLED_RST        6
#define OLED_DET        4

#define RSH             8
#define TSH             9

#define VS1053_RST     21  // reset pin
#define VS1053_CS      19  // chip select pin
#define VS1053_DCS     15  // data/command select
#define VS1053_DREQ    10  // data request
#define VS1053_SDCS    18  // SD card chip select
#define VS1053_SDCD    16  // SD card detection

#define ENCODER_A      23
#define ENCODER_B      22
#define ENCODER_BTN    20

#define PIN_BUZZER     17

#define IMPULSE         0
#define STARTMARK       1
