#pragma once

// Hardware SPI
#define SPI_MOSI       11
#define SPI_MISO       12
#define SPI_SCK        14  // Alternative pin to bypass the LED

#define OLED_DC         7
#define OLED_CS         5
#define OLED_RST        6
#define OLED_DET        4

#define VS1053_RST     21  // VS1053 reset pin (output)
#define VS1053_CS      19  // VS1053 chip select pin (output)
#define VS1053_DCS     15  // VS1053 Data/command select pin (output)
#define VS1053_DREQ    10  // VS1053 Data request, ideally an Interrupt pin
#define CARD_CS        18  // SD Card chip select pin
#define CARD_DET       16  // SD Card detection

#define ENCODER_A      23
#define ENCODER_B      22
#define ENCODER_BTN    20

#define BUZZER         17

#define IMPULSE         0
#define STARTMARK       1
