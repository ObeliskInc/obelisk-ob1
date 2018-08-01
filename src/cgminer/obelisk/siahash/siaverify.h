// Sia Header PoW Verification Library.

#include <stdbool.h>

extern bool siaHeaderMeetsMinimumTarget(uint8_t header[80]);
extern bool siaHeaderMeetsProvidedTarget(uint8_t header[80], uint8_t target[32]);
extern bool siaHeaderMeetsProvidedDifficulty(uint8_t header[80], double difficulty);
