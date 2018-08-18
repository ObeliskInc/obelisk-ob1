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

#define POPULATION_SIZE 3

typedef struct GenChild {
	double fitness; // hashes / second
	uint8_t maxBiasLevel;
	uint8_t initStringIncrements;
	uint8_t voltageLevel;
	// TODO: these don't necessarily match model.chipsPerBoard
	int8_t chipBiases[15];
	uint8_t chipDividers[15];
} GenChild;

typedef struct ControlLoopState {
	// Determine if we need to initialize the control state.
	bool initialized;

	// General status variables.
	uint8_t boardNumber;
	time_t currentTime;
	double currentStringVoltage;
	uint8_t currentVoltageLevel;
	time_t initTime;
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

	// Voltage management variables - algo 2.
	time_t   stringAdjustmentTime;
	uint64_t chipAdjustments;

	// Voltage management variables - genetic algo.
	GenChild population[POPULATION_SIZE];
	uint8_t populationSize;
	GenChild curChild; // same values as currentVoltageLevel, chipBiases, and chipDividers
	bool hasReset;

} ControlLoopState;

// Functions for adding/subtracting bias and dividers and formatting
void incrementDivider(ControlLoopState* clState);
void decrementDivider(ControlLoopState* clState);
void incrementBias(ControlLoopState* clState);
void decrementBias(ControlLoopState* clState);
void formatControlLoopState(char* buffer, ControlLoopState* clState);
void formatDividerAndBias(char* buffer, ControlLoopState* clState);

// Functions for executing the genetic algorithm.
void geneticAlgoIter(ControlLoopState *state);
ApiError saveThermalConfig(char *name, int boardID, ControlLoopState *state);
ApiError loadThermalConfig(char *name, int boardID, ControlLoopState *state);

#endif
