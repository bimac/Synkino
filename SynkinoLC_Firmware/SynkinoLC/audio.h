#pragma once
#include <Adafruit_VS1053.h>
#include <QuickPID.h>

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
    QuickPID myPID = QuickPID(&Input, &Output, &Setpoint);
    float Setpoint, Input, Output;
    int32_t average(int32_t);
    void speedControlPID();
    static bool connected();
    uint16_t _fsPhysical = 0;
    char _filename[11] = {0};
    uint8_t _fps = 0;
    uint16_t _trackNum = 0;
    const uint8_t _SDCD;
    const uint8_t _SDCS;
    uint16_t selectTrackScreen();
    bool loadTrack();
    uint16_t getSamplingRate();
    uint16_t getBitrate();
    uint16_t readWRAM(uint16_t);
};
