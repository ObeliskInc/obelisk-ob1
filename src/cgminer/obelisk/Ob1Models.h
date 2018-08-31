// hashBoardModel defines a few of the physical, unchanging parameters of a
// hashing board that is compatible with the obelisk mining software.
typedef struct hashBoardModel {
	char name[16];

	// General string information.
	uint16_t chipsPerBoard;
	uint16_t enginesPerChip;
	uint16_t minStringVoltageLevel;
	uint16_t maxStringVoltageLevel;

	// Information for working with block headers for this algorithm.
	uint64_t chipDifficulty;
	uint64_t headerSize;
	uint64_t headerTailSize;
	uint64_t midstateSize;
	uint64_t nonceOffset;
	uint64_t nonceOffsetInTail;
	uint64_t extranonce2Offset;
	uint64_t extranonce2OffsetInTail;

	// Thermal information.
	uint64_t boardHotTemp;
	uint64_t boardIdealTemp;
	uint64_t boardCoolTemp;
	uint64_t chipTempVariance;
	uint64_t hotChipTargetTemp;

	// Suggested parameters to use when operating the chips.
	uint64_t chipSpeed; // A chip running below this speed indicates a problem.
	uint8_t  defaultMaxBiasLevel;
	uint8_t  defaultStringIncrements;
	uint64_t nonceRange;
} hashBoardModel;

// miningRigModel defines a few of the physical, unchanging parameters of a
// mining rig that are useful when operating the unit.
typedef struct miningRigModel {
	uint64_t fanSpeedIncrement;
	uint64_t fanSpeedMax;
	uint64_t fanSpeedMin;
} miningRigModel;

// HASHBOARD_MODEL_SC1A defines the parameters for our SC1A hashing card.
const struct hashBoardModel HASHBOARD_MODEL_SC1A = {
	.name                  = "SC1A",
	.chipsPerBoard         = 15,
	.enginesPerChip        = 64,
	.minStringVoltageLevel = 18,
	.maxStringVoltageLevel = 127,

	.chipDifficulty    = 1099511627776ULL,
	.headerSize        = 80,
	.headerTailSize    = 0,  // no tail
	.midstateSize      = 0,  // no midstate
	.nonceOffset       = 32,
	.nonceOffsetInTail = 0,  // no tail
	.extranonce2Offset = 0,  // Not used
	.extranonce2OffsetInTail = 0, // no tail

	.boardHotTemp      = 99,
	.boardIdealTemp    = 95,
	.boardCoolTemp     = 90,
	.chipTempVariance  = 5,
	.hotChipTargetTemp = 105,

	.chipSpeed               = 100000000ULL, // 100 MHz
	.defaultMaxBiasLevel     = 22,           // Corresponds to a /2.-4
	.defaultStringIncrements = 16,
	.nonceRange              = 4294967296ULL // 2^32
};

// HASHBOARD_MODEL_DCR1A defines the parameters for our DCR1A hashing card.
const struct hashBoardModel HASHBOARD_MODEL_DCR1A = {
	.name                  = "DCR1A",
	.chipsPerBoard         = 15,
	.enginesPerChip        = 128,
	.minStringVoltageLevel = 18,
	.maxStringVoltageLevel = 127,

	.chipDifficulty    = 4294976296ULL,
	.headerSize        = 180,
	.headerTailSize    = 52,
	.midstateSize      = 32,
	.nonceOffset       = 140,
	.nonceOffsetInTail = 12,
	.extranonce2Offset = 148,
	.extranonce2OffsetInTail = 20,

	.boardHotTemp      = 99,
	.boardIdealTemp    = 95,
	.boardCoolTemp     = 90,
	.chipTempVariance  = 5,
	.hotChipTargetTemp = 105,

	.chipSpeed               = 2000000ULL, // 2 MHz - this will be increased as we optimize the SPI
	.defaultMaxBiasLevel     = 22,         // Corresponds to a /2.-4
	.defaultStringIncrements = 16,
	.nonceRange              = 4294967296ULL // 2^32
};

// MINING_RIG_MODEL_OB1 defines the parameters for our OB1 mining rig.
const struct miningRigModel MINING_RIG_MODEL_OB1 = {
	.fanSpeedIncrement = 5,
	.fanSpeedMax       = 100,
	.fanSpeedMin       = 10
};

// Temperature models according to thermal sims of our boards.
const int64_t OB1_TEMPS_HASHBOARD_2_0[15] = {   0,  12,  -6, -16,  -9,   4, -16, -19, -21, -11, -23, -12, -15,  -1,  -6 };
const int64_t OB1_TEMPS_HASHBOARD_2_1[15] = {   0,   8,   1,  -8,  -8,  -2,  -7, -11, -15, -12, -16, -10, -11,  -1,  -5 };
const int64_t OB1_TEMPS_HASHBOARD_3_0[15] = {   0,  -7, -10, -16, -12, -19, -19,  -9, -28, -27, -30, -28, -22, -18, -13 };
const int64_t OB1_TEMPS_HASHBOARD_3_1[15] = {   0,   6,  16, -16,  -9,   0,   9, -21, -20,  -3, -23, -13, -18,  -3, -12 };
const int64_t OB1_TEMPS_HASHBOARD_3_2[15] = {   0,   1,   5, -16,  -9,  -5,  -4, -19, -22, -14, -27, -19, -20,  -9, -14 };
