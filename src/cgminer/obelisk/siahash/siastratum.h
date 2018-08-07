#include <stdint.h>

// SiaStratumInput defines a list of inputs from a stratum server that can be
// turned into a Sia header.
typedef struct SiaStratumInput {
	// Block header information.
	uint8_t PrevHash[32];
	uint8_t Nonce[8];
	uint8_t Ntime[8];

	// Merkle branches inputs.
	uint16_t MerkleBranchesLen;
	uint8_t** MerkleBranches[20][32];

	// Coinbase inputs.
	uint16_t Coinbase1Size;
	uint16_t Coinbase2Size;
	uint8_t Coinbase1[256];
	uint8_t Coinbase2[256];

	// Extra nonce inputs.
	uint16_t ExtraNonce1Size;
	uint16_t ExtraNonce2Size;
	uint8_t ExtraNonce1[256];
	uint8_t ExtraNonce2[256];
} SiaStratumInput;

void siaCalculateStratumHeader(uint8_t* header, SiaStratumInput input);
