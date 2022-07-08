#pragma once

// Hardware SPI
#define SPI_MOSI        11
#define SPI_MISO        12
#define SPI_SCK         14  // Alternative pin to bypass the LED

#define OLED_DC          8
#define OLED_CS         10
#define OLED_RST         9

#define VS1053_RST      16  // VS1053 reset pin (output)
#define VS1053_CS       15  // VS1053 chip select pin (output)
#define VS1053_DCS      20  // VS1053 Data/command select pin (output)
#define VS1053_DREQ      3  // VS1053 Data request, ideally an Interrupt pin
#define CARD_CS         21  // SD Card chip select pin
#define CARD_DET        19  // SD Card detection

#define ENCODER_A        5
#define ENCODER_B        6
#define ENCODER_BTN      4

#define BUZZER           1
