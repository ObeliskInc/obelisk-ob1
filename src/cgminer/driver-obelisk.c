//===========================================================================
// Copyright 2018 Obelisk Inc.
//===========================================================================

/*
cgminer command lines for various pools:

./cgminer --url us-east.luxor.tech:3333 --user 79cc3a0572619864e6a45b0aa8c2294db3d27151ff88ed7e066e33ee545841c13d4797297324.obelisk --pass x --api-listen --api-allow W:127.0.0.1 --syslog

./cgminer --url us-east.siamining.com:3333 --user 79cc3a0572619864e6a45b0aa8c2294db3d27151ff88ed7e066e33ee545841c13d4797297324.obelisk --pass x --api-listen --api-allow W:127.0.0.1 --syslog

./cgminer --url us-east.toastpool.com:3333 --user 79cc3a0572619864e6a45b0aa8c2294db3d27151ff88ed7e066e33ee545841c13d4797297324.obelisk --pass x --api-listen --api-allow W:127.0.0.1 --syslog

./cgminer --url sc.f2pool.com:7778 --user 79cc3a0572619864e6a45b0aa8c2294db3d27151ff88ed7e066e33ee545841c13d4797297324.obelisk --pass x --api-listen --api-allow W:127.0.0.1 --syslog

Add more logging:
./cgminer --url us-west.luxor.tech:3333 --user 79cc3a0572619864e6a45b0aa8c2294db3d27151ff88ed7e066e33ee545841c13d4797297324.obelisk --pass x --api-listen --api-allow W:127.0.0.1 --log 4 --protocol-dump --syslog

*/

#include "miner.h"
#include "driver-obelisk.h"
#include "compat.h"
#include "config.h"
#include "klist.h"
#include "sha2.h"
#include "obelisk/siahash/siaverify.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#if (ALGO == BLAKE2B)
#include "obelisk/blake2.h"
#endif

static int num_chains = 0;
static ob_chain chains[MAX_CHAIN_NUM];

#if (MODEL == SC1)
Nonce SiaNonceLowerBound = 0;
Nonce SigNonceUpperBound = 0xFFFFFFFF;
#endif

static void control_loop(ob_chain* ob);

static void wq_enqueue(struct thr_info* thr, ob_chain* ob)
{
    struct work_queue* wq;
    wq = &ob->active_wq;

    while (wq->num_elems < MAX_WQ_SIZE) {
        struct work* work = get_work(thr, thr->id);
        struct work_ent* we;

        we = cgmalloc(sizeof(*we));

        we->work = work;
        INIT_LIST_HEAD(&we->head);

        mutex_lock(&ob->lock);
        list_add_tail(&we->head, &wq->head);
        wq->num_elems++;
        // applog(LOG_ERR, "+++++++ wq[%d].num_elems=%d", ob->chain_id, wq->num_elems);
        mutex_unlock(&ob->lock);
    }
}

static struct work* wq_dequeue(ob_chain* ob, bool sig)
{
    struct work_ent* we;
    struct work* work = NULL;
    struct work_queue* wq = &ob->active_wq;

    if (wq == NULL) {
        return NULL;
    }

    /* Sleep only a small duration if there is no work queued in case it's
	 * still refilling rather than we have no upstream work. */
    if (unlikely(!wq->num_elems && sig)) {
        cgsleep_ms(10);
    }

    mutex_lock(&ob->lock);
    if (likely(wq->num_elems > 0)) {
        we = list_entry(wq->head.next, struct work_ent, head);
        work = we->work;

        list_del(&we->head);
        free(we);
        wq->num_elems--;
    }
    if (sig) {
        pthread_cond_signal(&ob->work_cond);
    }
    mutex_unlock(&ob->lock);

    return work;
}

// Generate work in a separate thread so we don't block on work generation
// when an engine becomes ready for work.
static void* ob_gen_work_thread(void* arg)
{
    struct cgpu_info* cgpu = arg;
    ob_chain* ob = cgpu->device_data;
    char tname[16];

    sprintf(tname, "ob_gen_work_%d", ob->chain_id);
    RenameThread(tname);

    mutex_lock(&ob->lock);

    // The work_cond is signalled when this thread should check to see if
    // it needs to generate more work.
    while (!pthread_cond_wait(&ob->work_cond, &ob->lock)) {
        mutex_unlock(&ob->lock);

        // applog(LOG_ERR, "^^^^^^^^^^^^^^^^^^ ob->active_wq.num_elems= %d (ob_gen_work_thread)", ob->active_wq.num_elems);

        // Only start filling the queue once we reach the specified limit
        if (ob->active_wq.num_elems <= WQ_REFILL_SIZE) {
            applog(LOG_ERR, "Calling wq_enqueue() for thread %d", ob->chain_id);
            wq_enqueue(cgpu->thr[0], ob);
        }

        mutex_lock(&ob->lock);
    }

    return NULL;
}

// Perform a hash on the midstate with the specified nonce inserted in place
// and see if the resulting hash has sufficient difficulty to submit to the pool.
bool is_valid_nonce(ob_chain* ob, uint8_t chip_num, uint8_t engine_num, Nonce nonce)
{
#if (MODEL == SC1)
    // Make the header with the nonce inserted at the right spot
    uint8_t header[SIA_HEADER_SIZE];
    memcpy(header, ob->curr_work->midstate, SIA_HEADER_SIZE);
    memcpy(header + 32, &nonce, sizeof(Nonce));

    if (siaHeaderMeetsMinimumTarget(header)) {
        ob->good_nonces_found++;
        applog(LOG_ERR, "CH%u: GOOD NONCE FOUND: 0x%016llX (good count=%d, bad count=%d)", ob->chain_id, nonce, ob->good_nonces_found, ob->bad_nonces_found);
    } else {
        ob->bad_nonces_found++;
        applog(LOG_ERR, "CH%u: BAD NONCE FOUND: 0x%016llX (good count=%d, bad count=%d)", ob->chain_id, nonce, ob->good_nonces_found, ob->bad_nonces_found);
    }

    // Check if it meets the pool's stratum difficulty
    if (ob->curr_work && ob->curr_work->pool) {
        double sdiff = ob->curr_work->pool->sdiff;
        if (siaHeaderMeetsProvidedDifficulty(header, sdiff)) {
            // TODO: Anything else to do here?
            applog(LOG_ERR, "CH%u: NONCE 0x%016llX meets pool sdiff of %f", ob->chain_id, nonce, sdiff);
            return true;
        }
    }
    return false;

#elif (MODEL == DCR1)
    // TODO: Implement for Decred
#endif
}

// // Separate thread to handle nonces so that we are not polling for them
// static void* ob_nonce_thread(void* arg)
// {
//     struct cgpu_info* cgpu = arg;
//     ob_chain* ob = cgpu->device_data;
//     char tname[16];

//     sprintf(tname, "ob_nonce_%d", ob->chain_id);
//     RenameThread(tname);

//     mutex_lock(&ob->lock);

//     while (!pthread_cond_wait(&ob->nonce_cond, &ob->lock)) {
//         mutex_unlock(&ob->lock);

//         int n = num_pending_nonces(ob);
//         while (n > 0) {
//             nonce_info info;
//             if (pop_pending_nonce(ob, &info) == SUCCESS) {
//                 struct work* work = ob->chips[info.chip_num].work;

//                 // Check that the nonce is valid
//                 if (is_valid_nonce(ob, info.nonce)) { // TODO: Probably not using this function, but this may be wrong now
//                     // If valid, submit to the pool
//                     submit_nonce(cgpu->thr[0], work, info.nonce);
//                 } else {
//                     // TODO: handle error in some way
//                 }
//             }
//             n--;
//         }

//         mutex_lock(&ob->lock);
//     }

//     return NULL;
// }

// Separate thread to handle the control loop so we can react quickly to temp changes
static void* ob_control_thread(void* arg)
{
    struct cgpu_info* cgpu = arg;
    ob_chain* ob = cgpu->device_data;
    char tname[16];

    sprintf(tname, "ob_control_%d", ob->chain_id);
    RenameThread(tname);

    // Set initial clock divider and bias for all chips
    ControlLoopState* clBoard = &ob->control_loop_state;
    ob1SetClockDividerAndBias(ob->chain_id, ALL_CHIPS, clBoard->currDivider, clBoard->currBias);

    while (true) {
        control_loop(ob);
        cgsleep_ms(100);
    }

    return NULL;
}

#define QDX(i) (i % MAX_PENDING_NONCES)

ApiError push_pending_nonce(ob_chain* ob, int chip_num, int engine_num, Nonce nonce, bool nonce_limit_reached)
{
    nonce_fifo* fifo = &ob->pending_nonces;
    mutex_lock(&ob->lock);
    // Ensure not full
    if (fifo->head != QDX(fifo->tail + 1)) {
        // Add to the queue
        fifo->nonces[fifo->head].nonce = nonce;
        fifo->nonces[fifo->head].chip_num = chip_num;
        fifo->nonces[fifo->head].engine_num = engine_num;
        fifo->nonces[fifo->head].nonce_limit_reached = nonce_limit_reached;

        fifo->tail = QDX(fifo->tail + 1);

        pthread_cond_signal(&ob->nonce_cond);

        mutex_unlock(&ob->lock);
        return SUCCESS;
    } else {
        // Full!  Just log the fact that we dropped a nonce, but keep running.
        applog(LOG_ERR, "Can't push!  pending_nonces queue is full!");
        mutex_unlock(&ob->lock);
        return GENERIC_ERROR;
    }
}

ApiError pop_pending_nonce(ob_chain* ob, nonce_info* info)
{
    nonce_fifo* fifo = &ob->pending_nonces;
    mutex_lock(&ob->lock);
    // If not empty
    if (fifo->head != fifo->tail) {
        memcpy(info, &fifo->nonces[fifo->head], sizeof(nonce_info));
        fifo->head = QDX(fifo->head + 1);
        mutex_unlock(&ob->lock);
        return SUCCESS;
    } else {
        // Queue is empty
        applog(LOG_ERR, "Can't pop!  pending_nonces queue is empty!");
        mutex_unlock(&ob->lock);
        return GENERIC_ERROR;
    }
}

int num_pending_nonces(ob_chain* ob)
{
    nonce_fifo* fifo = &ob->pending_nonces;
    int num;
    mutex_lock(&ob->lock);
    num = fifo->head > fifo->tail ? (MAX_PENDING_NONCES - fifo->head + fifo->tail + 1) : (fifo->tail - fifo->head + 1);
    mutex_unlock(&ob->lock);
    return num;
}

// void mark_engine_ready(ob_chain* ob, int chip_num, int engine_num)
// {
//     mutex_lock(&ob->lock);
//     ob->chips[chip_num].ready_engines[engine_num] = 1;
//     mutex_unlock(&ob->lock);
// }

#if (MODEL == SC1)
void set_engine_busy(ob_chain* ob, int chip_num, int engine_num, bool isBusy)
{
    mutex_lock(&ob->lock);
    uint64_t engine_bit = 1ULL << engine_num;
    if (isBusy) {
        // Set the corresponding bit
        ob->chips[chip_num].busy_engines = ob->chips[chip_num].busy_engines | engine_bit;
    } else {
        // Clear the corresponding bit
        ob->chips[chip_num].busy_engines = ob->chips[chip_num].busy_engines & ~engine_bit;
    }
    // applog(LOG_ERR, "set_engine_busy(%d): 0x%016llX", isBusy, ob->chips[chip_num].busy_engines);
    mutex_unlock(&ob->lock);
}

bool is_engine_busy(ob_chain* ob, int chip_num, int engine_num)
{
    mutex_lock(&ob->lock);
    uint64_t engine_bit = 1ULL << engine_num;
    bool isBusy = ob->chips[chip_num].busy_engines & engine_bit;
    mutex_unlock(&ob->lock);
    return isBusy;
}
#elif (MODEL == DCR1)
void set_engine_busy(ob_chain* ob, int chip_num, int engine_num, bool is_busy)
{
    mutex_lock(&ob->lock);

    uint64_t* pbusy_engines;
    if (engine_num < 64) {
        pbusy_engines = &ob->chips[chip_num].busy_engines[1];
    } else {
        pbusy_engines = &ob->chips[chip_num].busy_engines[0];
        engine_num -= 64;
    }

    uint64_t engine_bitmask = 1ULL << engine_num;
    if (is_busy) {
        // Set the corresponding bit
        *pbusy_engines = *pbusy_engines | engine_bitmask;
    } else {
        // Clear the corresponding bit
        *pbusy_engines = *pbusy_engines & ~engine_bitmask;
    }
    // applog(LOG_ERR, "set_engine_busy(%d): 0x%016llX", isBusy, ob->chips[chip_num].busy_engines);
    mutex_unlock(&ob->lock);
}

bool is_engine_busy(ob_chain* ob, int chip_num, int engine_num)
{
    mutex_lock(&ob->lock);
    uint64_t* pbusy_engines;
    if (engine_num < 64) {
        pbusy_engines = &ob->chips[chip_num].busy_engines[1];
    } else {
        pbusy_engines = &ob->chips[chip_num].busy_engines[0];
        engine_num -= 64;
    }

    uint64_t engine_bitmask = 1ULL << engine_num;
    bool isBusy = (*pbusy_engines) & engine_bitmask;
    mutex_unlock(&ob->lock);
    return isBusy;
}
#endif

void add_hashes(ob_chain* ob, uint64_t num_hashes)
{
    mutex_lock(&ob->lock);
    ob->num_hashes += num_hashes;
    mutex_unlock(&ob->lock);
}

void add_good_nonces(ob_chain* ob, uint64_t amt)
{
    mutex_lock(&ob->lock);
    ob->good_nonces_found += amt;
    mutex_unlock(&ob->lock);
}

void add_bad_nonces(ob_chain* ob, uint64_t amt)
{
    mutex_lock(&ob->lock);
    ob->bad_nonces_found += amt;
    mutex_unlock(&ob->lock);
}

// Get the number of hashes since we last checked, then reset to zero
uint64_t get_and_reset_hashes(ob_chain* ob)
{
    uint64_t n;
    mutex_lock(&ob->lock);
    n = ob->num_hashes;
    ob->num_hashes = 0;
    mutex_unlock(&ob->lock);
    return n;
}

uint64_t get_and_reset_good_nonces(ob_chain* ob)
{
    uint64_t n;
    mutex_lock(&ob->lock);
    n = ob->good_nonces_found;
    ob->good_nonces_found = 0;
    mutex_unlock(&ob->lock);
    return n;
}

uint64_t get_and_reset_bad_nonces(ob_chain* ob)
{
    uint64_t n;
    mutex_lock(&ob->lock);
    n = ob->bad_nonces_found;
    ob->bad_nonces_found = 0;
    mutex_unlock(&ob->lock);
    return n;
}

uint64_t get_num_hashes(ob_chain* ob)
{
    uint64_t n;
    mutex_lock(&ob->lock);
    n = ob->num_hashes;
    mutex_unlock(&ob->lock);
    return n;
}

uint64_t get_good_nonces(ob_chain* ob)
{
    uint64_t n;
    mutex_lock(&ob->lock);
    n = ob->good_nonces_found;
    mutex_unlock(&ob->lock);
    return n;
}

uint64_t get_bad_nonces(ob_chain* ob)
{
    uint64_t n;
    mutex_lock(&ob->lock);
    n = ob->bad_nonces_found;
    mutex_unlock(&ob->lock);
    return n;
}

// void ob_nonce_handler(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, Nonce nonce, bool nonceLimitReached)
// {
//     applog(LOG_ERR, "ob_nonce_handler called!");
//     // Take the info and put it into a queue that is service by a nonce thread and signal
//     // the condition variable.
//     ob_chain* ob = &chains[boardNum];
//     push_pending_nonce(ob, chipNum, engineNum, nonce, nonceLimitReached);

//     // TODO: API should clear interrupts after calling handler possibly multiple times
// }

// void ob_job_complete_handler(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum)
// {
//     applog(LOG_ERR, "ob_job_complete_handler called!");
//     ob_chain* ob = &chains[boardNum];
//     mark_engine_ready(ob, chipNum, engineNum);

//     add_hashes(ob, NONCE_RANGE_SIZE);
//     // TODO: API should clear interrupts after calling handler possibly multiple times
// }

static void obelisk_detect(bool hotplug)
{
    applog(LOG_ERR, "***** obelisk_detect()\n");

    // TODO: Detect how many boards are installed and set a global variable
    // num_chains, so we know how many boards to talk to, how many threads
    // to start, etc.

    // ob1RegisterNonceHandler(ob_nonce_handler);
    // ob1RegisterJobCompleteHandler(ob_job_complete_handler);

    ob1Initialize();

    int num_chains = ob1GetNumPresentHashboards();
    // int num_chains = 1;

    for (int i = 0; i < num_chains; i++) {
        struct cgpu_info* cgpu;
        ob_chain* ob;
        pthread_t pth;

        cgpu = cgcalloc(sizeof(*cgpu), 1);
        cgpu->drv = &obelisk_drv;
        cgpu->name = "Obelisk.Chain";
        cgpu->threads = 1;
        cgpu->chain_num = i;
        cgpu->device_data = ob = &chains[i];
        chains[i].chain_id = i;

        // Setup control loop initial state
        chains[i].control_loop_state.boardId = i;
        chains[i].control_loop_state.currDivider = 8;
        chains[i].control_loop_state.currBias = MIN_BIAS;
        chains[i].control_loop_state.lastChangeUp = false;
        chains[i].control_loop_state.lastTemp = 0;
        chains[i].control_loop_state.lastTempOnChange = 0;
        chains[i].control_loop_state.ticksSinceLastChange = 0;
        chains[i].control_loop_state.printCounter = 0;
        chains[i].control_loop_state.masterTickCounter = 0;
        chains[i].control_loop_state.lastGHS = 0.0;
        chains[i].control_loop_state.lastGHSStatistical = 0.0;

        cgtime(&cgpu->dev_start_tv);

        // TODO: Add this if necessary later
        //ob->lastshare = cgpu->dev_start_tv.tv_sec;

        // Initialize the ob chip fields
        for (int i = 0; i < NUM_CHIPS_PER_BOARD; i++) {
            struct chip_info* chip = &ob->chips[i];
            chip->engines_curr_work = cgcalloc(sizeof(struct work*), ob1GetNumEnginesPerChip());
        }

        INIT_LIST_HEAD(&ob->active_wq.head);

        chains[i].cgpu = cgpu;
        add_cgpu(cgpu);
        cgpu->device_id = i;

        applog(LOG_WARNING, "Detected ob chain %d", i);

        mutex_init(&ob->lock);
        pthread_cond_init(&ob->work_cond, NULL);
        pthread_create(&pth, NULL, ob_gen_work_thread, cgpu);

        pthread_cond_init(&ob->nonce_cond, NULL);
        //pthread_create(&pth, NULL, ob_nonce_thread, cgpu);
        pthread_create(&pth, NULL, ob_control_thread, cgpu);
    }

    applog(LOG_ERR, "***** obelisk_detect() DONE\n");
}

static void obelisk_identify(__maybe_unused struct cgpu_info* cgpu)
{
    applog(LOG_ERR, "***** obelisk_identify()");
}

static void obelisk_flush_work(struct cgpu_info* cgpu)
{
    applog(LOG_ERR, "***** obelisk_flush_work()");
}

static bool obelisk_thread_prepare(struct thr_info* thr)
{
    applog(LOG_ERR, "***** obelisk_thread_prepare()");
    return true;
}

static void obelisk_shutdown(struct thr_info* thr)
{
    applog(LOG_ERR, "***** obelisk_shutdown()");
}

static bool obelisk_queue_full(struct cgpu_info* cgpu)
{
    // applog(LOG_ERR, "***** obelisk_queue_full()");
    return true;
}

static void update_temp(temp_stats_t* temps, double curr_temp)
{
    if (curr_temp > temps->high) {
        temps->high = curr_temp;
    }
    if (curr_temp < temps->low) {
        temps->low = curr_temp;
    }
    temps->curr = curr_temp;
}

// CONTROL LOOP CONFIG
#define CONTROL_LOOP_SUPER_HIGH_TEMP 90.0
#define CONTROL_LOOP_HIGH_TEMP 85.0
#define CONTROL_LOOP_LOW_TEMP 80.0
#define TICKS_BETWEEN_BIAS_UPS 100

// Status display variables.
#define ChipCount 15
#define StatusOutputFrequency 3

// Temperature measurement variables.
#define ChipTempVariance 5.0 // Temp rise of chip due to silicon inconsistencies.
#define HotChipTargetTemp 105.0 // Acceptable temp for hottest chip.
#define HotChipTempRise 15.0 // Thermal sims suggest the hottest chip is this much hotter than the senosr chip.

// Overtemp variables.
#define TempDeviationAcceptable 5.0 // The amount the temperature is allowed to vary from the target temperature.
#define TempDeviationUrgent 3.0 // Temp above acceptable where rapid bias reductions begin.
#define OvertempCheckFrequency 1

// Undertemp variables.
#define TempGapCold 40
#define TempGapWarm 20
#define TempRiseSpeedCold 3
#define TempRiseSpeedWarm 2
#define TempRiseSpeedHot 1
#define UndertempCheckFrequency 1

// String stability variables.
#define StartingStringVoltageLevel 20
#define MaxVoltageLevel 72
#define VoltageStepSize 12
#define StringStableTime 360 // How long a string must have the same bias before being considered stable.
#define StringMaxTime 1200 // After this amount of time, the string is abandoned as being unstable.

// increaseDivider will increase the clock divider, resulting in a slower chip.
static void increaseDivider(uint8_t* divider) {
    switch (*divider) {
    case 1:
        *divider *= 2;
        break;
    case 2:
        *divider *= 2;
        break;
    case 4:
        *divider *= 2;
        break;
    default:
        // 8 or any other value means no change
        break;
    }
}

static void decreaseDivider(uint8_t* divider) {
    switch (*divider) {
    case 2:
		*divider /= 2;
        break;
    case 4:
		*divider /= 2;
        break;
    case 8:
		*divider /= 2;
        break;
    default:
        // 1 or any other value means no change
        break;
    }
}


static void increaseBias(int8_t* currentBias, uint8_t* currentDivider) {
    if (*currentBias == MAX_BIAS) {
        if (*currentDivider > 1) {
			decreaseDivider(currentDivider);
            *currentBias = MIN_BIAS;
        }
    } else {
        *currentBias += 1;
    }
}

static void decreaseBias(int8_t* currentBias, uint8_t* currentDivider) {
    if (*currentBias == MIN_BIAS) {
        if (*currentDivider < 8) {
			increaseDivider(currentDivider);
            *currentBias = MAX_BIAS;
        }
    } else {
        *currentBias -= 1;
    }
}

// commitBias will take all of the current chip biases and commit them to the
// string.
static void commitBias(ob_chain* ob) {
	int i = 0;
	for (i = 0; i < 15; i++) {
		ob1SetClockDividerAndBias(ob->control_loop_state.boardNumber, i, ob->control_loop_state.chipDividers[i], ob->control_loop_state.chipBiases[i]);
	}
	ob->control_loop_state.prevBiasChangeTime = ob->control_loop_state.currentTime;
	ob->control_loop_state.goodNoncesUponLastBiasChange = ob->control_loop_state.currentGoodNonces;
}

// initControlState will prepare the control state for operation.
static void initControlState(ob_chain* ob) {
	// Set the board number.
	ob->control_loop_state.boardNumber = ob->chain_id;
	ob->control_loop_state.currentTime = time(0);

	// Set the chip biases to minimum.
	int i = 0;
	for (i = 0; i < ChipCount; i++) {
		ob->control_loop_state.chipBiases[i] = MIN_BIAS;
		ob->control_loop_state.chipDividers[i] = 8;
	}

	// Set the prevBiasChangeTime and prevVoltageChangeTime to the current time.
	ob->control_loop_state.prevBiasChangeTime = ob->control_loop_state.currentTime;
	ob->control_loop_state.prevVoltageChangeTime = ob->control_loop_state.currentTime;

	// Set the voltage to level 20. This is the higest voltage that we start at,
	// and then we walk up through the whole voltage process, printing out the
	// hashrate as we go.
	ob1SetStringVoltage(ob->control_loop_state.boardNumber, StartingStringVoltageLevel);
	ob->control_loop_state.currentVoltageLevel = StartingStringVoltageLevel;

	// Set the board to initialized.
	ob->control_loop_state.initialized = true;
}

// updateControlState will update fields that depend on external factors.
// Things like the time and string temperature.
static void updateControlState(ob_chain* ob) {
	if (!ob->control_loop_state.initialized) {
		initControlState(ob);
	}

	// Fetch some status variables about the hashing board.
	HashboardStatus hbStatus = ob1GetHashboardStatus(ob->control_loop_state.boardNumber);

	// Update status values.
	ob->control_loop_state.currentTime = time(0);
	ob->control_loop_state.currentStringTemp = hbStatus.chipTemp;
	ob->control_loop_state.currentStringVoltage = hbStatus.asicV15;

	// Fetch nonce count updates.
    mutex_lock(&ob->lock);
	ob->control_loop_state.currentGoodNonces = ob->good_nonces_found;
    mutex_unlock(&ob->lock);
	ob->control_loop_state.goodNoncesSinceBiasChange = ob->control_loop_state.currentGoodNonces - ob->control_loop_state.goodNoncesUponLastBiasChange;
	ob->control_loop_state.goodNoncesSinceVoltageChange = ob->control_loop_state.currentGoodNonces - ob->control_loop_state.goodNoncesUponLastVoltageChange;
}

// displayControlState will check if enough time has passed, and then display
// the current state of the hashing board to the user.
static void displayControlState(ob_chain* ob) {
	time_t lastStatus = ob->control_loop_state.lastStatusOutput;
	time_t currentTime = ob->control_loop_state.currentTime;
	if (currentTime - lastStatus > StatusOutputFrequency) {
		// Compute the hashrate.
		uint64_t goodNonces = ob->control_loop_state.goodNoncesSinceBiasChange;
		time_t secondsElapsed = ob->control_loop_state.currentTime - ob->control_loop_state.prevBiasChangeTime + 1;
		uint64_t hashrate = 1099 * goodNonces / secondsElapsed;

		// Currently only displays the bias of the first chip.
		char outBuf[256];
        sprintf(outBuf, "HB%u:  Temp: %-5.1f  VString: %2.02f  Biases: %u.%i  Hashrate: %lld GH/s  Stability: %i  Timeouts: %lld VLevel: %u",
            ob->control_loop_state.boardNumber,
            ob->control_loop_state.currentStringTemp,
            ob->control_loop_state.currentStringVoltage,
			ob->control_loop_state.chipDividers[0],
			ob->control_loop_state.chipBiases[0],
			hashrate,
			secondsElapsed,
			ob->control_loop_state.stringTimeouts,
			ob->control_loop_state.currentVoltageLevel);
        applog(LOG_ERR, outBuf);
		sprintf(outBuf, "%u.%i, %u.%i, %u.%i, %u.%i, %u.%i, %u.%i, %u.%i, %u.%i, %u.%i, %u.%i, %u.%i, %u.%i, %u.%i, %u.%i, %u.%i",
				ob->control_loop_state.chipDividers[0],
				ob->control_loop_state.chipBiases[0],
				ob->control_loop_state.chipDividers[1],
				ob->control_loop_state.chipBiases[1],
				ob->control_loop_state.chipDividers[2],
				ob->control_loop_state.chipBiases[2],
				ob->control_loop_state.chipDividers[3],
				ob->control_loop_state.chipBiases[3],
				ob->control_loop_state.chipDividers[4],
				ob->control_loop_state.chipBiases[4],
				ob->control_loop_state.chipDividers[5],
				ob->control_loop_state.chipBiases[5],
				ob->control_loop_state.chipDividers[6],
				ob->control_loop_state.chipBiases[6],
				ob->control_loop_state.chipDividers[7],
				ob->control_loop_state.chipBiases[7],
				ob->control_loop_state.chipDividers[8],
				ob->control_loop_state.chipBiases[8],
				ob->control_loop_state.chipDividers[9],
				ob->control_loop_state.chipBiases[9],
				ob->control_loop_state.chipDividers[10],
				ob->control_loop_state.chipBiases[10],
				ob->control_loop_state.chipDividers[11],
				ob->control_loop_state.chipBiases[11],
				ob->control_loop_state.chipDividers[12],
				ob->control_loop_state.chipBiases[12],
				ob->control_loop_state.chipDividers[13],
				ob->control_loop_state.chipBiases[13],
				ob->control_loop_state.chipDividers[14],
				ob->control_loop_state.chipBiases[14]);
        applog(LOG_ERR, outBuf);
		sprintf(outBuf, "%lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld",
				ob->control_loop_state.voltageLevelHashrates[20],
				ob->control_loop_state.voltageLevelHashrates[24],
				ob->control_loop_state.voltageLevelHashrates[28],
				ob->control_loop_state.voltageLevelHashrates[32],
				ob->control_loop_state.voltageLevelHashrates[36],
				ob->control_loop_state.voltageLevelHashrates[40],
				ob->control_loop_state.voltageLevelHashrates[44],
				ob->control_loop_state.voltageLevelHashrates[48],
				ob->control_loop_state.voltageLevelHashrates[52],
				ob->control_loop_state.voltageLevelHashrates[56],
				ob->control_loop_state.voltageLevelHashrates[60],
				ob->control_loop_state.voltageLevelHashrates[64],
				ob->control_loop_state.voltageLevelHashrates[68],
				ob->control_loop_state.voltageLevelHashrates[72],
				ob->control_loop_state.voltageLevelHashrates[76],
				ob->control_loop_state.voltageLevelHashrates[80],
				ob->control_loop_state.voltageLevelHashrates[84],
				ob->control_loop_state.voltageLevelHashrates[88],
				ob->control_loop_state.voltageLevelHashrates[92],
				ob->control_loop_state.voltageLevelHashrates[96],
				ob->control_loop_state.voltageLevelHashrates[100],
				ob->control_loop_state.voltageLevelHashrates[104],
				ob->control_loop_state.voltageLevelHashrates[108],
				ob->control_loop_state.voltageLevelHashrates[112],
				ob->control_loop_state.voltageLevelHashrates[116],
				ob->control_loop_state.voltageLevelHashrates[120],
				ob->control_loop_state.voltageLevelHashrates[124]);
        applog(LOG_ERR, outBuf);

		ob->control_loop_state.lastStatusOutput = currentTime;
	}
}

// targetTemp returns the target temperature for the chip we want.
static double getTargetTemp(ob_chain* ob) {
	// TODO: As we start playing with the individual biases of the chips, we can
	// extend this function to take into account all the ways that the chips
	// have been pushed around.
	return HotChipTargetTemp-ChipTempVariance-HotChipTempRise;
}

// decrease the clock bias of every chip on the string.
static void decreaseStringBias(ob_chain* ob) {
	int i = 0;
	for (i = 0; i < ChipCount; i++) {
		decreaseBias(&ob->control_loop_state.chipBiases[i], &ob->control_loop_state.chipDividers[i]);
	}
	applog(LOG_ERR, "HB%u: decreasing the string bias.", ob->control_loop_state.boardNumber);
	commitBias(ob);
}

// increase the clock bias of every chip on the string.
static void increaseStringBias(ob_chain* ob) {
	int i = 0;
	for (i = 0; i < ChipCount; i++) {
		increaseBias(&ob->control_loop_state.chipBiases[i], &ob->control_loop_state.chipDividers[i]);
	}
	applog(LOG_ERR, "HB%u: increasing the string bias.", ob->control_loop_state.boardNumber);
	commitBias(ob);
}

// handleOvertemps will clock down the string if the string is overheating.
// 
// TODO: Add the deviation growth bits for unstable strings.
static void handleOvertemps(ob_chain* ob, double targetTemp) {
	time_t timeElapsed = ob->control_loop_state.currentTime - ob->control_loop_state.prevOvertempCheck;
	double currentTemp = ob->control_loop_state.currentStringTemp;
	if (timeElapsed > OvertempCheckFrequency) {
		// Reduce the string bias if we are overtemp.
		if (currentTemp > targetTemp+TempDeviationAcceptable) {
			decreaseStringBias(ob);
		}
		// Rapidly reduce the string bias again if we are at an urgent
		// temperature.
		if (currentTemp > targetTemp+TempDeviationAcceptable+TempDeviationUrgent) {
			decreaseStringBias(ob);
		}
		ob->control_loop_state.prevOvertempCheck = ob->control_loop_state.currentTime;
	}
}

// handleUndertemps will clock up the string if the string is too cold.
static void handleUndertemps(ob_chain* ob, double targetTemp) {
	time_t timeElapsed = ob->control_loop_state.currentTime - ob->control_loop_state.prevUndertempCheck;
	double currentTemp = ob->control_loop_state.currentStringTemp;
	double prevTemp = ob->control_loop_state.prevUndertempStringTemp;
	if (timeElapsed > UndertempCheckFrequency) {
		if (currentTemp < targetTemp-TempGapCold && currentTemp - prevTemp < TempRiseSpeedCold * UndertempCheckFrequency) {
			increaseStringBias(ob);
		} else if (currentTemp < targetTemp-TempGapWarm && currentTemp - prevTemp < TempRiseSpeedWarm * UndertempCheckFrequency) {
			increaseStringBias(ob);
		} else if (currentTemp < targetTemp-TempDeviationAcceptable && currentTemp - prevTemp < TempRiseSpeedHot * UndertempCheckFrequency) {
			increaseStringBias(ob);
		}
		ob->control_loop_state.prevUndertempCheck = ob->control_loop_state.currentTime;
		ob->control_loop_state.prevUndertempStringTemp = ob->control_loop_state.currentStringTemp;
	}
}

static void setVoltageLevel(ob_chain* ob, uint8_t level) {
	ob1SetStringVoltage(ob->control_loop_state.boardNumber, level);
	ob->control_loop_state.currentVoltageLevel = level;
	ob->control_loop_state.goodNoncesUponLastVoltageChange = ob->control_loop_state.currentGoodNonces;
	ob->control_loop_state.prevVoltageChangeTime = ob->control_loop_state.currentTime;

	// Changing the voltage is significant enough to count as changing the bias
	// too.
	ob->control_loop_state.goodNoncesUponLastBiasChange = ob->control_loop_state.currentGoodNonces;
	ob->control_loop_state.prevBiasChangeTime = ob->control_loop_state.currentTime;
}

// control_loop runs the hashing boards and attempts to work towards an optimal
// hashrate.
static void control_loop(ob_chain* ob) {
	// Get a static view of the current state of the hashing board.
	updateControlState(ob);
	displayControlState(ob);

	// Determine the target temperature for the temperature chip.
	double targetTemp = getTargetTemp(ob);
	handleOvertemps(ob, targetTemp);
	handleUndertemps(ob, targetTemp);

	// Determine if string is stable.
	time_t currentTime = ob->control_loop_state.currentTime;
	time_t lastBiasChange = ob->control_loop_state.prevBiasChangeTime;
	time_t lastVoltageChange = ob->control_loop_state.prevVoltageChangeTime;
	if (currentTime - lastBiasChange < StringStableTime && currentTime - lastVoltageChange <= StringMaxTime) {
		// The string is not stable, but has also not timed out.
		return;
	}

	// Check if the string is at the final voltage.
	uint8_t currentLevel = ob->control_loop_state.currentVoltageLevel;
	if (currentLevel+VoltageStepSize >= MaxVoltageLevel) {
		// TODO: Begin playing with chip bias.
		return;
	}

	// Check if the string is stable or not. If yes, use hashrate since last
	// bias change. If not, use hashrate since last voltage change.
	uint64_t hashrate = 0;
	if (currentTime - lastBiasChange < StringStableTime && currentTime - lastVoltageChange >= StringMaxTime) {
		// The string is not stable, instead has timed out. Use hashrate over
		// lifetime of string to estimate hashrate.
		applog(LOG_ERR, "HB%u: string is victim of timeout", ob->control_loop_state.boardNumber);
		ob->control_loop_state.stringTimeouts++;
		hashrate = ob->control_loop_state.goodNoncesSinceVoltageChange * 1099 / (currentTime-lastVoltageChange+1);
	} else {
		// The string is stable. Use hashrate over lifetime of recent bias
		// change to esmiate hashrate.
		hashrate = ob->control_loop_state.goodNoncesSinceBiasChange * 1099 / (currentTime-lastBiasChange+1);
	}

	// The string has reached a state where it is time to try the next voltage.
	ob->control_loop_state.voltageLevelHashrates[currentLevel] = hashrate;
	setVoltageLevel(ob, currentLevel+VoltageStepSize);
	return;
}

static void control_loop_old(ob_chain* ob)
{
    HashboardModel model = ob1GetHashboardModel();
    char outBuf[256];

    // Control "loop"
    uint8_t board = ob->chain_id;
    HashboardStatus hbStatus = ob1GetHashboardStatus(board);
    double currAsicTemp = hbStatus.chipTemp;
    ControlLoopState* clBoard = &ob->control_loop_state;

    // Calculate hashrate based on DONE jobs
    clBoard->masterTickCounter++;
    if ((clBoard->masterTickCounter % 300) == 0) {
        double hashes = (double)get_and_reset_hashes(ob);
        clBoard->lastGHS = (hashes / 1000000000.0) / 30.0;
    }

    if ((clBoard->masterTickCounter % 600) == 0) {
        double good_nonces = (double)get_and_reset_good_nonces(ob);
        uint64_t hashes_per_nonce = 1ULL << 40;
        double hashes = (double)hashes_per_nonce * good_nonces;
        clBoard->lastGHSStatistical = (hashes / 1000000000.0) / 36.0;
    }

    // applog(LOG_ERR, "CH%u: chipTemp=%3.1fC  ticks=%lu", ob->chain_id, currAsicTemp, clBoard->ticksSinceLastChange);

    // Check if the temp is too high
    if (currAsicTemp >= CONTROL_LOOP_HIGH_TEMP && clBoard->ticksSinceLastChange >= 10) {
        clBoard->ticksSinceLastChange = 0;
        clBoard->lastTempOnChange = currAsicTemp;

        decrementBias(clBoard);

        ob1SetClockDividerAndBias(board, ALL_CHIPS, clBoard->currDivider, clBoard->currBias);

        formatControlLoopState(outBuf, clBoard);
        applog(LOG_ERR, "CH%u: OVER HIGH_TEMP (%3.1fC): %s", board, CONTROL_LOOP_HIGH_TEMP, outBuf);
    }

    if (currAsicTemp >= CONTROL_LOOP_SUPER_HIGH_TEMP && clBoard->ticksSinceLastChange >= 5) {
        clBoard->ticksSinceLastChange = 0;
        clBoard->lastTempOnChange = currAsicTemp;

        decrementBias(clBoard);

        ob1SetClockDividerAndBias(board, ALL_CHIPS, clBoard->currDivider, clBoard->currBias);

        formatControlLoopState(outBuf, clBoard);
        applog(LOG_ERR, "CH%u: OVER SUPER_HIGH_TEMP (%3.1fC): %s", board, CONTROL_LOOP_SUPER_HIGH_TEMP, outBuf);
    }

    // Check if the temp is too low or if temp did not increase since the last time
    if (clBoard->ticksSinceLastChange >= TICKS_BETWEEN_BIAS_UPS) {
        if (currAsicTemp <= CONTROL_LOOP_LOW_TEMP && (int)currAsicTemp - (int)clBoard->lastTempOnChange <= 2) {
            // if (currAsicTemp <= CONTROL_LOOP_LOW_TEMP) {
            clBoard->lastTempOnChange = currAsicTemp;

            // Bump up the voltage bias (and thus the clockrate)
            incrementBias(clBoard);

            ob1SetClockDividerAndBias(board, ALL_CHIPS, clBoard->currDivider, clBoard->currBias);

            formatControlLoopState(outBuf, clBoard);
            applog(LOG_ERR, "CH%u: Undertemp (%3.1fC): %s", board, CONTROL_LOOP_LOW_TEMP, outBuf);

        } else {
            clBoard->lastTempOnChange = currAsicTemp;
        }
        clBoard->ticksSinceLastChange = 0;
    } else {
        // No change
        clBoard->ticksSinceLastChange++;
    }
    clBoard->lastTemp = currAsicTemp;

    // applog(LOG_ERR, "printCounter=%d", printCounter);

    // Manage string voltage - simple example
    // if (clBoard->masterTickCounter % 100 == 0) {
    //     ob1SetStringVoltage(clBoard->boardId, 50);
    // } else if (clBoard->masterTickCounter % 50 == 0) {
    //     ob1SetStringVoltage(clBoard->boardId, 30);
    // }

    clBoard->printCounter++;
    if (clBoard->printCounter == 10) {
        clBoard->printCounter = 0;

        // We can have up to 3 control loop instances, so only one of them should toggle the LED
        // TODO: Probably move/change this so we can handle showing errors from any of the control_loops()
        if (ob->chain_id == 0) {
            ob1ToggleGreenLED();
        }

        ControlLoopState* state = &ob->control_loop_state;

        char divAndBiasBuf[20];
        formatDividerAndBias(divAndBiasBuf, state);

        sprintf(outBuf, "CH%u: Temp: %-5.1f  Dir: %s  Div|Bias: %s  GH/s: %-5.1f  GH/s(stat): %-5.1f  VString: %2.02f",
            ob->chain_id,
            state->lastTemp,
            state->lastChangeUp ? "up  " : "down",
            divAndBiasBuf,
            clBoard->lastGHS,
            clBoard->lastGHSStatistical,
            hbStatus.asicV15);
        applog(LOG_ERR, outBuf);
    }
}

/*
 * TODO: allow this to run through more than once - the second+
 * time not sending any new work unless a flush occurs since:
 * at the moment we have BAB_STD_WORK_mS latency added to earliest replies
 */
static int64_t obelisk_scanwork(__maybe_unused struct thr_info* thr)
{
    struct cgpu_info* cgpu = thr->cgpu;
    ob_chain* ob = cgpu->device_data;
    // applog(LOG_ERR, "CH%u: obelisk_scanwork() - start", ob->chain_id);

    pthread_cond_signal(&ob->work_cond);

    // Get nonces from chip and submit
    if (false) {
        Nonce nonce = 0x12345678;
        // Need to pass back the work when submitting the nonce
        // This is pretty stupid, since we probably only need the job_id
        // and the extranonce2, which we could hang onto separately rather
        // than the entire huge work structure.
        // submit_nonce(thr, work, nonce);
    }

    // Check to see if it's been too long since we submitted a share
    // and exit if so.

#if (MODEL == SC1)
    // Look for nonces first so we can give new work in the same iteration below
    // Look for done engines, and read their nonces
    for (uint8_t chip_num = 0; chip_num < NUM_CHIPS_PER_BOARD; chip_num++) {
        uint64_t doneBitmask;
        ApiError error = ob1GetDoneEngines(ob->chain_id, chip_num, &doneBitmask);
        if (doneBitmask != 0) {
            // applog(LOG_ERR, "CH%u: doneBitmask=0x%016llX (chip_num=%u)", ob->chain_id, doneBitmask, chip_num);
        }
        if (error == SUCCESS && doneBitmask != 0) {
            for (uint8_t engine_num = 0; engine_num < ob1GetNumEnginesPerChip(); engine_num++) {
                // If engine is done
                if (doneBitmask & (1ULL << engine_num)) {

                    // We are done, so count the hashes
                    add_hashes(ob, NONCE_RANGE_SIZE);

                    NonceSet nonce_set;
                    error = ob1ReadNonces(ob->chain_id, chip_num, engine_num, &nonce_set);
                    for (uint8_t i = 0; i < nonce_set.count; i++) {
                        // Check that the nonce is valid
                        if (is_valid_nonce(ob, chip_num, engine_num, nonce_set.nonces[i])) {
                            add_good_nonces(ob, 1);
                            // If valid, submit to the pool
                            submit_nonce(cgpu->thr[0], ob->chips[chip_num].engines_curr_work[engine_num], nonce_set.nonces[i]);
                        } else {
                            // TODO: handle error in some way
                            add_bad_nonces(ob, 1);
                        }
                    }

                    // Reset the busy flag so it is given a new job below
                    // applog(LOG_ERR, "Setting engine not busy");
                    set_engine_busy(ob, chip_num, engine_num, false);

                    // NOTE: We don't clear the engine DONE flag, as that is not possible directly.
                    //  instead, you need to give it a new job.
                }
            }
        }
    }
#elif (MODEL == DCR1)
    // Look for nonces first so we can give new work in the same iteration below
    // Look for done engines, and read their nonces
    for (uint8_t chip_num = 0; chip_num < NUM_CHIPS_PER_BOARD; chip_num++) {
        uint64_t done_bitmask[2];
        ApiError error = ob1GetDoneEngines(ob->chain_id, chip_num, done_bitmask);
        if (done_bitmask[0] != 0 || done_bitmask[1] != 0) {
            // applog(LOG_ERR, "CH%u: doneBitmask=0x%016llX%016llX (chip_num=%u)", ob->chain_id, done_bitmask[1], done_bitmask[0], chip_num);
        }
        if (error == SUCCESS && done_bitmask != 0) {
            for (uint8_t engine_num = 0; engine_num < ob1GetNumEnginesPerChip(); engine_num++) {
                uint64_t curr_done_bitmask;
                uint64_t engine_bitmask;
                // TODO: There is probably a better way to do this with a loop or a function, but this works for now
                // e.g., isBitSetInBuffer(engineNum, done_bitmask);
                if (engine_num < 64) {
                    curr_done_bitmask = done_bitmask[1];
                    engine_bitmask = (1ULL << engine_num);
                } else {
                    curr_done_bitmask = done_bitmask[0];
                    engine_bitmask = (1ULL << (engine_num - 64));
                }

                // If engine is done
                if (curr_done_bitmask & engine_bitmask) {
                    applog(LOG_ERR, "Reading a nonce from %u:%u:%u", ob->chain_id, chip_num, engine_num);
                    // We are done, so count the hashes
                    add_hashes(ob, NONCE_RANGE_SIZE);

                    NonceSet nonce_set;
                    error = ob1ReadNonces(ob->chain_id, chip_num, engine_num, &nonce_set);
                    for (uint8_t i = 0; i < nonce_set.count; i++) {
                        // Check that the nonce is valid
                        if (is_valid_nonce(ob, chip_num, engine_num, nonce_set.nonces[i])) {
                            add_good_nonces(ob, 1);
                            // If valid, submit to the pool
                            submit_nonce(cgpu->thr[0], ob->chips[chip_num].engines_curr_work[engine_num], nonce_set.nonces[i]);
                        } else {
                            // TODO: handle error in some way
                            add_bad_nonces(ob, 1);
                        }
                    }

                    // Reset the busy flag so it is given a new job below
                    // applog(LOG_ERR, "Setting engine not busy");
                    set_engine_busy(ob, chip_num, engine_num, false);

                    // NOTE: We don't clear the engine DONE flag, as that is not possible directly.
                    //  instead, you need to give it a new job.
                }
            }
        }
    }
#endif

    // See if the pool asked us to start clean on new work
    if (ob->curr_work && ob->curr_work->pool->swork.clean) {
        ob->curr_work->pool->swork.clean = false;
        ob->curr_work = NULL;
    }

#if (MODEL == SC1)
check_for_new_work:
    // Get a single work item
    // TODO: We don;t need to get a new work item every time through - Sia has TONS of nonce space,
    // so we can keep reusing it.
    if (!ob->curr_work) {
        ob->curr_work = wq_dequeue(ob, true);
        applog(LOG_ERR, "***************************************************************");
        applog(LOG_ERR, "Switching to new work: job_id=%s, nonce2=%lld", ob->curr_work->job_id, ob->curr_work->nonce2);
        applog(LOG_ERR, "***************************************************************");

        hexdump(ob->curr_work->midstate, SIA_HEADER_SIZE);

        // Load the current job to all chips
        Job job;
        // TODO: Change ob1LoadJob() to take a byte pointer so we avoid this copy
        memcpy(&job.blake2b, ob->curr_work->midstate, SIA_HEADER_SIZE);

        ApiError error = ob1LoadJob(ob->chain_id, ALL_CHIPS, ALL_ENGINES, &job);
        if (error != SUCCESS) {
            // TODO: do something
            goto scanwork_exit;
        }

        ob->start_of_next_nonce = 0;
    }

    if (ob->curr_work) {

        for (uint8_t chip_num = 0; chip_num < NUM_CHIPS_PER_BOARD; chip_num++) {
            for (uint8_t engine_num = 0; engine_num < ob1GetNumEnginesPerChip(); engine_num++) {
                // applog(LOG_ERR, "Checking if engine_num=%d is done", engine_num);
                if (!is_engine_busy(ob, chip_num, engine_num)) {

                    // TODO: We are not currently taking advantage of the queued job ability

                    // applog(LOG_ERR, "Set nonce range: lower=%llu  upper=%llu", ob->start_of_next_nonce, ob->start_of_next_nonce + NONCE_RANGE_SIZE - 1);

                    //  Set different nonce ranges
                    ApiError error = ob1SetNonceRange(ob->chain_id, chip_num, engine_num, ob->start_of_next_nonce, ob->start_of_next_nonce + NONCE_RANGE_SIZE - 1);
                    if (error != SUCCESS) {
                        // TODO: do something
                        goto scanwork_exit;
                    }

                    // applog(LOG_ERR, "Start job on: B%u/C%u/E%u", ob->chain_id, chip_num, engine_num);

                    // Start everything hashing!
                    // TODO: Should we start things hashing as each engine gets its nonce range
                    // instead of starting them all here at the end?  Does that slow us down much?
                    error = ob1StartJob(ob->chain_id, chip_num, engine_num);
                    if (error != SUCCESS) {
                        // TODO: do something
                        goto scanwork_exit;
                    }

                    set_engine_busy(ob, chip_num, engine_num, true);

                    // Remember what work this engine is working on, so we can use the same one
                    // when testing and submitting a nonce.
                    ob->chips[chip_num].engines_curr_work[engine_num] = ob->curr_work;

                    // Increment for next engine
                    ob->start_of_next_nonce += NONCE_RANGE_SIZE;
                }
            }
        }
    }

#elif (MODEL == DCR1)
    for (uint8_t chip_num = 0; chip_num < NUM_CHIPS_PER_BOARD; chip_num++) {
        for (uint8_t engine_num = 0; engine_num < ob1GetNumEnginesPerChip(); engine_num++) {
            // applog(LOG_ERR, "Checking if engine_num=%d is done", engine_num);
            if (!is_engine_busy(ob, chip_num, engine_num)) {
                // If there is no work, get a new one
                if (!ob->curr_work) {
                    ob->curr_work = wq_dequeue(ob, true);
                    applog(LOG_ERR, "***************************************************************");
                    applog(LOG_ERR, "Switching to new work: job_id=%s, nonce2=%lld", ob->curr_work->job_id, ob->curr_work->nonce2);
                    applog(LOG_ERR, "***************************************************************");

                    hexdump(ob->curr_work->midstate, DECRED_HEADER_SIZE);

                    // Load the current job to the current chip and engine
                    Job job;
                    // TODO: Change ob1LoadJob() to take a uint8_t* so we avoid this copy
                    // memcpy(&job.blake256, ob->curr_work->midstate, DECRED_HEADER_SIZE);

                    // TODO: Can we take advantage of the queued job and just MULTICAST to all chips whenever we switch work?
                    ApiError error = ob1LoadJob(ob->chain_id, ALL_CHIPS, ALL_ENGINES, &job);
                    if (error != SUCCESS) {
                        // TODO: do something
                        goto scanwork_exit;
                    }

                    ob->start_of_next_nonce = 0;
                }

                // TODO: We are not currently taking advantage of the queued job ability

                // applog(LOG_ERR, "Set nonce range: lower=%llu  upper=%llu", ob->start_of_next_nonce, ob->start_of_next_nonce + NONCE_RANGE_SIZE - 1);

                //  Set different nonce ranges
                ApiError error = ob1SetNonceRange(ob->chain_id, chip_num, engine_num, ob->start_of_next_nonce, ob->start_of_next_nonce + NONCE_RANGE_SIZE - 1);
                if (error != SUCCESS) {
                    // TODO: do something
                    goto scanwork_exit;
                }
                // applog(LOG_ERR, "Start job on: B%u/C%u/E%u", ob->chain_id, chip_num, engine_num);

                // Start everything hashing!
                // TODO: Should we start things hashing as each engine gets its nonce range
                // instead of starting them all here at the end?  Does that slow us down much?
                error = ob1StartJob(ob->chain_id, chip_num, engine_num);
                if (error != SUCCESS) {
                    // TODO: do something
                    goto scanwork_exit;
                }

                set_engine_busy(ob, chip_num, engine_num, true);

                // Remember what work this engine is working on, so we can use the same one
                // when testing and submitting a nonce.
                ob->chips[chip_num].engines_curr_work[engine_num] = ob->curr_work;

                // Increment for next engine
                ob->start_of_next_nonce += NONCE_RANGE_SIZE;
                if (ob->start_of_next_nonce + NONCE_RANGE_SIZE >= 0xFFFFFFFFULL) {
                    applog(LOG_ERR, "NONCE RANGE EXHAUSTED FOR THIS WORK - NEED A NEW ONE (NONCE_RANGE_SIZE=0x%016llX", NONCE_RANGE_SIZE);
                    ob->curr_work = NULL;
                }
            }
        }
    }

    // Clear done flags - how?

#endif
    //applog(LOG_ERR, "^^^^^^^^^^^^^^^^^^ ob->active_wq.num_elems= %d (obelisk_scan_work)", ob->active_wq.num_elems);

    //applog(LOG_ERR, "sizeof(work)=%d", sizeof(struct work)); // 560 bytes

    // Work seems to need to be held as long as you are still working on it.
    // dragonmint_t1.c:1009 and 1016 call free_work()
    // They also hold a pointer to the work in their chip structure

    // WORK SPLITTING
    // Decred: Split the 32-bit nonce range up into 128 segments and give
    // each chip one slice.  Have a single work item per chips instead of
    // per engine, because we would need like 6MB of work jobs otherwise,
    // which is crazy.
    //
    // Sia: Split the nonce range up similarly to the Decred chip.  A 32-bit
    // range for the whole chip, but split across 64 engines.

    // Update hashboard temperatures
    HashboardStatus status = ob1GetHashboardStatus(ob->chain_id);

    // Update the min/max/curr temps for reporting via the API
    update_temp(&ob->board_temp, status.boardTemp);
    update_temp(&ob->chip_temp, status.chipTemp);
    update_temp(&ob->psu_temp, status.powerSupplyTemp);

    // TODO: Implement these fields for realz: TEMP HACK
    ob->fan_speed[0] = 2400;
    ob->fan_speed[1] = 2500;
    ob->num_chips = 15;
    ob->num_cores = 15 * 64;

    // applog(LOG_ERR, "CH%u: obelisk_scanwork() - end", ob->chain_id);
scanwork_exit:
    cgsleep_ms(10);

    return 1;
}

static struct api_data* obelisk_api_stats(struct cgpu_info* cgpu)
{
    struct ob_chain* ob = cgpu->device_data;
    struct api_data* stats = NULL;
    char buffer[32];

    applog(LOG_ERR, "***** obelisk_api_stats()");
    stats = api_add_uint32(stats, "chainId", &ob->chain_id, false);
    stats = api_add_uint16(stats, "numChips", &ob->num_chips, false);
    stats = api_add_uint16(stats, "numCores", &ob->num_cores, false);
    stats = api_add_double(stats, "boardTempLow", &ob->board_temp.low, false);
    stats = api_add_double(stats, "boardTempCurr", &ob->board_temp.curr, false);
    stats = api_add_double(stats, "boardTempHigh", &ob->board_temp.high, false);

    // BUG: Adding more stats below causes the response to never be returned.  Seems like a buffer size issue,
    //      but so far I cannot seem to find it.
    stats = api_add_double(stats, "chipTempLow", &ob->chip_temp.low, false);
    stats = api_add_double(stats, "chipTempCurr", &ob->chip_temp.curr, false);
    stats = api_add_double(stats, "chipTempHigh", &ob->chip_temp.high, false);
    stats = api_add_double(stats, "powerSupplyTempLow", &ob->psu_temp.low, false);
    stats = api_add_double(stats, "powerSupplyTempCurr", &ob->psu_temp.curr, false);
    stats = api_add_double(stats, "powerSupplyTempHigh", &ob->psu_temp.high, false);

    // These stats are per-cgpu, but the fans are global.  cgminer has
    // no support for global stats, so just repeat the fan speeds here
    // The receiving side will just pull the speeds from the first entry
    // returned.

    for (int i = 0; i < ob1GetNumFans(); i++) {
        sprintf(buffer, "fanSpeed%d", i);
        stats = api_add_int(stats, buffer, &ob->fan_speed[i], false);
    }
    applog(LOG_ERR, "***** obelisk_api_stats() DONE");

    return stats;
}

struct device_drv obelisk_drv = {
    .drv_id = DRIVER_obelisk,
    .dname = "Obelisk SC1/DCR1",
    .name = "Obelisk",
    .drv_detect = obelisk_detect,
    .get_api_stats = obelisk_api_stats,
    //.get_statline_before = obelisk_get_statline_before,
    .identify_device = obelisk_identify,
    .thread_prepare = obelisk_thread_prepare,
    .hash_work = hash_queued_work,
    .scanwork = obelisk_scanwork,
    .queue_full = obelisk_queue_full,
    .flush_work = obelisk_flush_work,
    .thread_shutdown = obelisk_shutdown
};