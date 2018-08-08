#include <string.h>
#include <stdio.h>
#include "blake2.h"
#include "siastratum.h"

// calculateHeader will compute a Sia block header that is ready for grinding
// from the provided merkle branches, coinbases, and extra nonces. The output
// will be written to the first 80 bytes of 'header'.
void siaCalculateStratumHeader(uint8_t* header, SiaStratumInput input) {
	// Start by building the arbitrary transaction. Use a buffer that's
	// guaranteed to be long enough to hold the whole arbtx.
	uint8_t arbTx[256];
	uint16_t offset = 0;

	// Leading 0-byte.
	arbTx[0] = 0;
	offset++;
	// coinbase1
	memcpy(arbTx + offset, input.Coinbase1, input.Coinbase1Size);
	offset += input.Coinbase1Size;
	// extraNonce1 + 2
	memcpy(arbTx + offset, input.ExtraNonce1, input.ExtraNonce1Size);
	offset += input.ExtraNonce1Size;
	memcpy(arbTx + offset, input.ExtraNonce2, input.ExtraNonce2Size);
	offset += input.ExtraNonce2Size;
	// coinbase2
	memcpy(arbTx + offset, input.Coinbase2, input.Coinbase2Size);
	offset += input.Coinbase2Size;

	// Get the base merkle root by hashing the arbTx.
	uint8_t checksum[32];
	blake2b(checksum, 32, arbTx, offset, NULL, 0);

	// Use the arbtx and markleBranch to build the root.
	int i = 0;
	for (i = 0; i < input.MerkleBranchesLen; i++) {
		// Build the 65 byte merkle group - [1, checksum, merkleBranches[i]]
		uint8_t merkleGroup[65];
		merkleGroup[0] = 1;
		memcpy(merkleGroup+1, input.MerkleBranches[i], 32);
		memcpy(merkleGroup+33, checksum, 32);
		blake2b(checksum, 32, merkleGroup, 65, NULL, 0);
	}

	// Create the final header.
	memcpy(header, input.PrevHash, 32);
	memcpy(header+32, input.Nonce, 8);
	memcpy(header+40, input.Ntime, 8);
	memcpy(header+48, checksum, 32);
}
