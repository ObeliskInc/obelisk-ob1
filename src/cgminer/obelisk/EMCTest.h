#include "miner.h"
#include "Ob1Hashboard.h"

#ifndef _HASHJOBINFO_
#define _HASHJOBINFO_
typedef struct HashJobInfo {
    struct timespec start;
    struct timespec end;
    uint64_t totalTime;
    uint64_t nonceCount;
    double hashrateGHS;
} HashJobInfo;

extern HashJobInfo hashJobInfo[MAX_NUMBER_OF_HASH_BOARDS];

typedef struct ControlLoopState {
    uint8_t boardId;
    uint32_t lastTemp;
    uint32_t lastTempOnChange;
    uint8_t currDivider;
    int8_t currBias;
    uint8_t ticksSinceLastChange;
    bool lastChangeUp;
} ControlLoopState;

extern ControlLoopState controlLoopState[MAX_NUMBER_OF_HASH_BOARDS];

#endif