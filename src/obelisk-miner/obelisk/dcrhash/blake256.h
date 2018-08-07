#include <stdint.h>

// Common C implementation of 32-bit bitwise right rotation
#define ROTR32(x, y)	(((x) >> (y)) | ((x) << (32 - (y))))

// Blake-256 constants, taken verbatim from the final
// spec, "SHA-3 proposal BLAKE" version 1.3, published
// on December 16, 2010 - section 2.1.1.
const uint32_t BLAKE256_CONSTS[16];

// Taken from the same document as the above constants,
// these are the initial values for Blake-256, which
// are shared by SHA-256 - found in section 2.1.1.
const uint32_t BLAKE256_IV[8];

// Sigma values - permutations of 0 -15, used in Blake-256.
// Found in the aforementioned spec in section 2.1.2.
const uint8_t BLAKE256_SIGMA[10][16];

// Implementation of Blake-256's round function G - iterations
// of which make up the main compression function. Found in
// section 2.1.2 of the specification.
#define G(a, b, c, d, msg0, msg1, const1, const0)   do { \
		a = a + b + (msg0 ^ const1); \
		d = ROTR32(d ^ a, 16); \
		c = c + d; \
		b = ROTR32(b ^ c, 12); \
		a = a + b + (msg1 ^ const0); \
		d = ROTR32(d ^ a, 8); \
		c = c + d; \
		b = ROTR32(b ^ c, 7); \
	} while (0)

void dcrBlake256CompressBlock(uint32_t *state, const uint32_t *m, const uint32_t t);
