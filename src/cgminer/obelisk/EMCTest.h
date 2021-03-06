#include "miner.h"
#include "Ob1Hashboard.h"
#include "Ob1Utils.h"

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

extern ControlLoopState controlLoopState[MAX_NUMBER_OF_HASH_BOARDS];

#endif