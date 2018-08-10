#include <string.h>
#include <stdio.h>
#include "dcrstratum.h"
#include "miner.h"


extern void dump(unsigned char* p, int len, char* label);

/*
typedef struct  {
  uint8_t version[4];
  uint8_t prevHash[32];

  // coinbase 1 covers the fields from here until the end comment
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
  uint8_t nonce[4];  // 108 bytes from merkleRoot to and including nonce
  // The coinbase1 include 4 extra butes at the end which we ignore

  // The next 3 fields make of 32 bytes of "extra data"
  uint8_t extraNonce1[4];
  uint8_t extraNonce2[4];
  uint8_t extraDataPadding[24]; // TODO: VERIFY

  uint8_t stakeVersion[4];
} DecredBlockHeader;*/

// Build a Decred block header that is ready for hashing.
void dcrBuildBlockHeader(DecredBlockHeader* header, struct work* work) {
  struct pool* pool = work->pool;

  // Clear out the struct to all zeroes
  memset(header, 0, sizeof(DecredBlockHeader));

  // Convert from string directly into the version field
  hex2bin((uint8_t*)header->version, (const char*)&pool->bbversion, strlen(pool->bbversion));

  // TODO: Reverse the prev hash?
  memcpy(header->prevHash, work->prev_hash, sizeof(header->prevHash));

  // Copy a bunch of fields from coinbase1 all at once
  memcpy(header->merkleRoot, pool->coinbase1, pool->coinbase1_len - 4);  // We trim off the last 4 bytes as they are unused

  memcpy(header->extraNonce1, pool->nonce1bin, sizeof(header->extraNonce1));
 
  memcpy(header->extraNonce2, &work->nonce2, sizeof(header->extraNonce2));

  // the extraDataPadding remains all zeroes

  // Stake version comes from coinbase2
  memcpy(header->stakeVersion, pool->coinbase2, sizeof(header->stakeVersion));

	dump((uint8_t*)header, sizeof(DecredBlockHeader), "header");
}



/*
[
   "dlux1184",
   "ad2b21457eb90d3c830f7ca5afd853086b79affd1048d8dc0000000100000000",
   "2733b5809b3e3143c6c7468e37d71e5602f002494e728d0472755f613f3e0e056059723ee9ae6ef5b6deee4e61013bc50a43f78a80237bfeb59f1e08c316ef8e010090ea95dac86e05000500b89f00002f1301194d300c4f0200000087070400d01300005b926c5b0000000000000000",
   "05000000",
   [

   ],
   "05000000",
   "0a2f1301",
   "5b6c925b",
   true
]





JobID: dlux1184
PrevHash (Reverse): ad2b21457eb90d3c830f7ca5afd853086b79affd1048d8dc0000000100000000
CB1:     00000000
Version: 05000000

Version Bytes: (4 Bytes)
PrevHash (32 bytes)
CB1 Bytes (112 Bytes)
Extra Data: (24 Bytes)
Extranonce: (extranonce1 + extranonce2)
---------------------


-----------------------------
Block Build
-----------------------------
Version (4 bytes): 05000000
PrevBlock (32 Bytes): ad2b21457eb90d3c830f7ca5afd853086b79affd1048d8dc0000000100000000
MerkleRoot (32 Bytes): 2733b5809b3e3143c6c7468e37d71e5602f002494e728d0472755f613f3e0e05
StakeRoot (32 Bytes): 6059723ee9ae6ef5b6deee4e61013bc50a43f78a80237bfeb59f1e08c316ef8e
VoteBits (2 Bytes): 0100
FinalState (6 Bytes): 90ea95dac86e
Voters (2 bytes): 0500
FreshStake (1 bytes): 05
Revocation (1 bytes): 00
Size of Ticket Pool (4 bytes): b89f0000
Bits (4 bytes): 2f130119
SBits (8 bytes): 4d300c4f02000000
Height (4 bytes): 87070400
Size (4 bytes): d0130000
Timestamp (4 bytes): 5b926c5b
Nonce (4 bytes): 00000000

Unplaced: 00000000
--------------------------
ExtraData
  EN1 (4 Bytes): 0000000
  EN2 (4 Bytes): 0000000
  Padding (24 Bytes): 000000000000000000000000   
StakeVersion (4 bytes): ?

*/