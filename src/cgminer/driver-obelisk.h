//===========================================================================
// Copyright 2018 Obelisk Inc.
//===========================================================================

#include "elist.h"
#include "miner.h"
#include "obelisk/Ob1API.h"
#include "obelisk/Ob1Models.h"
#include "obelisk/Ob1Utils.h"
#include "obelisk/Ob1FanCtrl.h"
#include "obelisk/err_codes.h"

#define MAX_CHAIN_NUM 3
#define NUM_FANS 2

#if (MODEL == SC1)
#define NUM_ENGINES_PER_CHIP 64

// Each engine will be given a range this size
#define NONCE_RANGE_SIZE (4294976296ULL / 4)
// #define NONCE_RANGE_SIZE (4294976296L / NUM_ENGINES_PER_CHIP) // 67,108,864

#define MAX_WQ_SIZE 2
#define WQ_REFILL_SIZE 1

#elif (MODEL == DCR1)
#define NUM_ENGINES_PER_CHIP 128U
#define NONCE_RANGE_SIZE (0xFFFFFFFFULL / 128ULL)

#define MAX_WQ_SIZE 1
#define WQ_REFILL_SIZE 0

#endif

// Each engine can queue up another job
#define MAX_JOBS 2

#define NUM_HASH_JOBS 1000

// Structs to keep track of actual hashrate based on start/end times of
// jobs and nonce_range_size.
typedef struct hashrate_entry {
    struct timespec start;
    struct timespec end;
    uint64_t nonce_range_size;
} hashrate_entry;

typedef struct hashrate_list {
    int next_index;
    struct hashrate_entry jobs[NUM_HASH_JOBS];
} hashrate_list;

struct work_ent {
    struct work* work;
    struct list_head head;
};

struct work_queue {
    int num_elems;
    struct list_head head;
};

// Forward declare the ob_chain
typedef struct ob_chain ob_chain;
typedef struct stringSettings stringSettings;

typedef Job      (*prepareNextChipJobFn)(ob_chain* ob);
typedef ApiError (*setChipNonceRangeFn)(ob_chain* ob, uint16_t chipNum, uint8_t tries);
typedef bool (*areEnginesDoneFn)(ob_chain* ob, uint16_t chipNum);
typedef ApiError (*startNextEngineJobFn)(ob_chain* ob, uint16_t chipNum, uint16_t engineNum);
typedef ApiError (*validNonceFn)(ob_chain* ob, uint16_t chipNum, uint16_t engineNum, Nonce nonce);

// stringSettings contains a list of settings for the string.
//
// TODO: A lot of this is hardcoded, going to have to make this more general.
struct stringSettings {
	// How well this string performed.
	double score;

	// Tracking the number of nonces found during the string's runtime.
	bool     started;
	time_t   startTime;
	time_t   endTime;
	uint64_t startGoodNonces;
	uint64_t endGoodNonces;

	// What the explicit settings of the string were.
	int8_t chipBiases[15];
	uint8_t chipDividers[15];
	uint8_t voltageLevel;
};

// ob_chain is essentially the global state variable for a hashboard. Each
// hashing board has its own ob_chain.
struct ob_chain {
	// Board information.
	hashBoardModel staticBoardModel;
	miningRigModel staticRigModel;
	int            staticBoardNumber;
	int            staticTotalBoards;

	// Static hashing information.
	uint8_t  staticChipTarget[32];           // The target that the chip needs to meet before returning a nonce.
	uint64_t staticHashesPerSuccessfulNonce; // Number of hashes required to find a header meeting the chip target.

	// Work information.
	// 
	// The nonce counters in this struct do not use locking even though they are
	// accessed by multiple threads. That's because only one thread is ever
	// writing to it, the others are reading. And the ones that are reading are
	// only displaying output to a user, so if it's occasionally corrupted,
	// that's not so bad.
	struct work*  bufferedWork;
	bool          bufferWork;
	bool          chipsStarted;
	uint64_t      goodNoncesFound;    // Total number of good nonces found.
	struct work** chipWork;           // The work structures for each chip.
	uint64_t*     chipGoodNonces;     // The good nonce counts for each chip.
	uint64_t*     chipBadNonces;      // The bad nonce counts for each chip.
	uint32_t      decredEN2[15][128]; // ExtraNonce2 for decred chips.

	// Work spacing timers.
	cgtimer_t  iterationStartTime;
	cgtimer_t* chipStartTimes;
	cgtimer_t* chipResetTimes;
	cgtimer_t* chipCheckTimes;

    // Performance timers.
    cgtimer_t startTime;
    int totalScanWorkTime;
    int loadJobTime;
    int obLoadJobTime;
    int obStartJobTime;
    int spiLoadJobTime;
    int submitNonceTime;
    int readNonceTime;
	int doneNonceTime;
	int chipScanTime;
	int statsTime;

	// String setings.
	stringSettings bestSettings[10];
	stringSettings settings;
	uint64_t       stringsTried;

	// Chip specific function pointers.
	prepareNextChipJobFn prepareNextChipJob;
	setChipNonceRangeFn  setChipNonceRange;
	areEnginesDoneFn  areEnginesDone;
	startNextEngineJobFn startNextEngineJob;
	validNonceFn         validNonce;

	// Control loop information.
    int chain_id;
    uint16_t num_chips;
    uint16_t num_cores;
    struct cgpu_info* cgpu;

    // Locking/Notification
    pthread_mutex_t lock;
    pthread_cond_t work_cond;
    pthread_cond_t nonce_cond;

	// Hot temp and fan speed.
	double  hotChipTemp;
	uint8_t fanSpeed;
	uint8_t fanAdjustmentInterval;

    struct work_queue active_wq;

    // Sia has a huge nonce space, so we just keep reusing the same work
    // until the pool forces us to switch.
    // TODO: What triggers the switch?  Just need to call free_work(), then set this to null,
    //       and set start_of_next_nonce to zero in order to cause the scan_work to get a new work
    //       from the queue.
    struct work* curr_work;
    Nonce start_of_next_nonce;

    uint64_t good_nonces_found;
    uint64_t bad_nonces_found;

    ControlLoopState control_loop_state;

    // Stats
    // This number is added to when a job completes, and the scanwork function
    // reads it, clears this and returns the number so that the hashmeter works.
    // Must be modified under the lock.
    Nonce num_hashes;

    // Temperatures and voltages
    // Other temperatures per board
    temp_stats_t board_temp;
    temp_stats_t chip_temp;
    temp_stats_t psu_temp;

    // Fans
    int32_t fan_speed[NUM_FANS];

    // Voltage control
    uint32_t string_voltage;
};
