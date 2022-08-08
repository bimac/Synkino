# SynkinoLC

Allows sound playback synchronised to a running, jittery Super 8 projector.

This is a fork of [fwachsmuth/Synkino](https://github.com/fwachsmuth/Synkino) with the following changes:

* While the original Synkino relies on two 8-bit ATmega328P microcontrollers (one for the frontend, one for the backend), SynkinoLC makes use of a single 32-bit ARM processor by means of a Teensy microcontroller board.
* SynkinoLC uses only a few off-the-shelf components to make assembly as straightforward as possible. No SMD soldering is required.
* The firmware has been optimized to fit the 62K flash of the affordable but powerful Teensy LC microcontroller board (with a few limitations - see below). Teensy 3.2 is also supported.
* Unlike the original Synkino, SynkinoLC is not designed for battery operation. Use any micro-USB phone charger as a power supply unit.

## Teensy LC vs Teensy 3.2

When assembling SynkinoLC, you can chose between using a Teensy LC or Teensy 3.2 microcontroller board. As of writing, however, Teensy 3.2 microcontrollers are not available for purchase due to the global chip shortage. While Teensy LC has more than enough processing power for SynkinoLC, it comes with a few minor limitations that are linked to its smaller flash of only 62K (vs 256K on Teensy 3.2):

* The binary patch file for the VS1053b audio decoder (```patches.053```) cannot be included with the firmware and needs to be supplied by means of the microSD-card.
* You can store settings for "only" 7 projectors (vs 15 on Teensy 3.2).
* The USB stack has been omitted - you can't use debugging by means of USBSerial. You can, however, use the Teensy's HW serial interface (TX on pin 1, 31250 baud).
* SdFat is running in low-mem mode (no support for FAT32, limited to 32GB cards and 64 character filenames).

Neither of these limitations should have a significant impact on the usability of SynkinoLC.

## Bill of Materials

You'll need the following components to assemble SynkinoLC:

| Qty | Description                                                                                            |
| :-: | :----------------------------------------------------------------------------------------------------- |
|  1  | SynkinoLC PCB                                                                                          |
|  1  | [Teensy-LC microcontroller board](https://octopart.com/dev-13305-sparkfun-66786787)                    |
|  1  | [Adafruit VS1053 Codec + MicroSD Breakout](https://octopart.com/1381-adafruit+industries-32978404)     |
|  1  | [Texas Instruments SN74AHC14N ](https://octopart.com/sn74ahc14n-texas+instruments-465338)              |
|  2  | [2.5mm audio jack](https://octopart.com/sj1-2503a-cui+devices-106235597)
|  2  | [2.5mm audio cable, stereo](https://octopart.com/ca-2203-tensility-19254819)                           |
