#include <stdint.h>
#include "miner.h"

// DecredStratumInput defines a list of inputs from a stratum server that can be
// turned into a Decred header.
typedef struct  {
  uint8_t version[4];
  uint8_t prevHash[32];
  uint8_t merkleRoot[32];
  uint8_t stakeRoot[32];
  uint8_t voteBits[2];
  uint8_t finalState[6];
  uint8_t voters[2];
  uint8_t freshStake[1];
  uint8_t revocations[1];
  uint8_t poolSize[4];
  uint8_t bits[4];
  uint8_t sBits[8];
  uint8_t height[4];
  uint8_t size[4];
  uint8_t timestamp[4];
  uint8_t nonce[4];

  // The next 3 fields make of 32 bytes of "extra data"
  uint8_t extraNonce1[4];
  uint8_t extraNonce2[4];
  uint8_t extraDataPadding[24]; // TODO: VERIFY

  uint8_t stakeVersion[4];
} DecredBlockHeader;

void dcrBuildBlockHeader(DecredBlockHeader* header, struct work* work);
