#include <stdbool.h>
#include <string.h>
#include "blake256.h"
#include "dcrverify.h"

// dcrCompressToMidstate will take the header and do the compressions required
// to produce the midstate.
void dcrCompressToMidstate(uint8_t midstate[64], uint8_t header[180]) {
	// Have to typecast the midstate to a uint32_t because the whole blake256
	// library operates off of arrays of 32 bit integers.
	uint32_t* castMidstate = (uint32_t*)midstate;
	uint32_t* castHeader = (uint32_t*)header;
	memcpy(castMidstate, BLAKE256_IV, 32);
	dcrBlake256CompressBlock(castMidstate, castHeader, 0x200);
	dcrBlake256CompressBlock(castMidstate, castHeader + 16, 0x400);
}

// dcrPrepareMidstate will take a 180 byte block header and process it into a
// midstate that can be passed to the ASIC. The ASIC will not be able to
// process a full header.
//
// The input to the ASIC will be the midstate plus the final 52 bytes of the
// header. The nonce appears in bytes 12-16 of the headerTail (which itself is
// the final 52 bytes of the header).
void dcrPrepareMidstate(uint8_t midstate[64], uint8_t header[180]) {
	// Start by swapping the endian-ness.
	uint32_t *swap = (uint32_t*)header;
	int i = 0;
	for(i = 0; i < 45; i++) {
		swap[i] = ((swap[i] >> 24) & 0xff) | ((swap[i] << 8) & 0xff0000 ) | ((swap[i] >> 8) & 0xff00) | ((swap[i] << 24) & 0xff000000);
	}
	dcrCompressToMidstate(midstate, header);
}

// dcrMidstateMeetsMinimumTarget will take a midstate and a headerTail as input,
// and return whether the set of them meet the target. The midstate should be
// the midstate produced by 'dcrPrepareMidstate', and the headerTail should be a
// separate set of 52 bytes that get returned by the ASIC.
//
// This function will not modify the arguments that get passed in.
bool dcrMidstateMeetsMinimumTarget(uint8_t midstate[64], uint8_t headerTail[52]) {
	uint32_t checksum[8];
	memcpy(checksum, midstate, 32);

	// TODO: Unclear whether the bytes that we set in m13 and m15 are bytes that
	// need to be set for every example, or if for some reason they are specific
	// to this example.
	uint32_t m[16];
	memset(m, 0x00, 64);
	memcpy(m, headerTail, 52);
	m[13] = 0x80000001U;
	m[15] = 0x000005A0U;

	dcrBlake256CompressBlock(checksum, m, 0x5A0);
	
	if(checksum[7] == 0) { 
		return true;
	}
	return false;
}

// dcrMidstateMeetsProvidedTarget checks whether the provided midstate and
// header tail hash to the provided target.
bool dcrMidstateMeetsProvidedTarget(uint8_t midstate[64], uint8_t headerTail[52], uint8_t target[32]) {
	uint32_t compressionState[8];
	memcpy(compressionState, midstate, 32);

	// TODO: Unclear whether the bytes that we set in m13 and m15 are bytes that
	// need to be set for every example, or if for some reason they are specific
	// to this example.
	uint32_t m[16];
	memset(m, 0x00, 64);
	memcpy(m, headerTail, 52);
	m[13] = 0x80000001U;
	m[15] = 0x000005A0U;

	dcrBlake256CompressBlock(compressionState, m, 0x5A0);
	uint8_t *checksum = (uint8_t*)compressionState;

	// Do an endianness flip on the checksum.
	int i = 0;
	uint32_t *swap = (uint32_t*)checksum;
	for(i = 0; i < 8; i++) {
		swap[i] = ((swap[i] >> 24) & 0xff) | ((swap[i] << 8) & 0xff0000 ) | ((swap[i] >> 8) & 0xff00) | ((swap[i] << 24) & 0xff000000);
	}

	for(i = 0; i < 32; i++) {
		if(checksum[31-i] > target[i]) {
			return false;
		}
		if(checksum[31-i] < target[i]){
			return true;
		}
	}
	return true;
}

// dcrMidstateMeetsProvidedDifficulty checks whether the provided midstate and
// header tail hash to the provided target.
bool dcrMidstateMeetsProvidedDifficulty(uint8_t midstate[64], uint8_t headerTail[52], double difficulty) {
	uint64_t baseDifficulty = 0xffff000000000000;
	uint64_t adjustedDifficulty = baseDifficulty/difficulty;
	uint8_t target[32] = {0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	int i = 0;
	for( i = 0; i < 8; i++) {
		target[11-i] = adjustedDifficulty % 256;
		adjustedDifficulty /= 256;
	}
	return dcrMidstateMeetsProvidedTarget(midstate, headerTail, target);
}

int dcrHeaderMeetsChipTargetAndPoolDifficulty(uint8_t midstate[64], uint8_t headerTail[52], uint8_t chipTarget[32], double difficulty) {
	// Convert the difficulty into a poolTarget.
	uint64_t baseDifficulty = 0xffff000000000000;
	uint64_t adjustedDifficulty = baseDifficulty/difficulty;
	uint8_t poolTarget[32] = {0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	int i = 0;
	for( i = 0; i < 8; i++) {
		poolTarget[11-i] = adjustedDifficulty % 256;
		adjustedDifficulty /= 256;
	}

	// Get the checksum of midstate and headerTail.
	uint32_t compressionState[8];
	memcpy(compressionState, midstate, 32);
	uint32_t m[16];
	memset(m, 0x00, 64);
	memcpy(m, headerTail, 52);
	m[13] = 0x80000001U;
	m[15] = 0x000005A0U;
	dcrBlake256CompressBlock(compressionState, m, 0x5A0);
	uint8_t *checksum = (uint8_t*)compressionState;

	// Perform an endianness conversion on the checksum.
	uint32_t *swap = (uint32_t*)checksum;
	for(i = 0; i < 8; i++) {
		swap[i] = ((swap[i] >> 24) & 0xff) | ((swap[i] << 8) & 0xff0000 ) | ((swap[i] >> 8) & 0xff00) | ((swap[i] << 24) & 0xff000000);
	}

	// Compare to the pool target.
	for(i = 0; i < 32; i++) {
		if(checksum[31-i] > poolTarget[i]) {
			// Break out to compare to the chip target.
			break;
		}
		if(checksum[31-i] < poolTarget[i]){
			return 2;
		}
	}

	// Compare to the chip target.
	for(i = 0; i < 32; i++) {
		if(checksum[31-i] > chipTarget[i]) {
			return 0;
		}
		if(checksum[31-i] < chipTarget[i]){
			return 1;
		}
	}

	// Header exactly meets the chip target. Unlikely, but we count it.
	return 1;
}
