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
    uint8_t headerTail[ob->staticBoardModel.headerTailSize];
    memcpy(headerTail, &engine_work->header_tail, ob->staticBoardModel.headerTailSize);
    memcpy(headerTail + ob->staticBoardModel.nonceOffsetInTail, &nonce, sizeof(Nonce));

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
    ControlLoopState *state = &ob->control_loop_state;
    hashBoardModel *model = &ob->staticBoardModel;
    for (int i = 0; i < model->chipsPerBoard; i++) {
        ob1SetClockDividerAndBias(state->boardNumber, i, state->chipDividers[i], state->chipBiases[i]);
    }
    state->prevBiasChangeTime = state->currentTime;
    state->goodNoncesUponLastBiasChange = state->currentGoodNonces;

    // Write the new biases to disk.
    saveThermalConfig(model->name, model->chipsPerBoard, ob->chain_id, state->currentVoltageLevel, state->chipBiases, state->chipDividers);
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
    ControlLoopState *state = &ob->control_loop_state;
    hashBoardModel *model = &ob->staticBoardModel;

	// Sanity check on the voltage levels.
	if (level < model->minStringVoltageLevel) {
		level = model->minStringVoltageLevel;
	} else if (level > model->maxStringVoltageLevel) {
		level = model->maxStringVoltageLevel;
	}

    ob1SetStringVoltage(state->boardNumber, level);
    state->currentVoltageLevel = level;
    state->goodNoncesUponLastVoltageChange = state->currentGoodNonces;
    state->prevVoltageChangeTime = state->currentTime;

    // Changing the voltage is significant enough to count as changing the bias
    // too.
    state->goodNoncesUponLastBiasChange = state->currentGoodNonces;
    state->prevBiasChangeTime = state->currentTime;

    // Write the new voltage level to disk.
    saveThermalConfig(model->name, model->chipsPerBoard, ob->chain_id, state->currentVoltageLevel, state->chipBiases, state->chipDividers);
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

ApiError loadNextChipJob(ob_chain* ob, uint8_t chipNum){
	struct work* nextWork = wq_dequeue(ob, true);
	ob->chipWork[chipNum] = nextWork;
	if (ob->chipWork[chipNum] == NULL) {
		applog(LOG_ERR, "chipWork is null");
		return GENERIC_ERROR;
	}

      // Prepare the job to load on the chip.
    Job job = ob->prepareNextChipJob(ob, chipNum);

    // Load job
    cgtimer_t start_ob1LoadJob, end_ob1LoadJob, duration_ob1LoadJob;
    cgtimer_time(&start_ob1LoadJob);
    ApiError error = ob1LoadJob(&(ob->spiLoadJobTime), ob->chain_id, chipNum, ALL_ENGINES, &job);
    cgtimer_time(&end_ob1LoadJob);
    cgtimer_sub(&end_ob1LoadJob, &start_ob1LoadJob, &duration_ob1LoadJob);
    ob->obLoadJobTime += cgtimer_to_ms(&duration_ob1LoadJob);
    //applog(LOG_NOTICE, "ob1LoadJob took %d ms for chip %u", cgtimer_to_ms(&duration_ob1LoadJob), chipNum);
    if (error != SUCCESS) {
        return error;
    }

    // Start job
    cgtimer_t start_ob1StartJob, end_ob1StartJob, duration_ob1StartJob;
    cgtimer_time(&start_ob1StartJob);
    error = ob1StartJob(ob->chain_id, chipNum, ALL_ENGINES);
    cgtimer_time(&end_ob1StartJob);
    cgtimer_sub(&end_ob1StartJob, &start_ob1StartJob, &duration_ob1StartJob);
    //applog(LOG_NOTICE, "ob1StartJob took %d ms for chip %u", cgtimer_to_ms(&duration_ob1StartJob), chipNum);
    if (error != SUCCESS) {
    	return error;
    }

	return SUCCESS;
}

// siaPrepareNextChipJob will prepare the next job for a sia chip.
Job siaPrepareNextChipJob(ob_chain* ob, uint8_t chipNum) {
	Job job;
	memcpy(&job.blake2b, ob->chipWork[chipNum]->midstate, ob->staticBoardModel.headerSize);
    return job;
}

// dcrPrepareNextChipJob will prepare the next job for a decred chip.
Job dcrPrepareNextChipJob(ob_chain* ob, uint8_t chipNum) {
	Job job;
	memcpy(&job.blake256.v, ob->chipWork[chipNum]->midstate, ob->staticBoardModel.midstateSize);
	memcpy(&job.blake256.m, ob->chipWork[chipNum]->header_tail, ob->staticBoardModel.headerTailSize);
    return job;
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
		ob->staticTotalBoards = numHashboards;
        cgtimer_time(&ob->startTime);

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
			ob->prepareNextChipJob = siaPrepareNextChipJob;
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
			ob->prepareNextChipJob = dcrPrepareNextChipJob;
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
		//
		// Don't change the voltage at all for the first 5 mintues.
		ob->control_loop_state.boardNumber = ob->chain_id;
		ob->control_loop_state.currentTime = time(0);
		ob->control_loop_state.stringAdjustmentTime = ob->control_loop_state.currentTime+300;

		// Load the thermal configuration for this machine. If that fails (no
		// configuration file, or boards changed), fallback to default values
		// based on our thermal models.
		//
		// TODO: use actual boardID in addition to chain_id
		ApiError error = loadThermalConfig(ob->staticBoardModel.name, ob->staticBoardModel.chipsPerBoard, ob->chain_id,
			&ob->control_loop_state.currentVoltageLevel, ob->control_loop_state.chipBiases, ob->control_loop_state.chipDividers);
		if (error != SUCCESS) {
			// Start the chip biases at 3 levels above minimum, so there is room to
			// decrease them via the startup logic.
			int8_t baseBias = MIN_BIAS;
			uint8_t baseDivider = 8;
			increaseBias(&baseBias, &baseDivider);
			increaseBias(&baseBias, &baseDivider);
			increaseBias(&baseBias, &baseDivider);
			for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
				ob->control_loop_state.chipBiases[i] = baseBias;
				ob->control_loop_state.chipDividers[i] = baseDivider;
			}

			applog(LOG_ERR, "Loading thermal settings failed; using default values");
			for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
				// Figure out the delta based on our thermal models.
				int64_t chipDelta = 0;
				if (ob->staticTotalBoards == 2 && ob->staticBoardNumber == 0) {
					chipDelta = OB1_TEMPS_HASHBOARD_2_0[i];
				} else if (ob->staticTotalBoards == 2 && ob->staticBoardNumber == 1) {
					chipDelta = OB1_TEMPS_HASHBOARD_2_1[i];
				} else if (ob->staticTotalBoards == 3 && ob->staticBoardNumber == 0) {
					chipDelta = OB1_TEMPS_HASHBOARD_3_0[i];
				} else if (ob->staticTotalBoards == 3 && ob->staticBoardNumber == 1) {
					chipDelta = OB1_TEMPS_HASHBOARD_3_1[i];
				} else if (ob->staticTotalBoards == 3 && ob->staticBoardNumber == 2) {
					chipDelta = OB1_TEMPS_HASHBOARD_3_2[i];
				}

				// Adjust the bias for this chip accordingly. Further from baseline
				// means bigger adjustment.
				while (chipDelta > 5) {
					decreaseBias(&ob->control_loop_state.chipBiases[i], &ob->control_loop_state.chipDividers[i]);
					chipDelta -= 6;
				}
				while (chipDelta < -8) {
					increaseBias(&ob->control_loop_state.chipBiases[i], &ob->control_loop_state.chipDividers[i]);
					chipDelta += 9;
				}
			}

			// Set the string voltage to the highest voltage for starting up.
			setVoltageLevel(ob, ob->staticBoardModel.minStringVoltageLevel);
		}
		commitBoardBias(ob);
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

// Status display variables.
#define ChipCount 15
#define StatusOutputFrequency 10

// Temperature measurement variables.
#define ChipTempVariance 5.0 // Temp rise of chip due to silicon inconsistencies.
#define HotChipTargetTemp 105.0 // Acceptable temp for hottest chip.

// Overtemp variables.
#define TempDeviationAcceptable 5.0 // The amount the temperature is allowed to vary from the target temperature.
#define TempDeviationUrgent 3.0 // Temp above acceptable where rapid bias reductions begin.
#define OvertempCheckFrequency 1

// Undertemp variables.
#define TempGapCold 65
#define TempGapWarm 35
#define TempRiseSpeedCold 3
#define TempRiseSpeedWarm 2
#define TempRiseSpeedHot 1
#define UndertempCheckFrequency 1

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
        uint64_t hashrate = ob->staticBoardModel.chipDifficulty * goodNonces / secondsElapsed / 1000000000;

        // Currently only displays the bias of the first chip.
		applog(LOG_ERR, "");
        applog(LOG_ERR, "HB%u:  Temp: %-5.1f  VString: %2.02f  Hashrate: %lld GH/s  VLevel: %u",
            ob->control_loop_state.boardNumber,
            ob->control_loop_state.currentStringTemp,
            ob->control_loop_state.currentStringVoltage,
            hashrate,
            ob->control_loop_state.currentVoltageLevel);
        
        cgtimer_t currTime, totalTime;
        cgtimer_time(&currTime);
        cgtimer_sub(&currTime, &ob->startTime, &totalTime);
        applog(LOG_NOTICE, "totalTime: %d ms, totalScanWorkTime: %d ms, loadJobTime: %d ms, obLoadJobTime: %d ms,\nobStartJobTime: %d ms, spiLoadJobTime: %d ms, submitNonceTime: %d ms, readNonceTime: %d ms",
            cgtimer_to_ms(&totalTime),
            ob->totalScanWorkTime, 
            ob->loadJobTime, 
            ob->obLoadJobTime, 
            ob->obStartJobTime,
            ob->spiLoadJobTime,
            ob->submitNonceTime,
            ob->readNonceTime);
		for (int chipNum = 0; chipNum < ob->staticBoardModel.chipsPerBoard; chipNum++) {
			uint64_t goodNonces = ob->chipGoodNonces[chipNum];
			uint64_t badNonces = ob->chipBadNonces[chipNum];
			uint8_t divider = ob->control_loop_state.chipDividers[chipNum];
			int8_t bias = ob->control_loop_state.chipBiases[chipNum];
			applog(LOG_ERR, "Chip %i: bias=%u.%i  good=%lld  bad=%lld", chipNum, divider, bias, goodNonces, badNonces);
		}

        ob->control_loop_state.lastStatusOutput = currentTime;
    }
}

// biasToLevel converts the bias and divider fields into a smooth level that
// goes from 0 to 43, with each step represeting a step up in clock speed.
int biasToLevel(int8_t bias, uint8_t divider) {
	if (divider == 8) {
		return 5 + bias;
	}
	if (divider == 4) {
		return 16 + bias;
	}
	if (divider == 2) {
		return 27 + bias;
	}
	if (divider == 1) {
		return 38 + bias;
	}
}

// targetTemp returns the target temperature for the chip we want.
static double getTargetTemp(ob_chain* ob)
{
	// To get the hottest delta, iterate through the 14 chips that don't have a
	// temperature senosr, use the thermal model plus their bias difference to
	// determine the delta between that chip and the measured chip.
	//
	// Chip 0 has the temp sensor, so we skip chip 0.
	int hottestDelta = 0;
	int baseBiasLevel = biasToLevel(ob->control_loop_state.chipBiases[0], ob->control_loop_state.chipDividers[0]);
	for (int i = 1; i < 15; i++) {
		// Determine how much to adjust the expected temp based on the chip's
		// bias level.
		int biasLevel = biasToLevel(ob->control_loop_state.chipBiases[i], ob->control_loop_state.chipDividers[i]);
		int biasDelta = 0;
		if (biasLevel > baseBiasLevel) {
			biasDelta = (biasLevel-baseBiasLevel)*2;
		} else {
			biasDelta = (baseBiasLevel-biasLevel)*1;
		}

		// If there's only one board, assume a delta of 25. We don't have
		// information for this board.
		int chipDelta = 25; // Default assumption if we don't have information on this chip.
		if (ob->staticTotalBoards == 2 && ob->staticBoardNumber == 0) {
			chipDelta = OB1_TEMPS_HASHBOARD_2_0[i];
		} else if (ob->staticTotalBoards == 2 && ob->staticBoardNumber == 1) {
			chipDelta = OB1_TEMPS_HASHBOARD_2_1[i];
		} else if (ob->staticTotalBoards == 3 && ob->staticBoardNumber == 0) {
			chipDelta = OB1_TEMPS_HASHBOARD_3_0[i];
		} else if (ob->staticTotalBoards == 3 && ob->staticBoardNumber == 1) {
			chipDelta = OB1_TEMPS_HASHBOARD_3_1[i];
		} else if (ob->staticTotalBoards == 3 && ob->staticBoardNumber == 2) {
			chipDelta = OB1_TEMPS_HASHBOARD_3_2[i];
		}

		// Compute the full delta for the chip and see if it's the new hottest
		// delta.
		int fullDelta = biasDelta + chipDelta;
		if (fullDelta > hottestDelta) {
			hottestDelta = fullDelta;
		}
	}

	return HotChipTargetTemp - ChipTempVariance - hottestDelta;
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

	// Skip the string adjustments if we have not reached the next string
	// adjustment period.
	if (ob->control_loop_state.currentTime < ob->control_loop_state.stringAdjustmentTime) {
		return;
	}

	// Enough time has passed to perform chip adjustments.
	applog(LOG_ERR, "Beginning chip adjustments.");

	// Iterate through the chips and count the number of bad chips.
	int badChips = 0;
	for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
		if (ob->chipGoodNonces[i]/3 < ob->chipBadNonces[i]) {
			badChips++;
		}
	}

	// If there are more than 3 bad chips, we need to increase the string
	// voltage.
	bool atMinLevel = ob->control_loop_state.currentVoltageLevel <= ob->staticBoardModel.minStringVoltageLevel;
	if (badChips > 3 && !atMinLevel) {
		// Decrease the string voltage level and set the timeout for the next
		// level to be much higher.
		applog(LOG_ERR, "Increasing string voltage due to too many bad chips.");
		ob->control_loop_state.chipAdjustments = 0;
		ob->control_loop_state.stringAdjustmentTime = ob->control_loop_state.currentTime + 1200;
		for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
			ob->chipBadNonces[i] = 0;
			ob->chipGoodNonces[i] = 0;
		}
		setVoltageLevel(ob, ob->control_loop_state.currentVoltageLevel-1);
		return;
	} else if (badChips > 1 && ob->control_loop_state.chipAdjustments > 10 && !atMinLevel) {
		// Decrease the string voltage level.
		applog(LOG_ERR, "Increasing string voltage due to too many adjustments for bad chips.");
		ob->control_loop_state.chipAdjustments = 0;
		ob->control_loop_state.stringAdjustmentTime = ob->control_loop_state.currentTime + 1200;
		for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
			ob->chipBadNonces[i] = 0;
			ob->chipGoodNonces[i] = 0;
		}
		setVoltageLevel(ob, ob->control_loop_state.currentVoltageLevel-1);
		return;
	} else if (badChips == 0) {
		applog(LOG_ERR, "Decreasing string voltage beacuse there are no bad chips.");
		// Incrase the string voltage level.
		ob->control_loop_state.chipAdjustments = 0;
		ob->control_loop_state.stringAdjustmentTime = ob->control_loop_state.currentTime + 300;
		for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
			ob->chipBadNonces[i] = 0;
			ob->chipGoodNonces[i] = 0;
		}
		setVoltageLevel(ob, ob->control_loop_state.currentVoltageLevel+1);
		return;
	}

	// Decerase the bias on any of the bad chips.
	applog(LOG_ERR, "Adjusting the voltage on some bad chips.");
	for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
		if (ob->chipGoodNonces[i]/3 < ob->chipBadNonces[i]) {
			decreaseBias(&ob->control_loop_state.chipBiases[i], &ob->control_loop_state.chipDividers[i]);
		}
	}
	commitBoardBias(ob);
	// Mark that an adjustment has taken place.
	ob->control_loop_state.chipAdjustments++;
	ob->control_loop_state.stringAdjustmentTime = ob->control_loop_state.currentTime + 300;
	for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
		ob->chipBadNonces[i] = 0;
		ob->chipGoodNonces[i] = 0;
	}


	/*
	// Determine if the time has come to start collecting hashrate for the
	// string.
	if (!ob->settings.started && ob->control_loop_state.currentTime > ob->settings.startTime) {
		applog(LOG_ERR, "Starting metrics collection for current string.");
		ob->settings.started = true;
		ob->settings.startGoodNonces = ob->control_loop_state.currentGoodNonces;
	}

	// Determine if the time has come to switch to the next settings.
	if (ob->control_loop_state.currentTime > ob->settings.Endtime) {
		ob->settings.endGoodNonces = ob->control_loop_state.currentGoodNonces;
	}
	*/
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

    cgtimer_t start_scanwork, end_scanwork, duration_scanwork;
    cgtimer_time(&start_scanwork);

	// Get flags indicating which chips are done, and which chips have nonce
	// flags.
	uint16_t boardDoneFlags = 0;
	ApiError error = ob1ReadBoardDoneFlags(ob->staticBoardNumber, &boardDoneFlags);
	if (error != SUCCESS) {
		applog(LOG_ERR, "Failed to read board done flags.");
		return 0;
	}
	/*
	uint16_t nonceFoundFlags = 0;
	error = ob1ReadBoardNonceFlags(ob->staticBoardNumber, &nonceFoundFlags);
	if (error != SUCCESS) {
		applog(LOG_ERR, "Failed to read nonce done flags.");
		return 0;
	}
	*/

    // Look for done engines, and read their nonces
    for (uint8_t chip_num = 0; chip_num < ob->staticBoardModel.chipsPerBoard; chip_num++) {
		// If the chip does not appear to have work, give it work.
        cgtimer_t start_give_work, end_give_work, duration_give_work;
		if(ob->chipWork[chip_num] == NULL) {
            cgtimer_time(&start_give_work);
			loadNextChipJob(ob, chip_num);
            cgtimer_time(&end_give_work);
            cgtimer_sub(&end_give_work, &start_give_work, &duration_give_work);
            ob->loadJobTime += cgtimer_to_ms(&duration_give_work);
            //applog(LOG_NOTICE, "loadNextChipJob for chip %u took %d ms", chip_num, cgtimer_to_ms(&duration_give_work));
			continue;
		}

		// If the chip is not reporting as done, continue.
		bool chipDone = (boardDoneFlags >> chip_num) & 1;
		if (!chipDone) {
			continue;
		}

		// Check whether the chip is done by looking at the 'GetDoneEngines'
		// read. This is necessary because our board level check can have false
		// positives.
        uint8_t doneBitmask[ob->staticBoardModel.enginesPerChip/8];
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
		// Compare whole chip done reading to board done flags.
		if (!wholeChipDone) {
			// applog(LOG_ERR, "False positive detected on the chip done message: %u.%u", ob->staticBoardNumber, chip_num);
			continue;
		}

		// Check if this chip found a nonce. If not, move on.
		/*
		bool chipHasNonce = (nonceFoundFlags >> chip_num) & 1;
		if (!chipHasNonce) {
			// continue;
		}
		*/

		// Check all the engines on the chip.
		for (uint8_t engine_num = 0; engine_num < ob->staticBoardModel.enginesPerChip; engine_num++) {
			// Read any nonces that the engine found.
			NonceSet nonce_set;
			nonce_set.count = 0;
            cgtimer_t start_readNonces, end_readNonces, duration_readNonces;
            cgtimer_time(&start_readNonces);
			error = ob1ReadNonces(ob->chain_id, chip_num, engine_num, &nonce_set);
            cgtimer_time(&end_readNonces);
            cgtimer_sub(&end_readNonces, &start_readNonces, &duration_readNonces);
            ob->readNonceTime += cgtimer_to_ms(&duration_readNonces);
			if (error != SUCCESS) {
				applog(LOG_ERR, "Error reading nonces.");
				continue;
			}

			/*
			if (nonce_set.count == 0 && chipHasNonce) {
				// applog(LOG_ERR, "chip reported a nonce, but there is no nonce here.");
			}
			if (nonce_set.count > 0 && !chipHashNonce) {
				applog(LOG_ERR, "----- would have skipped over a nonce -----");
			}
			*/

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
                    cgtimer_t start_submit_nonce, end_submit_nonce, duration_submit_nonce;
                    cgtimer_time(&start_submit_nonce);
					submit_nonce(cgpu->thr[0], ob->chipWork[chip_num], nonce_set.nonces[i]);
                    cgtimer_time(&end_submit_nonce);
                    cgtimer_sub(&end_submit_nonce, &start_submit_nonce, &duration_submit_nonce);
                    ob->submitNonceTime += cgtimer_to_ms(&duration_submit_nonce);
                    //applog(LOG_NOTICE, "submit_nonce for chip %u/%u took %d ms", chip_num, engine_num, cgtimer_to_ms(&duration_submit_nonce));
            	}
			}
        }

		// Give a new job to the chip.
		// Get the next job.
        cgtimer_time(&start_give_work);
		loadNextChipJob(ob, chip_num);
        cgtimer_time(&end_give_work);
        cgtimer_sub(&end_give_work, &start_give_work, &duration_give_work);
        ob->loadJobTime += cgtimer_to_ms(&duration_give_work);
        //applog(LOG_NOTICE, "loadNextChipJob for chip %u took %d ms", chip_num, cgtimer_to_ms(&duration_give_work));
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
    cgsleep_ms(10);

    cgtimer_time(&end_scanwork);
    cgtimer_sub(&end_scanwork, &start_scanwork, &duration_scanwork);
    ob->totalScanWorkTime += cgtimer_to_ms(&duration_scanwork);

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
