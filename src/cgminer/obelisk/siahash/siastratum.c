#include <string.h>
#include <stdio.h>
#include "blake2.h"
#include "siastratum.h"
#include "miner.h"


void dump(unsigned char* p, int len, char* label) {
	char buffer[1024];
	int buffOffset = 0;
	for (int j = 0; j < len; j++) {
	    sprintf(buffer + buffOffset, "%02X", p[j]);
	    buffOffset += 2;
	    if (j > 0 && j % 80 == 0 || j == len - 1) {
	        buffer[buffOffset] = 0;
	        buffOffset = 0;
	        applog(LOG_ERR, "len=%d  %s=%s", len, label, buffer);
	    }
	}
}

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

	// applog(LOG_ERR, "DBG99:  input.ExtraNonce1=%02X%02X%02X%02X", input.ExtraNonce1[0], input.ExtraNonce1[1], input.ExtraNonce1[2], input.ExtraNonce1[3]);
	// applog(LOG_ERR, "DBG99:  input.ExtraNonce2=%02X%02X%02X%02X", input.ExtraNonce2[0], input.ExtraNonce2[1], input.ExtraNonce2[2], input.ExtraNonce2[3]);

	// coinbase2
	memcpy(arbTx + offset, input.Coinbase2, input.Coinbase2Size);
	offset += input.Coinbase2Size;

	// Get the base merkle root by hashing the arbTx.
	uint8_t checksum[32];
	blake2b(checksum, 32, arbTx, offset, NULL, 0);

	// Use the arbtx and markleBranch to build the root.
	int i = 0;
	for (i = 0; i < input.MerkleBranchesLen; i++) {
		// Build the 65 byte merkle group - [1, merkleBranches[i], checksum]
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

	// dump(header, SIA_HEADER_SIZE, "header");
}
