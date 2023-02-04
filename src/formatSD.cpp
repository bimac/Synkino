#include <SD.h>
#include "formatSD.h"
#include "pins.h"
#include "ui.h"
#include "serialdebug.h"

// modified from
// https://github.com/greiman/SdFat/blob/master/examples/SdFormatter/SdFormatter.ino

#define SPI_CLOCK SD_SCK_MHZ(50)
#define SD_CONFIG SdSpiConfig(VS1053_SDCS, SHARED_SPI, SPI_CLOCK)

void formatSD() {

  if (ui.userInterfaceMessage("WARNING", "All data will be lost.", "Are you sure?", " Cancel \n Yes ") == 1)
    return false;

  SdCardFactory cardFactory;
  SdCard* m_card = cardFactory.newCard(SD_CONFIG);
  uint8_t  sectorBuffer[512];
  uint32_t cardSectorCount = m_card->sectorCount();
  if (!cardSectorCount)
    ui.showError("Could not obtain sector count.");

  // Some feedback regarding size and format of SD card
  PRINT("\nSize of SD card: ");
  PRINT(cardSectorCount*5.12e-7);
  PRINT(" GB\n");
#if defined(MYSERIAL)
  PRINT("SD card will be formatted to ");
  if (cardSectorCount > 67108864)
    PRINTLN("exFAT.\n");
  else if (cardSectorCount > 4194304)
    PRINTLN("FAT32.\n");
  else
    PRINTLN("FAT16.\n");
#endif

  // Erase all data
  uint32_t const ERASE_SIZE = 262144L;
  PRINT("Erasing ");
  uint32_t firstBlock = 0;
  uint32_t lastBlock;
  uint16_t n = 0;
  do {
    lastBlock = firstBlock + ERASE_SIZE - 1;
    if (lastBlock >= cardSectorCount)
      lastBlock = cardSectorCount - 1;
    if (!m_card->erase(firstBlock, lastBlock))
      ui.showError("Erase failed.");
    PRINT(".");
    if ((n++)%64 == 63)
      PRINTLN(" ");
    firstBlock += ERASE_SIZE;
  } while (firstBlock < cardSectorCount);
  PRINTLN(" ");
  if (!m_card->readSector(0, sectorBuffer)) {
    ui.showError("Error reading block.");
  }
  PRINT("All data set to 0x\n");
  PRINTLN(int(sectorBuffer[0]), HEX);

  // Format SD card
  ExFatFormatter exFatFormatter;
  FatFormatter fatFormatter;
  bool rtn = cardSectorCount > 67108864 ?
    exFatFormatter.format(m_card, sectorBuffer, &Serial) :
    fatFormatter.format(m_card, sectorBuffer, &Serial);

  // Feedback and reboot
  ui.userInterfaceMessage("SD has been formatted.", "SynkinoLC will now reboot.", "", " OK ");
  SCB_AIRCR = 0x05FA0004;
}
