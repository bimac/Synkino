#pragma once
#include <Adafruit_VS1053.h>
#include <QuickPID.h>

union oggPage {
  struct {
    char     magicStr[4];
    uint8_t  version;
    uint8_t  headerType;
    int64_t  granulePos;
    uint32_t bitstreamSN;
    uint32_t crc;
    uint8_t  nSegments;
  };
  unsigned char header[23];
};

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

  private:
    QuickPID myPID = QuickPID(&Input, &Output, &Setpoint);
    float Setpoint = 0, Input, Output;

    uint16_t _fsPhysical = 0;
    char _filename[13] = {0};
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
    void enableResampler(bool);
    void adjustSamplerate(int32_t long);
    void clearSampleCounter();
    void clearErrorCounter();
    uint32_t sciRead32(uint16_t);
    void restoreSampleCounter(uint32_t);
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

    // related to OGG Vorbis
    bool isOgg();
    size_t findInFile(File*, const char*, uint8_t, size_t);
    size_t firstAudioPage(File*);
    int64_t granulePos(oggPage*);

    // see http://www.vsdsp-forum.com/phpbb/viewtopic.php?p=6679#p6679
    int16_t StreamBufferFillWords(void);
    int16_t StreamBufferFreeWords(void);
    int16_t AudioBufferFillWords(void);
    int16_t AudioBufferFreeWords(void);
    uint16_t AudioBufferUnderflow(void);
};
