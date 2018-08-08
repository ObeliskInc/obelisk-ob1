#include <string.h>
#include <stdio.h>
#include "blake2.h"
#include "siastratum.h"
#include "miner.h"


void dump(unsigned char* p, int len, char* label) {
	// HACK: START
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

	dump(arbTx, 121, "arbTx After ExtraNonce1");
	memcpy(arbTx + offset, input.ExtraNonce2, input.ExtraNonce2Size);
	offset += input.ExtraNonce2Size;

	dump(arbTx, 121, "arbTx After ExtraNonce2");

  applog(LOG_ERR, "DBG99:  input.ExtraNonce2=%02X%02X%02X%02X", input.ExtraNonce2[0], input.ExtraNonce2[1], input.ExtraNonce2[2], input.ExtraNonce2[3]);

	// coinbase2
	memcpy(arbTx + offset, input.Coinbase2, input.Coinbase2Size);
	offset += input.Coinbase2Size;
	dump(arbTx, 121, "arbTx After Coinbase2");



	// KEN
	// char* arb_tx_str = bin2hex(arbTx, offset);
	// applog(LOG_ERR, "arb_tx = %02X %02X...%02X %02X   %s", arbTx[0], arbTx[1], arbTx[119], arbTx[120], arb_tx_str);
	// free(arb_tx_str);

  char* arbStr ="00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010000000000000030000000000000004E6F6E536961000000000000000000004C55584F5200005349503100007CBD82080000000000000030000003000000000000000000000000";
//              "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010000000000000030000000000000004e6f6e536961000000000000000000004c55584f5200005349503100007cbd82080000000000000030000003000000000000000000000000"
//              "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010000000000000030000000000000004E6F6E536961000000000000000000004C55584F5200005349503100007CBD82080000000000000030000040000000000000000000000000"
	unsigned char cmp_arb_str[256];
	bin2hex(arbTx, offset);
	if (strcmp(arbStr, cmp_arb_str) == 0) {
		applog(LOG_ERR, "Strings are the SAME!");
	} else {
		applog(LOG_ERR, "Strings are DIFFERENT!!!!!!!!!!!!!!!!!!!!!!!!!!!: len1=%d,  len2=%d", strlen(arbTx), strlen(cmp_arb_str));
	}

	unsigned char arbBin[256];
	hex2bin(arbBin, arbStr, strlen(arbStr));
	uint8_t checksum2[32];
	blake2b(checksum2, 32, arbBin, strlen(arbStr)/2, NULL, 0);

	char buffer[1024];
	int buffOffset = 0;
	for (int j = 0; j < offset; j++) {
	    sprintf(buffer + buffOffset, "%02X", checksum2[j]);
	    buffOffset += 2;
	    if (j > 0 && j % 80 == 0 || j == offset - 1) {
	        buffer[buffOffset] = 0;
	        buffOffset = 0;
	        applog(LOG_ERR, "offset=%d  checksum2=%s", offset, buffer);
	    }
	}

	uint8_t checksum3[32];
	blake2b(checksum3, 32, arbBin, strlen(arbStr)/2, NULL, 0);

	for (int j = 0; j < offset; j++) {
	    sprintf(buffer + buffOffset, "%02X", checksum3[j]);
	    buffOffset += 2;
	    if (j > 0 && j % 80 == 0 || j == offset - 1) {
	        buffer[buffOffset] = 0;
	        buffOffset = 0;
	        applog(LOG_ERR, "offset=%d  checksum3=%s", offset, buffer);
	    }
	}


	// Get the base merkle root by hashing the arbTx.
	uint8_t checksum[32];
	blake2b(checksum, 32, arbTx, offset, NULL, 0);

	for (int j = 0; j < offset; j++) {
	    sprintf(buffer + buffOffset, "%02X", checksum[j]);
	    buffOffset += 2;
	    if (j > 0 && j % 80 == 0 || j == offset - 1) {
	        buffer[buffOffset] = 0;
	        buffOffset = 0;
	        applog(LOG_ERR, "offset=%d  checksum=%s", offset, buffer);
	    }
	}


	// Dump the merkles
	for (int i=0; i< input.MerkleBranchesLen; i++) {
		dump(input.MerkleBranches[i], HASH_SIZE, "merkle");
	}

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
}
