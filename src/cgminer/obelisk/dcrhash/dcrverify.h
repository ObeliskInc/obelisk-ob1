#include <stdbool.h>

// dcrCompressToMidstate takes a header that is already in little-endian. Most
// pools will not provide a header in little-endian. If you have a big-endian
// header, you can call 'dcrPrepareMidstate' to get the full correct result.
void dcrCompressToMidstate(uint8_t midstate[64], uint8_t header[180]);

// dcrPrepareMidstate will take a 180 byte block header and process it into a
// midstate that can be passed to the ASIC. The ASIC will not be able to
// process a full header.
//
// The input to the ASIC will be the midstate plus the final 52 bytes of the
// header. The nonce appears in bytes 12-16 of the headerTail (which itself is
// the final 52 bytes of the header).
// 
// The dcrPrepareMidstate function is expecting a header that is big-endian. The
// header will be converted to little-endian and then compressed. Most mining
// pools will provide the header in big-endian format.
void dcrPrepareMidstate(uint8_t midstate[64], uint8_t header[180]);

// dcrMidstateMeetsMinimumTarget will take a midstate and a headerTail as input,
// and return whether the set of them meet the target. The midstate should be
// the midstate produced by 'dcrPrepareMidstate', and the headerTail should be a
// separate set of 52 bytes that get returned by the ASIC.
//
// This function will not modify the arguments that get passed in.
bool dcrMidstateMeetsMinimumTarget(uint8_t midstate[64], uint8_t headerTail[52]);

// dcrMidstateMeetsProvidedTarget checks whether the provided midstate and
// header tail hash to the provided target.
bool dcrMidstateMeetsProvidedTarget(uint8_t midstate[64], uint8_t headerTail[52], uint8_t target[32]);

// dcrMidstateMeetsProvidedDifficulty checks whether the provided midstate and
// header tail hash to the provided target.
bool dcrMidstateMeetsProvidedDifficulty(uint8_t midstate[64], uint8_t headerTail[52], double difficulty);
