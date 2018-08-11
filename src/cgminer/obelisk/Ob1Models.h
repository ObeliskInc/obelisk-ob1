// hashBoardModel defines a few of the physical, unchanging parameters of a
// hashing board that is compatible with the obelisk mining software.
typedef struct hashBoardModel {
	// General string information.
	uint16_t chipsPerBoard;
	uint16_t enginesPerChip;
	uint16_t minStringVoltageLevel;
	uint16_t maxStringVoltageLevel;
} hashBoardModel;

// HASHBOARD_MODEL_SC1A defines the parameters for our SC1A hashing card.
const struct hashBoardModel HASHBOARD_MODEL_SC1A = {
	.chipsPerBoard = 15,
	.enginesPerChip = 64,
	.minStringVoltageLevel = 20,
	.maxStringVoltageLevel = 127
};
