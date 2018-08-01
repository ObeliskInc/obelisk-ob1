//===========================================================================
// Copyright 2018 Obelisk Inc.
//===========================================================================

#include "elist.h"
#include "miner.h"
#include "obelisk/Ob1API.h"

#define MAX_CHAIN_NUM 3
#define NUM_CHIPS_PER_BOARD 15
#define NUM_FANS 2

#if (MODEL == SC1)
#define NUM_ENGINES_PER_CHIP 64

// Each engine will be given a range this size
#define NONCE_RANGE_SIZE (4294976296ULL / 4ULL)
// #define NONCE_RANGE_SIZE (4294976296L / NUM_ENGINES_PER_CHIP) // 67,108,864

#define MAX_WQ_SIZE 2
#define WQ_REFILL_SIZE 1

#elif (MODEL == DCR1)
#define NUM_ENGINES_PER_CHIP 128
#define NONCE_RANGE_SIZE (4294976296ULL / NUM_ENGINES_PER_CHIP) // 33,554,432

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
    // Keep track of where to start the next nonce range when an engine
    // becomes ready.
    Nonce start_of_next_nonce_range;

    // This is the current work to be used to load a new job into an engine.
    struct work* work;

#if (MODEL == SC1)
    // An array of flags to indicate which engines are ready to have new jobs
    // loaded into them.  This is an array rather than queue, to avoid issues
    // where an engine signals ready more than once before the array can be
    // serviced.  We would need to walk the entries to dedupe, which is the
    // same as needing to walk the entries to fill jobs, but at least then it's
    // not done in the interrupt.
    //
    // Initialize this to all true when a new job is received, and then load
    // jobs to all engines.  Then kick off a thread that watches this array
    // and sets up new jobs.  Since all engines will be marked true, it will
    // program the pending job into each engine and mark the flags false immediately.
    // Then, as "done" interrupts occur, flags will be set to true in the array.
    // The thread will lock the array every 50ms and disable interrupts.  It will
    // make a copy of the flags on the stack, and then unlock/re-enable interrupts.
    // Then, outside of locking, it will generate the new jobs and load them
    // to the engines.
    //
    // TODO: Replace with a uint64_t bitmask for 1/8 the space use
    uint8_t ready_engines[NUM_ENGINES_PER_CHIP]; // Used as a bool

    // Keep track of busy engines manually, because reading the EBR doesn't seem to work
    uint64_t busy_engines;

#elif (MODEL == DCR1)
    // Each chip gets a separate job and it splits the 32-bit nonce range across
    // all engines.  This will leave us with no more nonce range to hand out, so
    // we need to have another work somehow.  This means we need to toggle back and
    // forth between work items. Need to keep a pointer for each engine of which
    // work it is using.
    struct work* curr_engine_work[NUM_ENGINES_PER_CHIP];

    // If the start_of_next_nonce_range is exhausted when our job thread goes
    // to make a new nonce range, then it instead calls wq_dequeue() to get a
    // new work, and resets the start_of_next_nonce_range to 0.  This will
    // not block, because the work has already been generated and queued to us.
    // If there is no work for some reason, then we just leave the flag for
    // this engine as true, and let the thread loop come around to it again
    // later, when there is hopefully work for it.

#endif
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

// Obelisk-specific info stored on the cgpu object - use this to keep track
// of the state of the chips and which work items are being handled by
// which chips, among other things.
typedef struct ob_chain {
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

    int good_nonces_found;
    int bad_nonces_found;

    // Pending nonces
    nonce_fifo pending_nonces;

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
} ob_chain;

ApiError push_pending_nonce(ob_chain* ob, int chip_num, int engine_num, Nonce nonce, bool nonce_limit_reached);
ApiError pop_pending_nonce(ob_chain* ob, nonce_info* info);
int num_pending_nonces(ob_chain* ob);
