// Obelisk ASIC API
#include "Ob1Defines.h"

//==================================================================================================
// Chip-level API
//==================================================================================================

// Program a job the specified engine(s).
ApiError ob1LoadJob(int* spiLoadJobTime, uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, Job* pJob);

// Set the upper and lower bounds for the specified engine(s).
ApiError ob1SetNonceRange(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, Nonce lowerBound, Nonce upperBound);

// Set the upper and lower bounds of each engine in each chip in the chain,
// splitting up the provided nonce range based on the given step_size.  Each
// engine will be given the next chunk of the range (e.g., if the lower bound is
// 10 and the subrange_size is 5, the first engine would get 10-14, the second
// would get 15-19, the third would get 20-24, etc.).
//
// This loops over the range using setEngineNonceRange().  Can specify one or
// all boards, and one or all chips.
ApiError ob1SpreadNonceRange(uint8_t boardNum, uint8_t chipNum, Nonce lowerBound, Nonce subrangeSize, Nonce* pNextNonce);

// Read all the nonces for a given engine (up to 8)
ApiError ob1ReadNonces(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, NonceSet* nonceSet);

// Get the bits corresponding to each engine's DONE status
ApiError ob1GetDoneEngines(uint8_t boardNum, uint8_t chipNum, uint64_t* pData);

// Get the bits corresponding to each engine's BUSY status
ApiError ob1GetBusyEngines(uint8_t boardNum, uint8_t chipNum, uint64_t* pData);

// Start the job and wait for the engine(s) to indicate busy.
ApiError ob1StartJob(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum);

// Stop the specified engine(s) from running (turns off the clock).
ApiError ob1StopChip(uint8_t boardNum, uint8_t chipNum);

// Register a function that will be called when a nonce is found.  The system
// will be interrupt driven, so the provided handler function will be called
// from an interrupt.  This means the handler should be short and quick.
ApiError ob1RegisterNonceHandler(NonceHandler handler);

// Register a function that will be called when a job exhausts the assigned
// nonce search space.  The system will be interrupt driven, so the provided
// handler function will be called from an interrupt.  This means the handler
// should be short and quick.
ApiError ob1RegisterJobCompleteHandler(JobCompleteHandler handler);
// TODO: What does the handler get called with?

// Set the frequency bias for the specified engines.
//
// The bias must be a value between -5 and +5
ApiError ob1SetClockBias(uint8_t boardNum, uint8_t chipNum, int8_t bias);

// Set the clock divider for the specified engine(s).
//
// The divider must be a value of 1, 2, 4 or 8.
ApiError ob1SetClockDivider(uint8_t boardNum, uint8_t chipNum, uint8_t divider);

// Set the clock divider for the specified engine(s).
//
// The divider must be a value of 1, 2, 4 or 8.
ApiError ob1SetClockDividerAndBias(uint8_t boardNum, uint8_t chipNum, uint8_t divider, int8_t bias);

void ob1EnableMasterHashClock(uint8_t boardNum, bool isEnabled);

bool ob1IsMasterHashClockEnabled(uint8_t boardNum);

//==================================================================================================
// Miner/Board-level API
//==================================================================================================

// Run a basic initialization process for the system.
// Checks that each board is working and are all of the same type and saves the type into
// gHashboardModel (HashboardModel::SC1 or HashboardModel::DCR1).
// If an error is returned, do not start the system any further (e.g., different board models
// installed, or fatal error of some sort).
ApiError ob1Initialize();

// If there is an error reading any of the values, the returned object's error
// field will contain an error code.
HashboardStatus ob1GetHashboardStatus(uint8_t boardNum);

// Valid values for voltage range from 8.5 to 10.0.  Since voltage is actually
// controlled in a discrete, stepwise fashion, the code will select the
// nearest value without going over the specified voltage.
// Values for voltage are about 12 to 127, and 127 is the lowest voltage.
ApiError ob1SetStringVoltage(uint8_t boardNum, uint8_t voltage);

// Read the Done status of the ASICs for one of the boards.  Returned value reflects
// which ASICs on the board are signaling they are done (corresponding bit is 1)
ApiError ob1ReadBoardDoneFlags(uint8_t boardNum, uint16_t* pValue);

// Read the Nonce status of the ASICs for one of the boards.  Returned value reflects
// which ASICs on the board are signaling they have a Nonce (corresponding bit is 1).
ApiError ob1ReadBoardNonceFlags(uint8_t boardNum, uint16_t* pValue);

//==================================================================================================
// Controller-level API
//==================================================================================================

// Returns the number of hashboard slots in the miner.  Hardcoded to 3 for now.
uint8_t ob1GetNumHashboardSlots();

// Get the number of present hashboard slots
uint8_t ob1GetNumPresentHashboards();

// Read the board info for the specified slot_num.  If there is an error or the
// board is not present, then the isPresent field will be set to false.
HashboardInfo ob1GetHashboardInfo(uint8_t boardNum);

uint8_t ob1GetNumEnginesPerChip();

HashboardModel ob1GetHashboardModel();

// Return the number of fans in the units
uint8_t ob1GetNumFans();

// Returns the fan speed in rpm
uint32_t ob1GetFanRPM(uint8_t fanNum);

// The percent can be 10-100
ApiError ob1SetFanSpeed(uint8_t fanNum, uint8_t percent);

//==================================================================================================
// Diagnostics API
//==================================================================================================

// Read the specified TDR register
uint64_t ob1ReadTDR(uint8_t regNum);
