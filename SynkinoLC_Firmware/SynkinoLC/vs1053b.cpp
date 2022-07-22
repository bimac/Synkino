#include "vs1053b.h"
#include "serialdebug.h"

// Add patches.053 to flash?
#ifdef INC_PATCHES
  #include <incbin.h>
  INCBIN(Patch, "patches.053");
#endif

// Constructor
VS1053B::VS1053B(int8_t rst, int8_t cs, int8_t dcs, int8_t dreq, int8_t cardCS, uint8_t SDCD)
    : Adafruit_VS1053_FilePlayer{ rst, cs, dcs, dreq, cardCS }
    , _SDCD { SDCD }, _SDCS { (uint8_t) cardCS } {
  pinMode(_SDCD, INPUT_PULLUP);
}

uint8_t VS1053B::begin() {
  PRINTLN("Initializing SD card ...");
  if (!SD.begin(_SDCS))                          // initialize SD library and card
    return 1;

  PRINTLN("Initializing VS1053B ...");
  if (!Adafruit_VS1053_FilePlayer::begin())      // initialize base class
    return 2;

  if (!loadPatch())                              // load & apply patch
    return 3;

  useInterrupt(VS1053_FILEPLAYER_PIN_INT);       // use VS1053 DREQ interrupt

  // decrease priority of DREQ interrupt
  #if defined(__MKL26Z64__)                      // Teensy LC  [MKL26Z64]

  #elif defined(__MK20DX256__)                   // Teensy 3.2 [MK20DX256]
    NVIC_SET_PRIORITY(IRQ_PORTC, 144);
  #elif defined(ARDUINO_TEENSY40)                // Teensy 4.0 [IMXRT1052]

  #endif

  return 0;                                      // return false (no error)
}

bool VS1053B::SDinserted() {
  return digitalReadFast(_SDCD);
}

bool VS1053B::loadPatch() {
  if (SD.exists("/patches.053")) {
    // If patches.053 is found on SD we'll always try to load it from there ...
    File file = SD.open("/patches.053", O_READ);
    uint16_t addr, n, val, i = 0;
    bool status = false;
    PRINTF("Applying \"patches.053\" (%d bytes) from SD card ... ", file.size());
    while (file.read(&addr, 2) && file.read(&n, 2)) {
      i += 2;
      if (n & 0x8000U) {
        n &= 0x7FFF;
        status = file.read(&val, 2);
        if (!status)
          break;
        while (n--)
          sciWrite(addr, val);
      } else {
        while (n--) {
          status = file.read(&val, 2);
          if (!status)
            break;
          i++;
          sciWrite(addr, val);
        }
      }
    }
    file.close();
    if (status) {
      PRINTLN("done");
      return status;
    } else
      PRINTLN("error reading file.");
  }

  // ... if not, we'll load it from flash memory
  #ifdef INC_PATCHES
    PRINTF("Applying \"patches.053\" (%d bytes) from flash ... ", gPatchSize);
    applyPatch(reinterpret_cast<const uint16_t *>(gPatchData), gPatchSize / 2);
    PRINTF("done\n\n");
    return true;
  #endif
  return false;
}

void VS1053B::enableResampler() {
  sciWrite(SCI_WRAMADDR, 0x1e09);
  sciWrite(SCI_WRAM, 0x0080);
  PRINTLN("15/16 resampling enabled.");
}

void VS1053B::adjustSamplerate(signed long ppm2) {
  sciWrite(SCI_WRAMADDR, 0x1e07);
  sciWrite(SCI_WRAM, ppm2);
  sciWrite(SCI_WRAM, ppm2 >> 16);
  sciWrite(SCI_WRAMADDR, 0x5b1c);
  sciWrite(SCI_WRAM, 0);
  sciWrite(SCI_AUDATA, sciRead(SCI_AUDATA));
}

void VS1053B::clearSampleCounter() {
  sciWrite(SCI_WRAMADDR, 0x1800);
  sciWrite(SCI_WRAM, 0);
  sciWrite(SCI_WRAM, 0);
}

void VS1053B::clearErrorCounter() {
  sciWrite(SCI_WRAMADDR, 0x5a82);
  sciWrite(SCI_WRAM, 0);
}

unsigned int VS1053B::read16BitsFromSCI(unsigned short addr) {
  return (unsigned int)sciRead(addr);
}

unsigned long VS1053B::read32BitsFromSCI(unsigned short addr) {
  unsigned short msbV1, lsb, msbV2;
  sciWrite(SCI_WRAMADDR, addr+1);
  msbV1 = (unsigned int)sciRead(SCI_WRAM);
  sciWrite(SCI_WRAMADDR, addr);
  lsb   = (unsigned long)sciRead(SCI_WRAM);
  msbV2 = (unsigned int)sciRead(SCI_WRAM);
  if (lsb < 0x8000U) {
    msbV1 = msbV2;
  }
  return ((unsigned long)msbV1 << 16) | lsb;
}

void VS1053B::restoreSampleCounter(unsigned long samplecounter) {
  sciWrite(SCI_WRAMADDR, 0x1800); // MSB
  sciWrite(SCI_WRAM, samplecounter);
  sciWrite(SCI_WRAM, samplecounter >> 16);
}

// uint16_t VS1053B::getBitrate() {
//   return (Mp3ReadWRAM(PARA_BYTERATE)>>7);
// }

const char VS1053B::getRevision() {
  char rev = 0;
  for (uint8_t i = 0; i<4; i++) {
    if (GPIO_digitalRead(i))
      bitSet(rev, i);
  }
  return rev + 65;
}
