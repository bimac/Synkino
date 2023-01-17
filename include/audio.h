#pragma once
#include <Adafruit_VS1053.h>
#include <QuickPID.h>

class Audio : public Adafruit_VS1053_FilePlayer {
  public:
    Audio(void);
    uint8_t begin();
    bool selectTrack();
    bool SDinserted();
    const char getRevision();
    static void countISR();
    static void leaderISR();
    bool loadTrack(uint16_t);
    bool currentlyLooping;
    uint8_t state;

  private:
    QuickPID myPID = QuickPID(&Input, &Output, &Setpoint);
    float Setpoint, Input, Output;

    uint16_t _fsPhysical = 0;
    char _filename[13] = {0};
    char autoLoop[12] = {0};
    bool _isLoop = false;
    uint8_t _fps = 0;
    uint16_t _trackNum = 0;
    int32_t _frameOffset = 0;
    uint32_t lastSampleCounterHaltPos = 0;
    int32_t  syncOffsetImps = 0;
    uint32_t sampleCountBaseLine = 0;
    uint16_t impToSamplerateFactor;
    uint16_t deltaToFramesDivider;
    uint16_t impToAudioSecondsDivider;

    bool loadPatch();
    void enableResampler();
    void adjustSamplerate(signed long ppm2);
    void clearSampleCounter();
    void clearErrorCounter();
    uint32_t sciRead32(uint16_t addr);
    void restoreSampleCounter(uint32_t samplecounter);
    int32_t average(int32_t);
    void speedControlPID();
    uint8_t handlePause();
    static bool connected();
    uint16_t selectTrackScreen();
    uint32_t getSampleCount();
    void drawPlayingMenuConstants();
    void drawWaitForPlayingMenu();
    void drawPlayingMenu();
    void drawPlayingMenuStatus();
    void handleFrameCorrectionOffsetInput();
    uint8_t drawTrackLoadedMenu();
    uint8_t drawTrackLoadedMenu(uint8_t);
    uint16_t getSamplingRate();
    uint16_t getBitrate();
    uint16_t readWRAM(uint16_t);
};
