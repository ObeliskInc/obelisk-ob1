// Obelisk ASIC API
#include <stdbool.h>
#include <stdint.h>
#include "CSS_SC1Defines.h"
#include "CSS_DCR1Defines.h"

#ifndef _OB1DEFINES_H_
#define _OB1DEFINES_H_
typedef enum ApiError_ {
    SUCCESS = 0,
    GENERIC_ERROR = -1,
} ApiError;

typedef enum HashboardModel {
    MODEL_SC1 = 0,
    MODEL_DCR1 = 1,
} HashboardModel;

extern HashboardModel gBoardModel;

typedef struct {
    uint64_t m[E_SC1_NUM_MREGS];
} Blake2BJob;

typedef struct {
    uint32_t m[E_DCR1_NUM_MREGS];  // Midstate
    uint32_t v[E_DCR1_NUM_VREGS];  // Header tail
} Blake256Job;

// Note that these unions have no type discriminator.  Instead, at startup, the API init function
// checks to see what type of boards we are dealing with, and saved that globally.
typedef struct {
    union {
        Blake2BJob blake2b;
        Blake256Job blake256;
    };
} Job;

typedef struct {
    bool isPresent; // The other fields are only reliable if isPresent is true
    uint64_t id;
    HashboardModel model;
    int revision;
    int numChains;
    int numChipsPerChain;
} HashboardInfo;

typedef struct {
    ApiError error;
    double boardTemp;
    double chipTemp;
    double powerSupplyTemp;
    double asicV15IO;
    double asicV1;
    double asicV15;
    double powerSupply12V;
} HashboardStatus;

#define MAX_NONCE_FIFO_LENGTH 8
typedef uint64_t Nonce;

typedef struct {
    Nonce nonces[MAX_NONCE_FIFO_LENGTH];
    uint8_t count;
} NonceSet;

#define ALL_BOARDS 255
#define ALL_CHIPS 255
#define ALL_ENGINES 255

#define MIN_BIAS -5
#define NO_BIAS 0
#define MAX_BIAS 5

// Interrupt handler for nonce
// TODO: This is probably going to receive more than one nonce
typedef void (*NonceHandler)(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, Nonce nonce, bool nonceLimitReached);

// The job complete handler is called when the nonce range for a job is exhausted.  This means
// the specified engine(s) need(s) a new job.
//
// The API needs to complete the handshake after calling our handler to start the queued job,
// if any.  This means we need to keep track of whether a job is queued.
//
typedef void (*JobCompleteHandler)(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum);
#endif
