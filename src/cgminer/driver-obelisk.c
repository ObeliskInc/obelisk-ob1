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

SC1:
./cgminer --url us-west.luxor.tech:3333 --user 79cc3a0572619864e6a45b0aa8c2294db3d27151ff88ed7e066e33ee545841c13d4797297324.obelisk --pass x --api-listen --api-allow W:127.0.0.1 --log 4 --protocol-dump --syslog

DCR1:
./cgminer --url us-west.luxor.tech:4445 --user DsViAH5o1dUUe1SjjxpNK9m7g8KrqWeAYW5.obelisk --pass x --api-listen --api-allow W:127.0.0.1 --log 4 --protocol-dump --syslog



*/

#include "miner.h"
#include "driver-obelisk.h"
#include "compat.h"
#include "config.h"
#include "klist.h"
#include "sha2.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// HACK:
extern void dump(unsigned char* p, int len, char* label);

#include "obelisk/siahash/siaverify.h"
#include "obelisk/dcrhash/dcrverify.h"

static int num_chains = 0;
static ob_chain chains[MAX_CHAIN_NUM];

#if (MODEL == SC1)
Nonce SiaNonceLowerBound = 0;
Nonce SigNonceUpperBound = 0xFFFFFFFF;
#endif

static void control_loop(ob_chain* ob);

void add_good_nonces(ob_chain* ob, uint64_t amt);
void add_bad_nonces(ob_chain* ob, uint64_t amt);


static void wq_enqueue(struct thr_info* thr, ob_chain* ob)
{
    struct work_queue* wq;
    wq = &ob->active_wq;

    while (wq->num_elems < MAX_WQ_SIZE) {
        struct work* work = get_work(thr, thr->id);
        // applog(LOG_ERR, "wq_enqueue() got work");
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

    // Discard stale work
    if (work->id < work->pool->stale_share_id) {
        applog(LOG_ERR, "DISCARDING STALE WORK: job_id=%s  work->id=%llu  (stale_share_id=%llu)", work->job_id, work->id, work->pool->stale_share_id);
        free_work(work);
        work = NULL;
    }

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
            // applog(LOG_ERR, "Calling wq_enqueue() for thread %d", ob->chain_id);
            wq_enqueue(cgpu->thr[0], ob);
        }

        mutex_lock(&ob->lock);
    }

    return NULL;
}

// siaValidNonce returns '0' if the nonce is not valid under either the pool
// difficulty nor the chip difficulty, '1' if the nonce is not valid under the
// pool difficulty but is valid under the chip difficutly, and '2' if the nonce
// is valid under both the pool difficulty and the chip difficulty.
int siaValidNonce(struct ob_chain* ob, struct work* engine_work, Nonce nonce) {
	// Create the header with the nonce set up correctly.
    uint8_t header[ob->staticBoardModel.headerSize];
    memcpy(header, engine_work->midstate, ob->staticBoardModel.headerSize);
    memcpy(header + ob->staticBoardModel.nonceOffset, &nonce, sizeof(Nonce));

    // Check if it meets the pool's stratum difficulty
	if (!engine_work->pool) {
		return siaHeaderMeetsProvidedTarget(header, ob->staticChipTarget) ? 1 : 0;
	}
	return siaHeaderMeetsChipTargetAndPoolDifficulty(header, ob->staticChipTarget, engine_work->pool->sdiff);
}

// dcrValidNonce returns '0' if the nonce is not valid under either the pool
// difficulty nor the chip difficulty, '1' if the nonce is not valid under the
// pool difficulty but is valid under the chip difficutly, and '2' if the nonce
// is valid under both the pool difficulty and the chip difficulty.
int dcrValidNonce(struct ob_chain* ob, struct work* engine_work, Nonce nonce) {
    // Create the midstate + tail with the nonce set up correctly.
    uint8_t headerTail[DECRED_HEADER_TAIL_SIZE];
    memcpy(headerTail, &engine_work->header_tail, DECRED_HEADER_TAIL_SIZE);
    memcpy(headerTail + DECRED_HEADER_TAIL_NONCE_OFFSET, &nonce, sizeof(Nonce));

	// Check if it meets the pool's stratum difficulty.
	if (!engine_work->pool) {
		return dcrMidstateMeetsProvidedTarget(engine_work->midstate, headerTail, ob->staticChipTarget) ? 1 : 0;
	}
	return dcrHeaderMeetsChipTargetAndPoolDifficulty(engine_work->midstate, headerTail, ob->staticChipTarget, engine_work->pool->sdiff);
}

// increaseDivider will increase the clock divider, resulting in a slower chip.
static void increaseDivider(uint8_t* divider)
{
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

static void decreaseDivider(uint8_t* divider)
{
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

static void increaseBias(int8_t* currentBias, uint8_t* currentDivider)
{
    if (*currentBias == MAX_BIAS) {
        if (*currentDivider > 1) {
            decreaseDivider(currentDivider);
            *currentBias = MIN_BIAS;
        }
    } else {
        *currentBias += 1;
    }
}

static void decreaseBias(int8_t* currentBias, uint8_t* currentDivider)
{
    if (*currentBias == MIN_BIAS) {
        if (*currentDivider < 8) {
            increaseDivider(currentDivider);
            *currentBias = MAX_BIAS;
        }
    } else {
        *currentBias -= 1;
    }
}

// commitBoardBias will take all of the current chip biases and commit them to the
// string.
static void commitBoardBias(ob_chain* ob)
{
    int i = 0;
    for (i = 0; i < 15; i++) {
        ob1SetClockDividerAndBias(ob->control_loop_state.boardNumber, i, ob->control_loop_state.chipDividers[i], ob->control_loop_state.chipBiases[i]);
    }
    ob->control_loop_state.prevBiasChangeTime = ob->control_loop_state.currentTime;
    ob->control_loop_state.goodNoncesUponLastBiasChange = ob->control_loop_state.currentGoodNonces;
}

// decrease the clock bias of every chip on the string.
static void decreaseStringBias(ob_chain* ob)
{
    int i = 0;
    for (i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
        decreaseBias(&ob->control_loop_state.chipBiases[i], &ob->control_loop_state.chipDividers[i]);
    }
    commitBoardBias(ob);
}

// increase the clock bias of every chip on the string.
static void increaseStringBias(ob_chain* ob)
{
    int i = 0;
    for (i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
        increaseBias(&ob->control_loop_state.chipBiases[i], &ob->control_loop_state.chipDividers[i]);
    }
    commitBoardBias(ob);
}

static void setVoltageLevel(ob_chain* ob, uint8_t level)
{
    ob1SetStringVoltage(ob->control_loop_state.boardNumber, level);
    ob->control_loop_state.currentVoltageLevel = level;
    ob->control_loop_state.goodNoncesUponLastVoltageChange = ob->control_loop_state.currentGoodNonces;
    ob->control_loop_state.prevVoltageChangeTime = ob->control_loop_state.currentTime;

    // Changing the voltage is significant enough to count as changing the bias
    // too.
    ob->control_loop_state.goodNoncesUponLastBiasChange = ob->control_loop_state.currentGoodNonces;
    ob->control_loop_state.prevBiasChangeTime = ob->control_loop_state.currentTime;
}

// Separate thread to handle the control loop so we can react quickly to temp changes
static void* ob_control_thread(void* arg)
{
    struct cgpu_info* cgpu = arg;
    ob_chain* ob = cgpu->device_data;
    char tname[16];

    sprintf(tname, "ob_control_%d", ob->chain_id);
    RenameThread(tname);

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

// siaLoadNextChipJob will load a job into a chip.
ApiError siaLoadNextChipJob(ob_chain* ob, uint8_t chipNum) {
	struct work* nextWork = wq_dequeue(ob, true);
	// Mark what job the chip has.
	ob->chipWork[chipNum] = nextWork;
	if (nextWork == NULL) {
		return GENERIC_ERROR;
	}

	// Load the job to the chip.
	Job job;
	memcpy(&job.blake2b, ob->chipWork[chipNum]->midstate, ob->staticBoardModel.headerSize);
	ApiError error = ob1LoadJob(ob->chain_id, chipNum, ALL_ENGINES, &job);
	if (error != SUCCESS) {
		return error;
	}

	// Start the job on the chip.
	error = ob1StartJob(ob->chain_id, chipNum, ALL_ENGINES);
	if (error != SUCCESS) {
		return error;
	}
	return SUCCESS;
}

// dcrLoadNextChipJob will load the next job onto a decred chip.
ApiError dcrLoadNextChipJob(ob_chain* ob, uint8_t chipNum) {
	struct work* nextWork = wq_dequeue(ob, true);
	ob->chipWork[chipNum] = nextWork;
	if (nextWork == NULL) {
		applog(LOG_ERR, "nextWork is NULL");
		return GENERIC_ERROR;
	}

	// Load the current job to the current chip and engine
	Job job;
	// TODO: Change ob1LoadJob() to take a uint8_t* so we avoid this copy
	memcpy(&job.blake256.v, nextWork, DECRED_MIDSTATE_SIZE);
	memcpy(&job.blake256.m, nextWork->header_tail, DECRED_HEADER_TAIL_SIZE);
	job.blake256.is_nonce2_roll_only = nextWork->is_nonce2_roll_only;

	// Load the job onto the chip.
	ApiError error = ob1LoadJob(ob->chain_id, chipNum, ALL_ENGINES, &job);
	if (error != SUCCESS) {
		applog(LOG_ERR, "ob1LoadJob failed");
		return error;
	}

	// Start the job on the chip.
	error = ob1StartJob(ob->chain_id, chipNum, ALL_ENGINES);
	if (error != SUCCESS) {
		applog(LOG_ERR, "ob1StartJob failed");
		return error;
	}
	return SUCCESS;
}


static void obelisk_detect(bool hotplug)
{
	// Basic initialization.
    applog(LOG_ERR, "Initializing Obelisk\n");
    ob1Initialize();
	gBoardModel = eGetBoardType(0);

	// Initialize each hashboard.
    int numHashboards = ob1GetNumPresentHashboards();
    for (int i = 0; i < numHashboards; i++) {
		// Enable the persistence for this board.
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

		// Set the board number.
		ob->staticBoardNumber = i;

		// Determine the type of board.
		//
		// TODO: The E_ASIC_TYPE_T is a misnomer, it actually returns the board
		// type.
		//
		// TODO: The tag 'MODEL_SC1' is correct for the chip type, but not the
		// board type.
		//
		// TODO: Switch to a case statement.
		E_ASIC_TYPE_T boardType = eGetBoardType(i);
		if (boardType == MODEL_SC1) {
			ob->staticBoardModel = HASHBOARD_MODEL_SC1A;

			// Employ memcpy because we can't set the target directly.
			uint8_t chipTarget[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
			memcpy(ob->staticChipTarget, chipTarget, 32);

			// Use a proxy because we can't set the value directly to 1 << 40.
			uint64_t hashesPerNonce = 1;
			hashesPerNonce = hashesPerNonce << 40;
			ob->staticHashesPerSuccessfulNonce = hashesPerNonce;

			// Functions.
			ob->loadNextChipJob = siaLoadNextChipJob;
			ob->validNonce = siaValidNonce;
		} else if (boardType == MODEL_DCR1) {
			ob->staticBoardModel = HASHBOARD_MODEL_DCR1A;

			// Employ memcpy because we can't set the target directly.
			uint8_t chipTarget[] = { 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
			memcpy(ob->staticChipTarget, chipTarget, 32);

			// Use a proxy because we can't set the value directly to 1 << 40.
			uint64_t hashesPerNonce = 1;
			hashesPerNonce = hashesPerNonce << 32;
			ob->staticHashesPerSuccessfulNonce = hashesPerNonce;

			// Functions.
			ob->loadNextChipJob = dcrLoadNextChipJob;
			ob->validNonce = dcrValidNonce;
		}

        cgtime(&cgpu->dev_start_tv);

        // TODO: Add this if necessary later
        //ob->lastshare = cgpu->dev_start_tv.tv_sec;

        // Initialize the ob chip fields
        for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
            struct chip_info* chip = &ob->chips[i];
            chip->engines_curr_work = cgcalloc(sizeof(struct work*), ob->staticBoardModel.enginesPerChip);
        }

		// Set the board number.
		ob->control_loop_state.boardNumber = ob->chain_id;
		ob->control_loop_state.currentTime = time(0);

		// Set the chip biases to minimum.
		int8_t baseBias = MIN_BIAS;
		uint8_t baseDivider = 8;
		increaseBias(&baseBias, &baseDivider);
		increaseBias(&baseBias, &baseDivider);
		for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
			ob->control_loop_state.chipBiases[i] = baseBias;
			ob->control_loop_state.chipDividers[i] = baseDivider;
		}
		// Level specific bias shifting.
		decreaseBias(&ob->control_loop_state.chipBiases[1], &ob->control_loop_state.chipDividers[1]);
		decreaseBias(&ob->control_loop_state.chipBiases[1], &ob->control_loop_state.chipDividers[1]);
		increaseBias(&ob->control_loop_state.chipBiases[3], &ob->control_loop_state.chipDividers[3]);
		increaseBias(&ob->control_loop_state.chipBiases[3], &ob->control_loop_state.chipDividers[3]);
		increaseBias(&ob->control_loop_state.chipBiases[4], &ob->control_loop_state.chipDividers[4]);
		increaseBias(&ob->control_loop_state.chipBiases[4], &ob->control_loop_state.chipDividers[4]);
		increaseBias(&ob->control_loop_state.chipBiases[7], &ob->control_loop_state.chipDividers[7]);
		increaseBias(&ob->control_loop_state.chipBiases[7], &ob->control_loop_state.chipDividers[7]);
		increaseBias(&ob->control_loop_state.chipBiases[8], &ob->control_loop_state.chipDividers[8]);
		increaseBias(&ob->control_loop_state.chipBiases[8], &ob->control_loop_state.chipDividers[8]);
		increaseBias(&ob->control_loop_state.chipBiases[8], &ob->control_loop_state.chipDividers[8]);
		increaseBias(&ob->control_loop_state.chipBiases[8], &ob->control_loop_state.chipDividers[8]);
		increaseBias(&ob->control_loop_state.chipBiases[9], &ob->control_loop_state.chipDividers[9]);
		increaseBias(&ob->control_loop_state.chipBiases[10], &ob->control_loop_state.chipDividers[10]);
		increaseBias(&ob->control_loop_state.chipBiases[10], &ob->control_loop_state.chipDividers[10]);
		increaseBias(&ob->control_loop_state.chipBiases[11], &ob->control_loop_state.chipDividers[11]);
		increaseBias(&ob->control_loop_state.chipBiases[12], &ob->control_loop_state.chipDividers[12]);
		commitBoardBias(ob);

		// Set the string voltage to the highest voltage for starting up.
		setVoltageLevel(ob, ob->staticBoardModel.minStringVoltageLevel);

		// Set the nonce range for every chip.
		uint64_t nonceRangeFailures = 0;
		for (int chipNum = 0; chipNum < ob->staticBoardModel.chipsPerBoard; chipNum++) {
			Nonce nonceStart = 0;
			for (int engineNum = 0; engineNum < ob->staticBoardModel.enginesPerChip; engineNum++) {
				ApiError error = ob1SetNonceRange(ob->chain_id, chipNum, engineNum, nonceStart, nonceStart+ob->staticBoardModel.nonceRange-1);
				if (error != SUCCESS) {
					nonceRangeFailures++;
				}
				// TODO: This is pretty important, we probably need to crash/try
				// again if this fails.
				nonceStart += ob->staticBoardModel.nonceRange;
			}
		}
		// Crash the program if there were too many nonce range failures.
		if (nonceRangeFailures > 256) {
			exit(-1);
		}

		// Allocate the chip work fields.
		ob->chipWork = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(struct work*));
		ob->chipGoodNonces = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(uint64_t));
		ob->chipBadNonces = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(uint64_t));

        INIT_LIST_HEAD(&ob->active_wq.head);

        chains[i].cgpu = cgpu;
        add_cgpu(cgpu);
        cgpu->device_id = i;

        mutex_init(&ob->lock);
        pthread_cond_init(&ob->work_cond, NULL);
        pthread_create(&pth, NULL, ob_gen_work_thread, cgpu);

        pthread_cond_init(&ob->nonce_cond, NULL);
        //pthread_create(&pth, NULL, ob_nonce_thread, cgpu);
        pthread_create(&pth, NULL, ob_control_thread, cgpu);

        applog(LOG_ERR, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
        applog(LOG_ERR, "good = %llu", get_good_nonces(ob));
        applog(LOG_ERR, "bad  = %llu", get_bad_nonces(ob));
    }

    applog(LOG_ERR, "***** obelisk_detect() DONE\n");
}

static void obelisk_identify(__maybe_unused struct cgpu_info* cgpu)
{
    applog(LOG_ERR, "***** obelisk_identify()");
}

static void obelisk_flush_work(struct cgpu_info* cgpu)
{
    // applog(LOG_ERR, "***** obelisk_flush_work()");
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
#define StatusOutputFrequency 30

// Temperature measurement variables.
#define ChipTempVariance 5.0 // Temp rise of chip due to silicon inconsistencies.
#define HotChipTargetTemp 112.0 // Acceptable temp for hottest chip.
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
#define StartingStringVoltageLevel 32
#define MaxVoltageLevel 72
#define VoltageStepSize 12
#define StringStableTime 2360 // How long a string must have the same bias before being considered stable.
#define StringMaxTime 6200 // After this amount of time, the string is abandoned as being unstable.

// updateControlState will update fields that depend on external factors.
// Things like the time and string temperature.
static void updateControlState(ob_chain* ob)
{
    // Fetch some status variables about the hashing board.
    HashboardStatus hbStatus = ob1GetHashboardStatus(ob->control_loop_state.boardNumber);

    // Update status values.
    ob->control_loop_state.currentTime = time(0);
    ob->control_loop_state.currentStringTemp = hbStatus.chipTemp;
    ob->control_loop_state.currentStringVoltage = hbStatus.asicV15;

    // Fetch nonce count updates.
    mutex_lock(&ob->lock);
    ob->control_loop_state.currentGoodNonces = ob->goodNoncesFound;
    mutex_unlock(&ob->lock);
    ob->control_loop_state.goodNoncesSinceBiasChange = ob->control_loop_state.currentGoodNonces - ob->control_loop_state.goodNoncesUponLastBiasChange;
    ob->control_loop_state.goodNoncesSinceVoltageChange = ob->control_loop_state.currentGoodNonces - ob->control_loop_state.goodNoncesUponLastVoltageChange;
}

// displayControlState will check if enough time has passed, and then display
// the current state of the hashing board to the user.
static void displayControlState(ob_chain* ob)
{
    time_t lastStatus = ob->control_loop_state.lastStatusOutput;
    time_t currentTime = ob->control_loop_state.currentTime;
    if (currentTime - lastStatus > StatusOutputFrequency) {
        // Compute the hashrate.
        uint64_t goodNonces = ob->control_loop_state.goodNoncesSinceVoltageChange;
        time_t secondsElapsed = ob->control_loop_state.currentTime - ob->control_loop_state.prevVoltageChangeTime + 1;
        uint64_t hashrate = 1099 * goodNonces / secondsElapsed;

        // Currently only displays the bias of the first chip.
		applog(LOG_ERR, "");
        applog(LOG_ERR, "HB%u:  Temp: %-5.1f  VString: %2.02f  Hashrate: %lld GH/s  VLevel: %u",
            ob->control_loop_state.boardNumber,
            ob->control_loop_state.currentStringTemp,
            ob->control_loop_state.currentStringVoltage,
            hashrate,
            ob->control_loop_state.currentVoltageLevel);
		for (int chipNum = 0; chipNum < ob->staticBoardModel.chipsPerBoard; chipNum++) {
			uint64_t goodNonces = ob->chipGoodNonces[chipNum];
			uint64_t badNonces = ob->chipBadNonces[chipNum];
			uint8_t divider = ob->control_loop_state.chipDividers[chipNum];
			int8_t bias = ob->control_loop_state.chipBiases[chipNum];
			applog(LOG_ERR, "Chip %u: bias=%u.%i  good=%u  bad=%u", chipNum, divider, bias, goodNonces, badNonces);
		}

        ob->control_loop_state.lastStatusOutput = currentTime;
    }
}

// targetTemp returns the target temperature for the chip we want.
static double getTargetTemp(ob_chain* ob)
{
    // TODO: As we start playing with the individual biases of the chips, we can
    // extend this function to take into account all the ways that the chips
    // have been pushed around.
    return HotChipTargetTemp - ChipTempVariance - HotChipTempRise;
}

// handleOvertemps will clock down the string if the string is overheating.
//
// TODO: Add the deviation growth bits for unstable strings.
static void handleOvertemps(ob_chain* ob, double targetTemp)
{
    time_t timeElapsed = ob->control_loop_state.currentTime - ob->control_loop_state.prevOvertempCheck;
    double currentTemp = ob->control_loop_state.currentStringTemp;
    if (timeElapsed > OvertempCheckFrequency) {
        // Reduce the string bias if we are overtemp.
        if (currentTemp > targetTemp + TempDeviationAcceptable) {
            decreaseStringBias(ob);
        }
        // Rapidly reduce the string bias again if we are at an urgent
        // temperature.
        if (currentTemp > targetTemp + TempDeviationAcceptable + TempDeviationUrgent) {
            decreaseStringBias(ob);
        }
        ob->control_loop_state.prevOvertempCheck = ob->control_loop_state.currentTime;
    }
}

// handleUndertemps will clock up the string if the string is too cold.
static void handleUndertemps(ob_chain* ob, double targetTemp)
{
    time_t timeElapsed = ob->control_loop_state.currentTime - ob->control_loop_state.prevUndertempCheck;
    double currentTemp = ob->control_loop_state.currentStringTemp;
    double prevTemp = ob->control_loop_state.prevUndertempStringTemp;
    if (timeElapsed > UndertempCheckFrequency) {
        if (currentTemp < targetTemp - TempGapCold && currentTemp - prevTemp < TempRiseSpeedCold * UndertempCheckFrequency) {
            increaseStringBias(ob);
        } else if (currentTemp < targetTemp - TempGapWarm && currentTemp - prevTemp < TempRiseSpeedWarm * UndertempCheckFrequency) {
            increaseStringBias(ob);
        } else if (currentTemp < targetTemp - TempDeviationAcceptable && currentTemp - prevTemp < TempRiseSpeedHot * UndertempCheckFrequency) {
            increaseStringBias(ob);
        }
        ob->control_loop_state.prevUndertempCheck = ob->control_loop_state.currentTime;
        ob->control_loop_state.prevUndertempStringTemp = ob->control_loop_state.currentStringTemp;
    }
}

// control_loop runs the hashing boards and attempts to work towards an optimal
// hashrate.
static void control_loop(ob_chain* ob)
{
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
    if (currentLevel + VoltageStepSize >= MaxVoltageLevel) {
        // TODO: Begin playing with chip bias.
        return;
    }

    // Check if the string is stable or not. If yes, use hashrate since last
    // bias change. If not, use hashrate since last voltage change.
    uint64_t hashrate = 0;
    if (currentTime - lastBiasChange < StringStableTime && currentTime - lastVoltageChange >= StringMaxTime) {
        // The string is not stable, instead has timed out. Use hashrate over
        // lifetime of string to estimate hashrate.
        // applog(LOG_ERR, "HB%u: string is victim of timeout", ob->control_loop_state.boardNumber);
        ob->control_loop_state.stringTimeouts++;
        hashrate = ob->control_loop_state.goodNoncesSinceVoltageChange * 1099 / (currentTime - lastVoltageChange + 1);
    } else {
        // The string is stable. Use hashrate over lifetime of recent bias
        // change to esmiate hashrate.
        hashrate = ob->control_loop_state.goodNoncesSinceBiasChange * 1099 / (currentTime - lastBiasChange + 1);
    }

    // The string has reached a state where it is time to try the next voltage.
    ob->control_loop_state.voltageLevelHashrates[currentLevel] = hashrate;
    setVoltageLevel(ob, currentLevel + VoltageStepSize);
    return;
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
    pthread_cond_signal(&ob->work_cond);

	int64_t hashesConfirmed = 0;

    // Look for nonces first so we can give new work in the same iteration below
    // Look for done engines, and read their nonces
    for (uint8_t chip_num = 0; chip_num < ob->staticBoardModel.chipsPerBoard; chip_num++) {
		// If the chip does not appear to have work, give it work.
		if(ob->chipWork[chip_num] == NULL) {
			ob->loadNextChipJob(ob, chip_num);
			continue;
		}

		// Check whether the chip is done.
		uint8_t* doneBitmask = malloc(ob->staticBoardModel.enginesPerChip/8);
        ApiError error = ob1GetDoneEngines(ob->chain_id, chip_num, (uint64_t*)doneBitmask);
		// Skip this chip if there was an error, or if the entire chip is not
		// done.
		if (error != SUCCESS) {
			continue;
		}
		bool wholeChipDone = true;
		for (int i = 0; i < ob->staticBoardModel.enginesPerChip/8; i++) {
			if (doneBitmask[i] != 0xff) {
				wholeChipDone = false;
				break;
			}
		}
		if (!wholeChipDone) {
			continue;
		}

		// Check all the engines on the chip.
		for (uint8_t engine_num = 0; engine_num < ob->staticBoardModel.enginesPerChip; engine_num++) {
			// Read any nonces that the engine found.
			NonceSet nonce_set;
			nonce_set.count = 0;
			error = ob1ReadNonces(ob->chain_id, chip_num, engine_num, &nonce_set);
			if (error != SUCCESS) {
				applog(LOG_ERR, "Error reading nonces.");
				continue;
			}

			// Check the nonces and submit them to a pool if valid.
			// 
			// TODO: Make sure the pool submission code is low-impact.
			for (uint8_t i = 0; i < nonce_set.count; i++) {
				// Check that the nonce is valid
				struct work* engine_work = ob->chipWork[chip_num];
				int nonceResult = ob->validNonce(ob, engine_work, nonce_set.nonces[i]);
				if (nonceResult == 0) {
					ob->chipBadNonces[chip_num]++;
				}
				if (nonceResult > 0) {
					ob->goodNoncesFound++;
					ob->chipGoodNonces[chip_num]++;
					hashesConfirmed += ob->staticHashesPerSuccessfulNonce;
				}
				if (nonceResult == 2) {
					submit_nonce(cgpu->thr[0], ob->chipWork[chip_num], nonce_set.nonces[i]);
				}
			}
        }

		// Give a new job to the chip.
		// Get the next job.
		error = ob->loadNextChipJob(ob, chip_num);
		if (error != SUCCESS) {
			applog(LOG_ERR, "Error loading chip job");
			continue;
		}
    }

    // See if the pool asked us to start clean on new work
    if (ob->curr_work && ob->curr_work->pool->swork.clean) {
        ob->curr_work->pool->swork.clean = false;
        ob->curr_work = NULL;
		ob->curr_work = wq_dequeue(ob, true);
    }

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

    return hashesConfirmed;
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
