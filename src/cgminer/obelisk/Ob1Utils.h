// Ob1 low-level utils functions

#include "Ob1Defines.h"

#define INIT_LOCK(m) (pthread_mutex_init(m, NULL))
#define LOCK(m) (pthread_mutex_lock(m))
#define UNLOCK(m) (pthread_mutex_unlock(m))

void ob1SetRedLEDOff();
void ob1SetRedLEDOn();
void ob1ToggleRedLED();
void ob1SetGreenLEDOff();
void ob1SetGreenLEDOn();
void ob1ToggleGreenLED();

ApiError ob1InitializeControlBoard();
ApiError ob1InitializeHashBoards();

ApiError ob1SpiWriteReg(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, uint8_t registerId, void* pData);

ApiError ob1SpiReadReg(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, uint8_t registerId, void* pData);

ApiError ob1SpiReadChipReg(uint8_t boardNum, uint8_t chipNum, uint8_t registerId, void* pData);

uint64_t getSC1DividerBits(uint8_t divider);
uint64_t getDCR1DividerBits(uint8_t divider);
uint64_t getSC1BiasBits(int8_t bias);
uint64_t getDCR1BiasBits(int8_t bias);
ApiError pulseSC1DataValid(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum);
ApiError pulseDCR1DataValid(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum);
ApiError pulseSC1ReadComplete(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum);
ApiError pulseDCR1ReadComplete(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum);
