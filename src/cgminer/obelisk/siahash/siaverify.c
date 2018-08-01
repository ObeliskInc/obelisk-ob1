// Verify a sia nonce.

#include <stdio.h>
#include "blake2.h"
#include "blake2-impl.h"
#include "siaverify.h"

// isValidSiaHeader checks whether the provided header is a header that meets
// the minimum target. In this case, the minimum target is 32 leading zeroes.
// The primary purpose of this function is to verify that the ASIC did not make
// a mistake in finding a special header. If the first 32 bits are all '0', then
// the ASIC did not make a mistake and this header is ready to be passed to a
// higher level function.
bool siaHeaderMeetsMinimumTarget(uint8_t header[80]) {
	uint8_t checksum[32];
	blake2b(checksum, 32, header, 80, NULL, 0);

	int i = 0;
	for(i = 0; i < 4; i++) {
		if(checksum[i] != 0) {
			return false;
		}
	}
	return true;
}

// siaHeaderMeetsProvidedTarget will check that the hash of the header meets the
// provided target, generally this will be the target set by the pool.
bool siaHeaderMeetsProvidedTarget(uint8_t header[80], uint8_t target[32]) {
	uint8_t checksum[32];
	blake2b(checksum, 32, header, 80, NULL, 0);

	int i = 0;
	for(i = 0; i < 32; i++) {
		if(checksum[i] > target[i]) {
			return false;
		}
		if(checksum[i] < target[i]){
			return true;
		}
	}
	return true;
}

// siaHeaderMeetsProvidedDifficulty will check that the hash of the header meets
// the provided difficulty, generally this will be the target set by the pool.
// 
// NOTE: The difficulty that is provided is treated as a STRATUM difficulty,
// which is different from a Sia difficulty.
bool siaHeaderMeetsProvidedDifficulty(uint8_t header[80], double difficulty) {
	uint64_t baseDifficulty = 0xffff000000000000;
	uint64_t adjustedDifficulty = baseDifficulty/difficulty;
	uint8_t target[32] = {0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	int i = 0;
	for( i = 0; i < 8; i++) {
		target[11-i] = adjustedDifficulty % 256;
		adjustedDifficulty /= 256;
	}
	return siaHeaderMeetsProvidedTarget(header, target);
}
