#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "blake256.h"

// Blake-256 constants, taken verbatim from the final
// spec, "SHA-3 proposal BLAKE" version 1.3, published
// on December 16, 2010 - section 2.1.1.
const uint32_t BLAKE256_CONSTS[16] = {
	0x243F6A88U, 0x85A308D3U, 0x13198A2EU, 0x03707344U,
	0xA4093822U, 0x299F31D0U, 0x082EFA98U, 0xEC4E6C89U,
	0x452821E6U, 0x38D01377U, 0xBE5466CFU, 0x34E90C6CU,
	0xC0AC29B7U, 0xC97C50DDU, 0x3F84D5B5U, 0xB5470917U
};

// Taken from the same document as the above constants,
// these are the initial values for Blake-256, which
// are shared by SHA-256 - found in section 2.1.1.
const uint32_t BLAKE256_IV[8] = {
	0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
	0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U
};

// Sigma values - permutations of 0 -15, used in Blake-256.
// Found in the aforementioned spec in section 2.1.2.
const uint8_t BLAKE256_SIGMA[10][16] = {
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
	{ 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
	{  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
	{  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
	{  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
	{ 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
	{ 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
	{  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
	{ 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13 , 0 }
};

void dcrBlake256CompressBlock(uint32_t *state, const uint32_t *m, const uint32_t t) {
	uint32_t v[16];
	
	memcpy(v, state, 32);
	memcpy(v + 8, BLAKE256_CONSTS, 32);
	
	v[12] ^= t;
	v[13] ^= t;
	
	for(int i = 0; i < 14; ++i) {
		uint32_t rnd = (i > 9) ? i - 10 : i;
		
		G(v[0], v[4], v[8], v[12], m[BLAKE256_SIGMA[rnd][0]], m[BLAKE256_SIGMA[rnd][1]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][1]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][0]]);
		G(v[1], v[5], v[9], v[13], m[BLAKE256_SIGMA[rnd][2]], m[BLAKE256_SIGMA[rnd][3]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][3]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][2]]);
		G(v[2], v[6], v[10], v[14], m[BLAKE256_SIGMA[rnd][4]], m[BLAKE256_SIGMA[rnd][5]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][5]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][4]]);
		G(v[3], v[7], v[11], v[15], m[BLAKE256_SIGMA[rnd][6]], m[BLAKE256_SIGMA[rnd][7]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][7]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][6]]);
		
		G(v[0], v[5], v[10], v[15], m[BLAKE256_SIGMA[rnd][8]], m[BLAKE256_SIGMA[rnd][9]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][9]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][8]]);
		G(v[1], v[6], v[11], v[12], m[BLAKE256_SIGMA[rnd][10]], m[BLAKE256_SIGMA[rnd][11]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][11]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][10]]);
		G(v[2], v[7], v[8], v[13], m[BLAKE256_SIGMA[rnd][12]], m[BLAKE256_SIGMA[rnd][13]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][13]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][12]]);
		G(v[3], v[4], v[9], v[14], m[BLAKE256_SIGMA[rnd][14]], m[BLAKE256_SIGMA[rnd][15]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][15]], BLAKE256_CONSTS[BLAKE256_SIGMA[rnd][14]]);
	}
	
	for(int i = 0; i < 8; ++i) state[i] ^= v[i] ^ v[i + 8];
}
