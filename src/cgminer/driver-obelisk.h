//===========================================================================
// Copyright 2018 Obelisk Inc.
//===========================================================================

#include "elist.h"
#include "miner.h"
#include "obelisk/Ob1API.h"
#include "obelisk/Ob1Models.h"
#include "obelisk/Ob1Utils.h"
#include "obelisk/persist/persist.h"
#include "obelisk/err_codes.h"

#define MAX_CHAIN_NUM 3
#define NUM_CHIPS_PER_BOARD 15
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

#define MAX_WQ_SIZE (NUM_CHIPS_PER_BOARD * 2)
#define WQ_REFILL_SIZE (MAX_WQ_SIZE / 2)
#endif

// Each engine can queue up another job
#define MAX_JOBS 2

#define NUM_HASH_JOBS 1000

/*
Sia: Initial state
- All chips and engines are available to accept work.
- The same work can be split across all chips and engines
- chip_info for Sia will keep track of start_of_next_nonce_range, which starts at 0
- Load same job into all engines, then load individual ranges to engines one
  at a time

Interrupt handler for "done" will mark the chip as ready for work and then clear
the interrupt.  The engine should then start on the internally queued job if one
was setup already.

Need to lock access to the SPI if we have three cgpu threads.

Q: How will the interrupt know which card it is for?
*/

typedef struct chip_info {
#if (MODEL == SC1)
    // Keep track of busy engines manually, because reading the EBR doesn't seem to work
    uint64_t busy_engines;
#elif (MODEL == DCR1)
    // Keep track of busy engines manually, because reading the EBR doesn't seem to work
    uint64_t busy_engines[2];
#endif
    // Dynamically allocated array of work pointers (64 for sia, 128 for decred)
    struct work** engines_curr_work;

} chip_info;

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

#define MAX_PENDING_NONCES (8 * NUM_ENGINES_PER_CHIP)

typedef struct nonce_info {
    Nonce nonce;
    int chip_num;
    int engine_num;
    bool nonce_limit_reached;
} nonce_info;

typedef struct nonce_fifo {
    int head; // Read from the head
    int tail; // Write to the tail
    nonce_info nonces[MAX_PENDING_NONCES];
} nonce_fifo;

// Forward declare the ob_chain
typedef struct ob_chain ob_chain;
typedef struct stringSettings stringSettings;

typedef Job (*prepareNextChipJobFn)(ob_chain* ob, uint8_t chipIndex);
typedef ApiError (*validNonceFn)(ob_chain* ob, struct work* engine_work, Nonce nonce);

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
	uint64_t      goodNoncesFound; // Total number of good nonces found.
	struct work** chipWork;        // The work structures for each chip.
	struct work** nextChipWork;    // The next work structures for each chip.
	uint64_t*     chipGoodNonces;  // The good nonce counts for each chip.
	uint64_t*     chipBadNonces;   // The bad nonce counts for each chip.

    // Timers
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
	validNonceFn      validNonce;

	// Control loop information.
    int chain_id;
    uint16_t num_chips;
    uint16_t num_cores;
    struct cgpu_info* cgpu;
    struct chip_info chips[NUM_CHIPS_PER_BOARD];

    // Locking/Notification
    pthread_mutex_t lock;
    pthread_cond_t work_cond;
    pthread_cond_t nonce_cond;

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

    // Pending nonces
    nonce_fifo pending_nonces;

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

    // Configs for individual ASICs on the board
    chip_config_t chip_config[NUM_CHIPS_PER_BOARD];
};

ApiError push_pending_nonce(ob_chain* ob, int chip_num, int engine_num, Nonce nonce, bool nonce_limit_reached);
ApiError pop_pending_nonce(ob_chain* ob, nonce_info* info);
int num_pending_nonces(ob_chain* ob);


void set_engine_busy(ob_chain* ob, int chip_num, int engine_num, bool is_busy);

bool is_engine_busy(ob_chain* ob, int chip_num, int engine_num);

void add_hashes(ob_chain* ob, uint64_t num_hashes);

void add_good_nonces(ob_chain* ob, uint64_t amt);

void add_bad_nonces(ob_chain* ob, uint64_t amt);

uint64_t get_and_reset_hashes(ob_chain* ob);

uint64_t get_and_reset_good_nonces(ob_chain* ob);

uint64_t get_and_reset_bad_nonces(ob_chain* ob);

uint64_t get_num_hashes(ob_chain* ob);

uint64_t get_good_nonces(ob_chain* ob);

uint64_t get_bad_nonces(ob_chain* ob);
