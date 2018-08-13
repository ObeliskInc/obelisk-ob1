// Sia Header PoW Verification Library.

#include <stdbool.h>

extern bool siaHeaderMeetsMinimumTarget(uint8_t* header);
extern bool siaHeaderMeetsProvidedTarget(uint8_t* header, uint8_t* target);
extern bool siaHeaderMeetsProvidedDifficulty(uint8_t* header, double difficulty);
extern int siaHeaderMeetsChipTargetAndPoolDifficulty(uint8_t header[80], uint8_t chipTarget[32], double poolDifficulty);
