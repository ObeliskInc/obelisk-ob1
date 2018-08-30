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
#include "obelisk/gpio_bsp.h"
#include "obelisk/multicast.h"
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
static void control_loop(ob_chain* ob);

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

    bool retry;
    do {
        retry = false;

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

        // Discard stale work - this will cause us to loop around and dequeue work until
        // we either run out of work or find non-stale work.
        if (work && work->id < work->pool->stale_share_id) {
            applog(LOG_ERR, "HB%u: DISCARDING STALE WORK: job_id=%s  work->id=%llu  (stale_share_id=%llu)",
                ob->chain_id, work->job_id, work->id, work->pool->stale_share_id);
            free_work(work);
            work = NULL;
            retry = true;
        }
    } while (retry);

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
int siaValidNonce(struct ob_chain* ob, uint16_t chipNum, uint16_t engineNum, Nonce nonce) {
	// Create the header with the nonce set up correctly.
	struct work* engine_work = ob->chipWork[chipNum];
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
// pool difficulty but is valid under the chip difficulty, and '2' if the nonce
// is valid under both the pool difficulty and the chip difficulty.
int dcrValidNonce(struct ob_chain* ob, uint16_t chipNum, uint16_t engineNum, Nonce nonce) {
	// Create the header with the nonce and en2 set up correctly.
	struct work* engine_work = ob->chipWork[chipNum];

    uint8_t midstate[ob->staticBoardModel.midstateSize];
    memcpy(midstate, engine_work->midstate, ob->staticBoardModel.midstateSize);

    uint8_t header_tail[ob->staticBoardModel.headerTailSize];
    memcpy(header_tail, engine_work->header_tail, ob->staticBoardModel.headerTailSize);
    memcpy(header_tail + ob->staticBoardModel.nonceOffsetInTail, &nonce, sizeof(Nonce));
    memcpy(header_tail + ob->staticBoardModel.extranonce2OffsetInTail, &ob->decredEN2[chipNum][engineNum], sizeof(uint32_t));

	// Check if it meets the pool's stratum difficulty.
	if (!engine_work->pool) {
		return dcrMidstateMeetsProvidedTarget(midstate, header_tail, ob->staticChipTarget) ? 1 : 0;
	}
	return dcrHeaderMeetsChipTargetAndPoolDifficulty(midstate, header_tail, ob->staticChipTarget, engine_work->pool->sdiff);
}

// commitBoardBias will take all of the current chip biases and commit them to the
// string.
static void commitBoardBias(ob_chain* ob)
{
    ControlLoopState *state = &ob->control_loop_state;
    hashBoardModel *model = &ob->staticBoardModel;
    for (int i = 0; i < model->chipsPerBoard; i++) {
        ob1SetClockDividerAndBias(ob->staticBoardNumber, i, state->chipDividers[i], state->chipBiases[i]);
    }
    state->prevBiasChangeTime = state->currentTime;
    state->goodNoncesUponLastBiasChange = state->currentGoodNonces;

    // Write the new biases to disk.
    saveThermalConfig(model->name, ob->chain_id, state);
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
    for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
        if (biasToLevel(ob->control_loop_state.chipBiases[i], ob->control_loop_state.chipDividers[i]) >= ob->control_loop_state.curChild.maxBiasLevel) {
            return;
        }
    }
    for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
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

    ob1SetStringVoltage(ob->staticBoardNumber, level);
    state->currentVoltageLevel = level;
    state->goodNoncesUponLastVoltageChange = state->currentGoodNonces;
    state->prevVoltageChangeTime = state->currentTime;

    // Changing the voltage is significant enough to count as changing the bias
    // too.
    state->goodNoncesUponLastBiasChange = state->currentGoodNonces;
    state->prevBiasChangeTime = state->currentTime;

    // Write the new voltage level to disk.
    saveThermalConfig(model->name, ob->chain_id, state);
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
        cgsleep_ms(250);
    }

    return NULL;
}

ApiError bufferGlobalChipJob(ob_chain* ob) {
	struct work* nextWork = wq_dequeue(ob, true);
	ob->bufferedWork = nextWork;
	if (ob->bufferedWork == NULL) {
		applog(LOG_ERR, "bufferedWork is NULL: %u", ob->staticBoardNumber);
		return GENERIC_ERROR;
	}

	// Prepare a job and load it onto the chip.
	Job job = ob->prepareNextChipJob(ob);
	ApiError error = ob1LoadJob(&(ob->spiLoadJobTime), ob->chain_id, ALL_CHIPS, ALL_ENGINES, &job);
	if (error != SUCCESS) {
		return error;
	}
}

// siaPrepareNextChipJob will prepare the next job for a sia chip.
Job siaPrepareNextChipJob(ob_chain* ob) {
	Job job;
	memcpy(&job.blake2b, ob->bufferedWork->midstate, ob->staticBoardModel.headerSize);
    return job;
}

// dcrPrepareNextChipJob will prepare the next job for a decred chip.
Job dcrPrepareNextChipJob(ob_chain* ob) {
	Job job;
	memcpy(&job.blake256.v, ob->bufferedWork->midstate, ob->staticBoardModel.midstateSize);
	memcpy(&job.blake256.m, ob->bufferedWork->header_tail, ob->staticBoardModel.headerTailSize);
    return job;
}

// siaSetChipNonceRange will set the nonce range of every engine on the chip to
// a different value, offset by the nonce range of the board model.
ApiError siaSetChipNonceRange(ob_chain* ob, uint16_t chipNum, uint8_t tries) {
	// Set every engine.
	for (int engineNum = 0; engineNum < ob->staticBoardModel.enginesPerChip; engineNum++) {
		Nonce nonceStart = (chipNum * ob->staticBoardModel.enginesPerChip * ob->staticBoardModel.nonceRange) + (engineNum * ob->staticBoardModel.nonceRange);
		Nonce nonceEnd = nonceStart + ob->staticBoardModel.nonceRange-1;

		// Try each engine several times. If the engine is not set correctly on
		// the first try, try again.
		for (uint8_t i = 0; i < tries; i++) {
			// Check that the nonce range was set correctly.
			uint64_t lowerBound, upperBound;
			ob1SpiReadReg(ob->staticBoardNumber, chipNum, engineNum, E_SC1_REG_LB, &lowerBound);
			ob1SpiReadReg(ob->staticBoardNumber, chipNum, engineNum, E_SC1_REG_UB, &upperBound);
			if (lowerBound == nonceStart && upperBound == nonceEnd) {
				// Bounds set correctly, move on.
				break;
			}

			// Attempt setting the nonce range.
			ApiError error = ob1SetNonceRange(ob->chain_id, chipNum, engineNum, nonceStart, nonceEnd);
			if (error != SUCCESS) {
				continue;
			}
		}
	}
}

// dcrSetChipNonceRange will set the nonce range of every engine on the chip to
// span the full possible nonce range, which is only 2^32 for the DCR1.
ApiError dcrSetChipNonceRange(ob_chain* ob, uint16_t chipNum, uint8_t tries) {
	// Set the baseline nonces.
	Nonce nonceStart = 0x00000000;
	Nonce nonceEnd   = 0xffffffff;

	// Set every engine.
	for (int engineNum = 0; engineNum < ob->staticBoardModel.enginesPerChip; engineNum++) {
		// Try each engine several times. If the engine is not set correctly on
		// the first try, try again.
		for (uint8_t i = 0; i < tries; i++) {
			// Check that the nonce range was set correctly.
			uint64_t lowerBound, upperBound;
			ob1SpiReadReg(ob->staticBoardNumber, chipNum, engineNum, E_SC1_REG_LB, &lowerBound);
			ob1SpiReadReg(ob->staticBoardNumber, chipNum, engineNum, E_SC1_REG_UB, &upperBound);
			if (lowerBound == nonceStart && upperBound == nonceEnd) {
				// Bounds set correctly, move on.
				break;
			}

			// Attempt setting the nonce range.
			ApiError error = ob1SetNonceRange(ob->chain_id, chipNum, engineNum, nonceStart, nonceEnd);
			if (error != SUCCESS) {
				continue;
			}
		}
	}
}

ApiError siaStartNextEngineJob(ob_chain* ob, uint16_t chipNum, uint16_t engineNum) {
	return ob1StartJob(ob->staticBoardNumber, chipNum, engineNum);
}

// For Decred, we need to set the M5 register and save it somewhere that it can
// be recovered during validNonce.
ApiError dcrStartNextEngineJob(ob_chain* ob, uint16_t chipNum, uint16_t engineNum) {
	uint32_t extraNonce2 = (ob->staticBoardModel.enginesPerChip * chipNum) + engineNum + ob->bufferedWork->nonce2;
	ob->decredEN2[chipNum][engineNum] = extraNonce2;
	ApiError error = ob1SpiWriteReg(ob->staticBoardNumber, chipNum, engineNum, E_DCR1_REG_M5, &extraNonce2);
	if (error != SUCCESS) {
		return error;
	}
	error = ob1StartJob(ob->staticBoardNumber, chipNum, engineNum);
	return error;
}

// SC1A specific initialization.
void initSC1ABoard(ob_chain* ob) {
	ob->staticBoardModel = HASHBOARD_MODEL_SC1A;

	// Employ memcpy because we can't set the target directly.
	uint8_t chipTarget[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	memcpy(ob->staticChipTarget, chipTarget, 32);

	// Functions.
	ob->prepareNextChipJob = siaPrepareNextChipJob;
	ob->setChipNonceRange = siaSetChipNonceRange;
	ob->startNextEngineJob = siaStartNextEngineJob;
	ob->validNonce = siaValidNonce;
}

// DCR1A specific initialization.
void initDCR1ABoard(ob_chain* ob) {
	ob->staticBoardModel = HASHBOARD_MODEL_DCR1A;

	// Employ memcpy because we can't set the target directly.
	uint8_t chipTarget[] = { 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	memcpy(ob->staticChipTarget, chipTarget, 32);

	// Functions.
	ob->prepareNextChipJob = dcrPrepareNextChipJob;
	ob->setChipNonceRange = dcrSetChipNonceRange;
	ob->startNextEngineJob = dcrStartNextEngineJob;
	ob->validNonce = dcrValidNonce;
}

static void obelisk_detect(bool hotplug)
{
    pthread_t pth;

	// Basic initialization.
    applog(LOG_ERR, "Initializing Obelisk\n");
    ob1Initialize();
	gBoardModel = eGetBoardType(0);

    // Set the initial fan speed - control loop will take over shortly
    ob1SetFanSpeeds(100);

	// Initialize each hashboard.
    int numHashboards = ob1GetNumPresentHashboards();
    for (int i = 0; i < numHashboards; i++) {
		// Enable the persistence for this board.
        struct cgpu_info* cgpu;
        ob_chain* ob;

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
		ob->fanSpeed = 100;
        cgtimer_time(&ob->startTime);

		// Determine the type of board.
		//
		// TODO: The E_ASIC_TYPE_T is a misnomer, it actually returns the board
		// type and the tag 'MODEL_SC1' is correct for the chip type, but not
		// the board type.
		E_ASIC_TYPE_T boardType = eGetBoardType(i);
		if (boardType == MODEL_SC1) {
			initSC1ABoard(ob);
		} else if (boardType == MODEL_DCR1) {
			initDCR1ABoard(ob);
		}

        cgtime(&cgpu->dev_start_tv);

		ob->control_loop_state.currentTime = time(0);
		ob->control_loop_state.initTime = ob->control_loop_state.currentTime;
		ob->control_loop_state.stringAdjustmentTime = ob->control_loop_state.currentTime+60;
		ob->control_loop_state.prevVoltageChangeTime = ob->control_loop_state.currentTime;
		ob->control_loop_state.hasReset = false;

		// Load the thermal configuration for this machine. If that fails (no
		// configuration file, or boards changed), fallback to default values
		// based on our thermal models.
		ApiError error = loadThermalConfig(ob->staticBoardModel.name, ob->staticBoardNumber, &ob->control_loop_state);
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
			ob->control_loop_state.currentVoltageLevel = ob->staticBoardModel.minStringVoltageLevel;
			ob->control_loop_state.curChild.maxBiasLevel = ob->staticBoardModel.defaultMaxBiasLevel;
			ob->control_loop_state.curChild.initStringIncrements = ob->staticBoardModel.defaultStringIncrements;

			// Initialize the genetic algorithm.
			ob->control_loop_state.curChild.voltageLevel = ob->control_loop_state.currentVoltageLevel;
			memcpy(ob->control_loop_state.curChild.chipBiases, ob->control_loop_state.chipBiases, ob->staticBoardModel.chipsPerBoard);
			memcpy(ob->control_loop_state.curChild.chipDividers, ob->control_loop_state.chipDividers, ob->staticBoardModel.chipsPerBoard);
			ob->control_loop_state.populationSize = 0;
		}
		setVoltageLevel(ob, ob->control_loop_state.currentVoltageLevel);
		commitBoardBias(ob);

		// Set the nonce ranges for this chip.
		for (uint16_t chipNum = 0; chipNum < ob->staticBoardModel.chipsPerBoard; chipNum++) {
			ob->setChipNonceRange(ob, chipNum, 2);
		}
		ob->bufferWork = true;

		// Allocate the chip work fields.
		ob->chipWork = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(struct work*));
		ob->chipGoodNonces = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(uint64_t));
		ob->chipBadNonces = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(uint64_t));
		ob->chipStartTimes = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(cgtimer_t));
		ob->chipCheckTimes = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(cgtimer_t));

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
#define StatusOutputFrequency 60 

// Temperature measurement variables.
#define ChipTempVariance 5.0 // Temp rise of chip due to silicon inconsistencies.
#define HotChipTargetTemp 105.0 // Acceptable temp for hottest chip.

// Overtemp variables.
#define TempDeviationAcceptable 5.0 // The amount the temperature is allowed to vary from the target temperature.
#define TempDeviationUrgent 3.0 // Temp above acceptable where rapid bias reductions begin.
#define OvertempCheckFrequency 1

// Undertemp variables.
#define TempRiseSpeedHot 1
#define UndertempCheckFrequency 1

// targetTemp returns the target temperature for the chip we want.
static double getHottestDelta(ob_chain* ob)
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
	return hottestDelta;
}

// updateControlState will update fields that depend on external factors.
// Things like the time and string temperature.
static void updateControlState(ob_chain* ob)
{
    // Fetch some status variables about the hashing board.
    HashboardStatus hbStatus = ob1GetHashboardStatus(ob->staticBoardNumber);

	// TODO: Update some API level stuffs. This may not be the best place for
	// these.
	//
    // Update the min/max/curr temps for reporting via the API
    update_temp(&ob->board_temp, hbStatus.boardTemp);
    update_temp(&ob->chip_temp, hbStatus.chipTemp);
    update_temp(&ob->psu_temp, hbStatus.powerSupplyTemp);
    // TODO: Implement these fields for realz: TEMP HACK
    ob->fan_speed[0] = 2400;
    ob->fan_speed[1] = 2500;
    ob->num_chips = 15;
    ob->num_cores = 15 * 64;

	// Update the current hotChipTemp for this board, under lock.
	double hottestDelta = getHottestDelta(ob);
	double hotChipTemp = hottestDelta + ob->control_loop_state.currentStringTemp;
	ob->hotChipTemp = hotChipTemp;

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

	// Sanity check - exit with error if the voltage is at unsafe levels.
	if (ob->control_loop_state.currentStringVoltage < 6) {
		exit(-1);
	}
}

static uint64_t computeHashRate(ob_chain *ob)
{
    uint64_t goodNonces = ob->control_loop_state.goodNoncesSinceVoltageChange;
    time_t secondsElapsed = (ob->control_loop_state.currentTime - ob->control_loop_state.prevVoltageChangeTime) + 1;
    return ob->staticBoardModel.chipDifficulty * goodNonces / secondsElapsed;
}

// displayControlState will check if enough time has passed, and then display
// the current state of the hashing board to the user.
static void displayControlState(ob_chain* ob)
{
    time_t lastStatus = ob->control_loop_state.lastStatusOutput;
    time_t currentTime = ob->control_loop_state.currentTime;
	time_t totalTime = (ob->control_loop_state.currentTime - ob->control_loop_state.initTime) + 1;
    uint64_t goodNonces = ob->control_loop_state.currentGoodNonces;
    if (currentTime - lastStatus > StatusOutputFrequency) {
        // Currently only displays the bias of the first chip.
		applog(LOG_ERR, "");
        applog(LOG_ERR, "HB%u:  Temp: %-5.1f  VString: %2.02f  Time: %ds - %ds Current Hashrate: %lld GH/s - %lld GH/s  VLevel: %u",
            ob->staticBoardNumber,
            ob->hotChipTemp,
            ob->control_loop_state.currentStringVoltage,
			ob->control_loop_state.currentTime - ob->control_loop_state.prevVoltageChangeTime,
			totalTime,
            computeHashRate(ob) / 1000000000,
			ob->staticBoardModel.chipDifficulty * goodNonces / totalTime / 1000000000,
            ob->control_loop_state.currentVoltageLevel);
		applog(LOG_ERR, "");
        
		/*
        cgtimer_t currTime, totalTime;
        cgtimer_time(&currTime);
        cgtimer_sub(&currTime, &ob->startTime, &totalTime);
        applog(LOG_NOTICE, "totalTime: %d ms, totalScanWorkTime: %d ms, doneNoncetime: %d ms, chipScanTime %d ms, loadJobTime: %d ms, submitNonceTime: %d ms, readNonceTime: %d ms, statsTime: %d ms",
            cgtimer_to_ms(&totalTime),
            ob->totalScanWorkTime, 
			ob->doneNonceTime,
			ob->chipScanTime,
            ob->loadJobTime, 
            ob->obLoadJobTime, 
            ob->submitNonceTime,
            ob->readNonceTime,
			ob->statsTime);
		*/
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
        if (currentTemp < targetTemp - TempDeviationAcceptable && currentTemp - prevTemp < TempRiseSpeedHot * UndertempCheckFrequency) {
            increaseStringBias(ob);
        }
        ob->control_loop_state.prevUndertempCheck = ob->control_loop_state.currentTime;
        ob->control_loop_state.prevUndertempStringTemp = ob->control_loop_state.currentStringTemp;
    }
}

// handleFanChange will adjust the fans based on the current temperatures of all
// the boards.
static void handleFanChange(ob_chain* ob) {
	// Only board 0 manipulates the fans.
	//
	// NOTE: This means that fan control is not in place if there is no board in
	// slot zero.
	if (ob->staticBoardNumber != 0) {
		return;
	}

	// Check that enough time has elapsed since the last fan speed adjustment,
	// only happens once every 20 seconds.
	if (ob->control_loop_state.currentTime - ob->control_loop_state.lastFanAdjustment < 20) {
		return;
	}

	// Check for conditions where we change the fan speed.
	bool allOver98 = true;
	bool allUnder95 = true;
	bool anyUnder90 = false;
	for (int i = 0; i < ob->staticTotalBoards; i++) {
		mutex_lock(&chains[i].lock);
		double chainTemp = chains[i].hotChipTemp;
		mutex_unlock(&chains[i].lock);
		if (chainTemp <= 98) {
			allOver98 = false;
		}
		if (chainTemp >= 95) {
			allUnder95 = false;
		}
		if (chainTemp < 90) {
			anyUnder90 = true;
		}
	}

	if (allOver98 && ob->fanSpeed != 100) {
		ob->fanSpeed = 100;
		ob1SetFanSpeeds(ob->fanSpeed);
	}
	if ((allUnder95 || anyUnder90) && ob->fanSpeed > 10) {
		ob->fanSpeed = ob->fanSpeed - 5;
		ob1SetFanSpeeds(ob->fanSpeed);
	}
}

// handleOvertimeExit will exit if cgminer has been running for too long. The
// watchdog should revive cgminer. If cgminer runs for a long time, for whatever
// reason the hashrate just falls off of a cliff. So we restart cgminer after 30
// minutes.
static void handleOvertimeExit(ob_chain* ob) {
	if (ob->control_loop_state.currentTime - ob->control_loop_state.initTime > 1800) {
		exit(0);
	}
}

// handleVoltageAndBiasTuning will adjust the voltage of the string and the
// biases of the chips as deemed beneficial to the overall hashrate.
static void handleVoltageAndBiasTuning(ob_chain* ob) {
	// Determine whether the string is running slowly. The string is considered
	// to be running slowly if the chips are not producing nonces as fast as
	// expected.
	uint64_t requiredTime = 300;
	time_t resetTime = 30;
	time_t timeElapsed = ob->control_loop_state.currentTime - ob->control_loop_state.prevVoltageChangeTime;
	// The max time allowed is twice the expected amount of time for the whole
	// string to hit the required number of nonces.

	if (timeElapsed > resetTime && !ob->control_loop_state.hasReset) {
		ob->control_loop_state.hasReset = true;
		for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
			ob->chipBadNonces[i] = 0;
			ob->chipGoodNonces[i] = 0;
		}
		ob->control_loop_state.prevVoltageChangeTime = ob->control_loop_state.currentTime;
		ob->control_loop_state.goodNoncesUponLastVoltageChange = ob->control_loop_state.currentGoodNonces;
		return;
	}

	// If we haven't found enough nonces and also not too much time has passed,
	// no changges are made to voltage or bias.
	if (timeElapsed < requiredTime) {
		return;
	}

	// Set the fitness of the current child.
	ob->control_loop_state.curChild.fitness = computeHashRate(ob);

	// Set the curChild voltage and bias levels to equal what they've been
	// changed to by the temp regulation and any other external factors.
	ob->control_loop_state.curChild.voltageLevel = ob->control_loop_state.currentVoltageLevel;
	memcpy(ob->control_loop_state.curChild.chipBiases, ob->control_loop_state.chipBiases, sizeof(ob->control_loop_state.curChild.chipBiases));
	memcpy(ob->control_loop_state.curChild.chipDividers, ob->control_loop_state.chipDividers, sizeof(ob->control_loop_state.curChild.chipDividers));

	// Run the genetic algorithm, recording the performance of the current
	// settings and breeding the population to produce new settings.
	geneticAlgoIter(&ob->control_loop_state);

	// Commit the new settings and mark that an adjustment has taken place.
	setVoltageLevel(ob, ob->control_loop_state.currentVoltageLevel);
	commitBoardBias(ob);
	for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
		ob->chipBadNonces[i] = 0;
		ob->chipGoodNonces[i] = 0;
	}

	// Set the curChild voltage and bias levels to equal what they've been
	// changed to by any no-ops that occured after the voltage was updated.
	ob->control_loop_state.curChild.voltageLevel = ob->control_loop_state.currentVoltageLevel;
	ob->control_loop_state.hasReset = false;

	// cgminer will exit if it has been running for too long.
	handleOvertimeExit(ob);
}

// control_loop runs the hashing boards and attempts to work towards an optimal
// hashrate.
static void control_loop(ob_chain* ob)
{
    // Get a static view of the current state of the hashing board.
    updateControlState(ob);
    displayControlState(ob);

    // Determine the target temperature for the temperature chip.
    double hottestDelta = getHottestDelta(ob);
	double targetTemp = HotChipTargetTemp - ChipTempVariance - hottestDelta;
    handleOvertemps(ob, targetTemp);
    handleUndertemps(ob, targetTemp);
	handleFanChange(ob);

	// Perform any adjustments to the voltage and bias that may be required.
	handleVoltageAndBiasTuning(ob);
}

// TODO: need to incorporate some sort of chip check/reset in here, to try and
// save any chips which have desync'd, have had their nonce ranges reset, or
// otherwise aren't working well for some reason.
static int64_t obelisk_scanwork(__maybe_unused struct thr_info* thr)
{
	struct cgpu_info* cgpu = thr->cgpu;
	ob_chain* ob = cgpu->device_data;
	pthread_cond_signal(&ob->work_cond);

	int64_t hashesConfirmed = 0;

	// Check against the global timer to see if 100ms has passed since we
	// started the previous iteration.
	int minMSPerIter = 100;
	cgtimer_t currentTime, timeSinceLastIter;
	cgtimer_time(&currentTime);
	cgtimer_sub(&currentTime, &ob->iterationStartTime, &timeSinceLastIter);
	int msSinceLastIter = cgtimer_to_ms(&timeSinceLastIter);
	if (msSinceLastIter > minMSPerIter) {
		applog(LOG_ERR, "iter complete: %u.%i", ob->staticBoardNumber, msSinceLastIter);
	}
	if (msSinceLastIter < minMSPerIter || ob->bufferedWork == NULL) {
		// If there is a request to get work buffered into a chip, send out a
		// global message to add a new job to all chips. Then clear the flag
		// indicating that a global work object needs to be distributed.
		if (ob->bufferWork) {
			ApiError error = bufferGlobalChipJob(ob);
			if (error == SUCCESS && ob->bufferedWork != NULL) {
				ob->bufferWork = false;
			} else if (ob->bufferedWork == NULL) {
				// It is critical to the program that bufferedWork is not NULL.
				// Sleep for a bit to give some time to reset, and try again.
				applog(LOG_ERR, "error buffering a global chip job: %u", ob->staticBoardNumber);
				cgsleep_ms(25);
				return 0;
			}
		}

		// Sleep until a full 100ms have passed since the previous iteration,
		// this keeps SPI congestion to a minimum.
		cgtimer_time(&currentTime);
		cgtimer_sub(&currentTime, &ob->iterationStartTime, &timeSinceLastIter);
		msSinceLastIter = cgtimer_to_ms(&timeSinceLastIter);
		cgsleep_ms(minMSPerIter - msSinceLastIter);
	}
	// Set the start time of the current iteration.
	cgtimer_time(&ob->iterationStartTime);

	cgtimer_t lastStart, lastEnd, lastDuration;
	cgtimer_t doneStart, doneEnd, doneDuration;
	cgtimer_t readStart, readEnd, readDuration;
	cgtimer_t loadStart, loadEnd, loadDuration;
	int lastTotal = 0;
	int doneTotal = 0;
	int readTotal = 0;
	int loadTotal = 0;

	// Look for done engines, and read their nonces
	for (uint8_t chipNum = 0; chipNum < ob->staticBoardModel.chipsPerBoard; chipNum++) {
		cgtimer_time(&lastStart);

		// Check how long it has been since the last time this chip was started.
		cgtimer_t lastChipStart;
		cgtimer_sub(&currentTime, &ob->chipStartTimes[chipNum], &lastChipStart);
		int msLastChipStart = cgtimer_to_ms(&lastChipStart);
		if (msLastChipStart < 5000) {
			// It has been less than 7.5 seconds, assume that this chip is not
			// finished. 7.5 seconds would imply a clock speed of 570 MHz, which
			// we do not believe the chips are capable of. 
		cgtimer_time(&lastEnd);
		cgtimer_sub(&lastEnd, &lastStart, &lastDuration);
		lastTotal += cgtimer_to_ms(&lastDuration);
			continue;
		} else if (msLastChipStart > 120000) {
			// It has been more than 120 seconds, assume that something went
			// wrong with the chip, and that the chip needs to be started again.
			applog(LOG_ERR, "Doing a chip reset due to time out: %u.%u.%i", ob->staticBoardNumber, chipNum, msLastChipStart);
			ob->setChipNonceRange(ob, chipNum, 1);
			for (uint8_t engineNum = 0; engineNum < ob->staticBoardModel.enginesPerChip; engineNum++) {
				ApiError error = ob->startNextEngineJob(ob, chipNum, engineNum);
				if (error != SUCCESS) {
					applog(LOG_ERR, "Error loading engine job: %u.%u.%u", ob->staticBoardNumber, chipNum, engineNum);
				}
			}
			ob->chipWork[chipNum] = ob->bufferedWork;
			cgtimer_time(&ob->chipStartTimes[chipNum]);
		cgtimer_time(&lastEnd);
		cgtimer_sub(&lastEnd, &lastStart, &lastDuration);
		lastTotal += cgtimer_to_ms(&lastDuration);
			continue;
		}

		cgtimer_time(&lastEnd);
		cgtimer_sub(&lastEnd, &lastStart, &lastDuration);
		lastTotal += cgtimer_to_ms(&lastDuration);
		cgtimer_time(&doneStart);

		// Check whether the chip is done by looking at the 'GetDoneEngines'
		// read. If the first engines are reporting done, scan through the whole
		// chip. The idea behind only checking the first engines is that by the
		// time we get through them, the later engines will also be done.
		uint8_t doneBitmask[ob->staticBoardModel.enginesPerChip/8];
		ApiError error = ob1GetDoneEngines(ob->chain_id, chipNum, (uint64_t*)doneBitmask);
		if (error != SUCCESS) {
			applog(LOG_ERR, "error from GetDoneEngines: %u.%u", ob->staticBoardNumber, chipNum);
		cgtimer_time(&doneEnd);
		cgtimer_sub(&doneEnd, &doneStart, &doneDuration);
		doneTotal += cgtimer_to_ms(&doneDuration);
			continue;
		}
		// Check the first 16 engines. If 16 engines are done, all engines
		// should finish as we get to them.
		if (doneBitmask[0] != 0xff || doneBitmask[1] != 0xff) {
			cgtimer_time(&ob->chipCheckTimes[chipNum]);
		cgtimer_time(&doneEnd);
		cgtimer_sub(&doneEnd, &doneStart, &doneDuration);
		doneTotal += cgtimer_to_ms(&doneDuration);
			continue;
		}
		cgtimer_t lastCheck;
		cgtimer_sub(&currentTime, &ob->chipCheckTimes[chipNum], &lastCheck);
		int msLastCheck = cgtimer_to_ms(&lastCheck);
		if (msLastCheck > 500) {
			applog(LOG_ERR, "a chip is reporting itself as partially done: %u.%u.%i.%i", ob->staticBoardNumber, chipNum, msLastChipStart, msLastCheck);
		}

		cgtimer_time(&doneEnd);
		cgtimer_sub(&doneEnd, &doneStart, &doneDuration);
		doneTotal += cgtimer_to_ms(&doneDuration);
		cgtimer_time(&readStart);

		// Reset the timer on this chip.
		cgtimer_time(&ob->chipStartTimes[chipNum]);

		// Check all the engines on the chip.
		for (uint8_t engineNum = 0; engineNum < ob->staticBoardModel.enginesPerChip; engineNum++) {
			// Read any nonces that the engine found.
			NonceSet nonceSet;
			nonceSet.count = 0;
			error = ob1ReadNonces(ob->chain_id, chipNum, engineNum, &nonceSet);
			if (error != SUCCESS) {
				applog(LOG_ERR, "error reading nonces: %u.%u.%u", ob->staticBoardNumber, chipNum, engineNum);
				continue;
			}

			// Check the nonces and submit them to a pool if valid.
			for (uint8_t i = 0; i < nonceSet.count; i++) {
				// Check that the nonce is valid
				int nonceResult = ob->validNonce(ob, chipNum, engineNum, nonceSet.nonces[i]);
				if (nonceResult == 0) {
					ob->chipBadNonces[chipNum]++;
				}
				if (nonceResult > 0) {
					ob->goodNoncesFound++;
					ob->chipGoodNonces[chipNum]++;
					hashesConfirmed += ob->staticBoardModel.chipDifficulty;
				}
				if (nonceResult == 2) {
					// TODO: Should turn these into separate functions with ptrs
					if (gBoardModel == MODEL_SC1) {
						applog(LOG_ERR, "Submitting SC nonce=0x%08x  en2=0x%08x", nonceSet.nonces[i], ob->chipWork[chipNum]->nonce2);
						submit_nonce(cgpu->thr[0], ob->chipWork[chipNum], nonceSet.nonces[i], ob->chipWork[chipNum]->nonce2);
					} else {
						applog(LOG_ERR, "Submitting DCR nonce=0x%08x  en2=0x%08x", nonceSet.nonces[i], ob->decredEN2[chipNum][engineNum]);
						// NOTE: We byte-reverse the extranonce2 here, but not the nonce, because...who wouldn't?
						submit_nonce(cgpu->thr[0], ob->chipWork[chipNum], nonceSet.nonces[i], htonl(ob->decredEN2[chipNum][engineNum]));
					}
                }
			}

		cgtimer_time(&loadStart);

			// Start the next job for this engine.
			error = ob->startNextEngineJob(ob, chipNum, engineNum);
			if (error != SUCCESS) {
				applog(LOG_ERR, "error starting engine job: %u.%u.%u", ob->staticBoardNumber, chipNum, engineNum);
			}

		cgtimer_time(&loadEnd);
		cgtimer_sub(&loadEnd, &loadStart, &loadDuration);
		loadTotal += cgtimer_to_ms(&loadDuration);

		}

		cgtimer_time(&readEnd);
		cgtimer_sub(&readEnd, &readStart, &readDuration);
		readTotal += cgtimer_to_ms(&readDuration);

		// Mark that we need a new global chip job buffered.
		cgtimer_time(&ob->chipCheckTimes[chipNum]);
		ob->chipWork[chipNum] = ob->bufferedWork;
		ob->bufferWork = true;
	}

	if (loadTotal > 500) {
		applog(LOG_ERR, "Iter timers: %u.%i.%i.%i.%i", ob->staticBoardNumber, lastTotal, doneTotal, readTotal, loadTotal);
	}

	// See if the pool asked us to start clean on new work
	if (ob->curr_work && ob->curr_work->pool->swork.clean) {
		ob->curr_work->pool->swork.clean = false;
		ob->curr_work = NULL;
		ob->curr_work = wq_dequeue(ob, true);
	}

	return hashesConfirmed;
}

static struct api_data* obelisk_api_stats(struct cgpu_info* cgpu)
{
    struct ob_chain* ob = cgpu->device_data;
    struct api_data* stats = NULL;
    char buffer[32];

    stats = api_add_uint32(stats, "boardId", &ob->chain_id, false);
    stats = api_add_uint16(stats, "numChips", &ob->num_chips, false);
    stats = api_add_uint16(stats, "numCores", &ob->num_cores, false);
    stats = api_add_double(stats, "boardTemp", &ob->board_temp.curr, false);
    stats = api_add_double(stats, "chipTemp", &ob->chip_temp.curr, false);
    stats = api_add_double(stats, "powerSupplyTemp", &ob->psu_temp.curr, false);

    // These stats are per-cgpu, but the fans are global.  cgminer has
    // no support for global stats, so just repeat the fan speeds here
    // The receiving side will just pull the speeds from the first entry
    // returned.

    for (int i = 0; i < ob1GetNumFans(); i++) {
        sprintf(buffer, "fanSpeed%d", i);
        stats = api_add_int(stats, buffer, &ob->fan_speed[i], false);
    }

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
