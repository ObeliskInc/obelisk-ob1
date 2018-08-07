// Ob1 low-level utils functions

#ifndef __OB1UTILS__
#define __OB1UTILS__
#include "Ob1Defines.h"

#define INIT_LOCK(m) (pthread_mutex_init(m, NULL))
#define LOCK(m) (pthread_mutex_lock(m))
#define UNLOCK(m) (pthread_mutex_unlock(m))

// Main API needs access too
extern pthread_mutex_t statusLock;

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

void logNonceSet(NonceSet* pNonceSet, char* prefix);

typedef struct ControlLoopState {
	// Determine if we need to initialize the control state.
	bool initialized;

	// General status variables.
	uint8_t boardNumber;
	time_t currentTime;
	double currentStringVoltage;
	uint8_t currentVoltageLevel;
	time_t lastStatusOutput;
	uint64_t stringTimeouts;

	// Chip settings.
	int8_t chipBiases[15];
	uint8_t chipDividers[15];

	// Hashrate preparation related variables.
	uint64_t currentGoodNonces;
	uint64_t goodNoncesUponLastBiasChange;
	uint64_t goodNoncesUponLastVoltageChange;

	// Temperature management variables.
	double currentStringTemp;
	time_t prevBiasChangeTime;
	time_t prevOvertempCheck;
	time_t prevUndertempCheck;
	double prevUndertempStringTemp;

	// Voltage management variables.
	uint64_t goodNoncesSinceBiasChange;
	uint64_t goodNoncesSinceVoltageChange;
	time_t   prevVoltageChangeTime;
	uint64_t voltageLevelHashrates[128];

	// Genetic coding for 6 children.
	uint64_t childOneHashrate;
	uint8_t childOneVoltageLevel;
	int8_t childOneBiases[15];
	uint8_t childOneDividerse[15];
	uint64_t childTwoHashrate;
	uint8_t childTwoVoltageLevel;
	int8_t childTwoBiases[15];
	uint8_t childTwoDividerse[15];
	uint64_t childThreeHashrate;
	uint8_t childThreeVoltageLevel;
	int8_t childThreeBiases[15];
	uint8_t childThreeDividerse[15];
	uint64_t childFourHashrate;
	uint8_t childFourVoltageLevel;
	int8_t childFourBiases[15];
	uint8_t childFourDividerse[15];
	uint64_t childFiveHashrate;
	uint8_t childFiveVoltageLevel;
	int8_t childFiveBiases[15];
	uint8_t childFiveDividerse[15];
	uint64_t childSixHashrate;
	uint8_t childSixVoltageLevel;
	int8_t childSixBiases[15];
	uint8_t childSixDividerse[15];

	// Pre-existing control loop state:
    uint8_t boardId;
    double lastTemp;
    double lastTempOnChange;
    uint8_t currDivider;
    int8_t currBias;
    uint32_t ticksSinceLastChange;
    bool lastChangeUp;
    uint16_t printCounter;
    uint64_t masterTickCounter;
    double lastGHS;
    double lastGHSStatistical;
} ControlLoopState;

// Functions for adding/subtracting bias and dividers and formatting
void incrementDivider(ControlLoopState* clState);
void decrementDivider(ControlLoopState* clState);
void incrementBias(ControlLoopState* clState);
void decrementBias(ControlLoopState* clState);
void formatControlLoopState(char* buffer, ControlLoopState* clState);
void formatDividerAndBias(char* buffer, ControlLoopState* clState);
#endif
