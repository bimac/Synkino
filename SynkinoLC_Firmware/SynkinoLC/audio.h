#pragma once

#include <Adafruit_VS1053.h>

// Adding some synonyms for backward compatibility
#define SCI_MODE            VS1053_REG_MODE
#define SCI_STATUS          VS1053_REG_STATUS
#define SCI_BASS            VS1053_REG_BASS
#define SCI_CLOCKF          VS1053_REG_CLOCKF
#define SCI_DECODE_TIME     VS1053_REG_DECODETIME
#define SCI_AUDATA          VS1053_REG_AUDATA
#define SCI_WRAM            VS1053_REG_WRAM
#define SCI_WRAMADDR        VS1053_REG_WRAMADDR
#define SCI_HDAT0           VS1053_REG_HDAT0
#define SCI_HDAT1           VS1053_REG_HDAT1

#define PARA_CHIPID_0       0x1E00
#define PARA_CHIPID_1       0x1E01
#define PARA_VERSION        0x1E02
#define PARA_CONFIG1        0x1E03
#define PARA_PLAYSPEED      0x1E04
#define PARA_BYTERATE       0x1E05
#define PARA_ENDFILLBYTE    0x1E06
#define PARA_MONOOUTPUT     0x1E09
#define PARA_POSITIONMSEC_0 0x1E27
#define PARA_POSITIONMSEC_1 0x1E28
#define PARA_RESYNC         0x1E29

class Audio : public Adafruit_VS1053_FilePlayer {
  public:
    Audio(int8_t rst, int8_t cs, int8_t dcs, int8_t dreq, int8_t cardCS, uint8_t SDCD);
    uint8_t begin();
    bool selectTrack();
    bool loadPatch();
    bool SDinserted();
    void enableResampler();
    void adjustSamplerate(signed long ppm2);
    void clearSampleCounter();
    void clearErrorCounter();
    unsigned int read16BitsFromSCI(unsigned short addr);
    unsigned long read32BitsFromSCI(unsigned short addr);
    void restoreSampleCounter(unsigned long samplecounter);
    const char getRevision();
    static void countISR();
    static void leaderISR();
  private:
    void speedControlPID();
    bool connected();
    uint16_t _fsPhysical = 0;
    char _filename[11] = {0};
    uint8_t _fps = 0;
    uint16_t _trackNum = 0;
    const uint8_t _SDCD;
    const uint8_t _SDCS;
    uint16_t selectTrackScreen();
    bool loadTrack();
    uint16_t getSamplingRate();
};
