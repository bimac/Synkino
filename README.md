# SynkinoLC

Allows sound playback synchronised to a running, jittery Super 8 projector.

This is a fork of [fwachsmuth/Synkino](https://github.com/fwachsmuth/Synkino) with the following changes:

* While the original Synkino relies on two 8-bit ATmega328P microcontrollers (one for the frontend, one for the backend), SynkinoLC makes use of a single 32-bit ARM processor by means of a Teensy microcontroller board.
* SynkinoLC uses only a few off-the-shelf components to make assembly as straightforward as possible. No SMD soldering is required.
* The firmware has been optimized to fit the 62K flash of the affordable but powerful Teensy LC microcontroller board (with a few limitations - see below). Teensy 3.2 is also supported.
* Unlike the original Synkino, SynkinoLC is not designed for battery operation. Use any micro-USB phone charger as a power supply unit.

Apart from these changes, [Friedemann's manual for the original Synkino](https://www.filmkorn.org/synkino-instruction-manual/?lang=en) should apply for SynkinoLC as well.


## Teensy LC vs Teensy 3.2

When assembling SynkinoLC, you can chose between using a Teensy LC or Teensy 3.2 microcontroller board. As of writing, however, Teensy 3.2 microcontrollers are not available for purchase due to the global chip shortage. While Teensy LC has more than enough processing power for SynkinoLC, it comes with a few minor limitations that are linked to its smaller flash of only 62K (vs 256K on Teensy 3.2):

* The binary patch file for the VS1053b audio decoder (```patches.053```) cannot be included with the firmware and needs to be supplied by means of the microSD-card.
* You can store settings for "only" 7 projectors (vs 15 on Teensy 3.2).
* The USB stack has been omitted - you can't use debugging by means of USBSerial. You can, however, use the Teensy's HW serial interface (TX on pin 1, 31250 baud).
* SdFat is running in low-mem mode (no support for exFAT, limited to 32GB cards and 64 character filenames).

Neither of these limitations should have a significant impact on the usability of SynkinoLC.


## Building the Firmware

SynkinoLC was created with [PlatformIO](https://docs.platformio.org/en/latest/what-is-platformio.htm) for VSCode. To install PlatformIO:

1. [Download](https://code.visualstudio.com/) and install official Microsoft Visual Studio Code. PlatformIO IDE is built on top of it.
2. Open VSCode Package Manager.
3. Search for the official PlatformIO ide extension.
4. Install PlatformIO IDE.

See [the official guide](https://docs.platformio.org/en/latest/integration/ide/vscode.html) for more details on the VSCode integration.

To build SynkinoLC:
* open ```Synkino.code-workspace```,
* select the respective project environment (```env:teensyLC``` or ```enc:teensy31```), and
* start the build

The firmware won't fit TeensyLC unless you modify ```SD.h``` as outlined [here](https://github.com/PaulStoffregen/SD/pull/44/commits/c3661d2aef4534b5e9cb3a7f66da09e8c61bf286).


## Choice of OLED display

SynkinoLC has been designed to work with two types of 1.3" OLED display, based on either SH1106 or SSD1306 driver chips:
* An SH1106 display module (as used in the original Synkino) can be obtained for little money on [aliexpress.com](https://aliexpress.com/wholesale?SearchText=sh1106+128+64) (go for the one that says ```1.30"OLED 4-SPI``` on the back). Other SH1106 modules should work as well, as long as they have identical pin-out (the foot-print for the retaining screws might not be an exact fit though).
* Alternatively, you can go for [Adafruit #938](https://octopart.com/938-adafruit+industries-32979003) which is based on an SSD1306 driver (you'll have to cut two traces on the back of the module to enable SPI as demonstrated [here](https://www.youtube.com/watch?v=SXfV4e_jpf8))

Just connect the display module to the respective slot on the SynkinoLC PCB - the firmware will automatically detect the type of display used.


## Bill of Materials

You'll need the following components to assemble SynkinoLC:

| Qty | Part                                                                                               | Description / Comment                     |
| :-: | :------------------------------------------------------------------------------------------------- | :---------------------------------------- |
|  1  | SynkinoLC PCB                                                                                      |                                           |
|  1  | 1.3" 128x64 monochrome OLED display, SPI                                                           | see separate comments above               |
|  1  | [Teensy-LC](https://octopart.com/dev-13305-sparkfun-66786787)                                      | microcontroller board                     |
|  1  | [Adafruit VS1053 Codec + MicroSD Breakout](https://octopart.com/1381-adafruit+industries-32978404) |                                           |
|  1  | [Texas Instruments SN74AHC14N](https://octopart.com/sn74ahc14n-texas+instruments-465338)           | inverted Schmitt-trigger                  |
|  2  | [SparkFun ROB-09453](https://octopart.com/rob-09453-sparkfun-67069573)                             | line sensor breakout (analog)             |
|  1  | [Bourns PEC11L-4215F-S0015](https://octopart.com/pec11l-4215f-s0015-bourns-25517430)               | rotary encoder                            |
|  1  | [Cliff CL170849BR](https://octopart.com/cl170849br-cliff-22810934) (or similar)                    | knob for rotary encoder                   |
|  1  | [TDK PS1240P02BT](https://octopart.com/ps1240p02bt-tdk-8602108)                                    | piezo buzzer                              |
|  2  | [CUI Devices SJ1-2503A](https://octopart.com/sj1-2503a-cui+devices-106235597)                      | 2.5mm audio jack                          |
|  2  | [Tensility CA-2203](https://octopart.com/ca-2203-tensility-19254819)  (or similar)                 | 2.5mm audio cable, stereo                 |
|  2  | [Sullins PPPC071LFBN-RC](https://octopart.com/pppc071lfbn-rc-sullins-271056) (or similar)          | 7 pos female header (for SH1106 display)  |
|  1  | [Sullins PPPC081LFBN-RC](https://octopart.com/pppc081lfbn-rc-sullins-271057) (or similar)          | 8 pos female header (for SSD1306 display) |
|  2  | [Sullins PPPC141LFBN-RC](https://octopart.com/pppc141lfbn-rc-sullins-271063) (or similar)          | 14 pos female header                      |
|  2  | [Sullins PPPC181LFBN-RC](https://octopart.com/pppc181lfbn-rc-sullins-271067) (or similar)          | 18 pos female header                      |
|  1  | [Adam Tech PH1-07-UA](https://octopart.com/ph1-07-ua-adam+tech-7873139) (or similar)               | 7 pos male header (for SH1106 display)    |
|  1  | [Adam Tech PH1-08-UA](https://octopart.com/ph1-08-ua-adam+tech-13205589) (or similar)              | 8 pos male header (for SSD1306 display)   |
|  2  | [Adam Tech PH1-14-UA](https://octopart.com/ph1-14-ua-adam+tech-14467007) (or similar)              | 14 pos male header                        |

Click [here](https://octopart.com/bom-tool/omFmd0tC) for a full BOM on [octopart.com](https://octopart.com). All necessary components should be obtainable for about USD60 / EUR60 (not including the PCB).
