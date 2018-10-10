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

extern int opt_ob_reboot_min_hashrate;
extern int opt_ob_disable_genetic_algo;

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
// pool difficulty but is valid under the chip difficulty, and '2' if the nonce
// is valid under both the pool difficulty and the chip difficulty.
int siaValidNonce(struct ob_chain* ob, uint16_t chipNum, uint16_t engineNum, Nonce nonce) {
	// Nonces should be divisible by the step size
	if (nonce % SC1_STEP_VAL != 0) {
		return 0;
	}

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
static void commitBoardBias(ob_chain* ob) {
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
static void decreaseStringBias(ob_chain* ob) {
	int i = 0;
	for (i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
		decreaseBias(&ob->control_loop_state.chipBiases[i], &ob->control_loop_state.chipDividers[i]);
	}
	commitBoardBias(ob);
}

// increase the clock bias of every chip on the string.
static void increaseStringBias(ob_chain* ob) {
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

static void setVoltageLevel(ob_chain* ob, uint8_t level) {
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

	for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
		ob->chipBadNonces[i] = 0;
		ob->chipGoodNonces[i] = 0;
		cgtimer_time(&ob->chipResetTimes[i]);
	}

	// Changing the voltage is significant enough to count as changing the bias
	// too.
	state->goodNoncesUponLastBiasChange = state->currentGoodNonces;
	state->prevBiasChangeTime = state->currentTime;

	// Write the new voltage level to disk.
	saveThermalConfig(model->name, ob->chain_id, state);
}

// Separate thread to handle the control loop so we can react quickly to temp changes
static void* ob_control_thread(void* arg) {
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

// bufferGlobalChipJob will send a job to all chips for work.
ApiError bufferGlobalChipJob(ob_chain* ob) {
	struct work* nextWork = wq_dequeue(ob, true);
	ob->bufferedWork = nextWork;
	if (ob->bufferedWork == NULL) {
		applog(LOG_ERR, "bufferedWork is NULL: %u", ob->staticBoardNumber);
		return GENERIC_ERROR;
	}

	// Prepare a job and load it onto the chip.
	Job job = ob->prepareNextChipJob(ob);
	return ob1LoadJob(&(ob->spiLoadJobTime), ob->chain_id, ALL_CHIPS, ALL_ENGINES, &job);
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

		// Take step size into account
		nonceStart *= SC1_STEP_VAL;
		nonceEnd *= SC1_STEP_VAL;

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
	ob->staticRigModel = MINING_RIG_MODEL_OB1;

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
	ob->staticRigModel = MINING_RIG_MODEL_OB1;

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

		// Apply user overrides
		ob->staticBoardModel.hotChipTargetTemp = opt_ob_max_hot_chip_temp_c;

		cgtime(&cgpu->dev_start_tv);

		ob->control_loop_state.currentTime = time(0);
		ob->control_loop_state.initTime = ob->control_loop_state.currentTime;
		ob->control_loop_state.lastHashrateCheckTime = ob->control_loop_state.currentTime;
		ob->control_loop_state.bootTime = ob->control_loop_state.currentTime;
		ob->control_loop_state.stringAdjustmentTime = ob->control_loop_state.currentTime+60;
		ob->control_loop_state.prevVoltageChangeTime = ob->control_loop_state.currentTime;
		ob->control_loop_state.hasReset = false;

		// Allocate the chip work fields.
		ob->chipWork = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(struct work*));
		ob->chipGoodNonces = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(uint64_t));
		ob->chipBadNonces = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(uint64_t));
		ob->chipStartTimes = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(cgtimer_t));
		ob->chipResetTimes = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(cgtimer_t));
		ob->chipCheckTimes = calloc(ob->staticBoardModel.chipsPerBoard, sizeof(cgtimer_t));

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
		ob->chipsStarted = false;

        INIT_LIST_HEAD(&ob->active_wq.head);

        chains[i].cgpu = cgpu;
        add_cgpu(cgpu);
        cgpu->device_id = i;

        mutex_init(&ob->lock);
        pthread_cond_init(&ob->work_cond, NULL);
        pthread_create(&pth, NULL, ob_gen_work_thread, cgpu);

        pthread_cond_init(&ob->nonce_cond, NULL);
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
#define StatusOutputFrequency 60 

// Overtemp variables.
#define TempDeviationAcceptable 5.0 // The amount the temperature is allowed to vary from the target temperature.
#define TempDeviationUrgent 3.0 // Temp above acceptable where rapid bias reductions begin.
#define OvertempCheckFrequency 1

// Undertemp variables.
#define TempRiseSpeedHot 1
#define UndertempCheckFrequency 1

// targetTemp returns the target temperature for the chip we want.
static double getHottestDelta(ob_chain* ob) {
	// To get the hottest delta, iterate through the 14 chips that don't have a
	// temperature sensor, use the thermal model plus their bias difference to
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
static void updateControlState(ob_chain* ob) {
	// Fetch some status variables about the hashing board.
	HashboardStatus hbStatus = ob1GetHashboardStatus(ob->staticBoardNumber);

	// TODO: Update some API level stuffs. This may not be the best place for
	// these.
	//
    // Update the min/max/curr temps for reporting via the API
    update_temp(&ob->board_temp, hbStatus.boardTemp);
    update_temp(&ob->chip_temp, hbStatus.chipTemp);
    update_temp(&ob->psu_temp, hbStatus.powerSupplyTemp);
    ob->fan_speed[0] = (int32_t)ob1GetFanRPM(0);
    ob->fan_speed[1] = (int32_t)ob1GetFanRPM(1);
    ob->num_chips = ob->staticBoardModel.chipsPerBoard;
    ob->num_cores = ob->staticBoardModel.chipsPerBoard * ob->staticBoardModel.enginesPerChip;

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

static uint64_t computeHashRate(ob_chain *ob) {
	uint64_t goodNonces = ob->control_loop_state.goodNoncesSinceVoltageChange;
	time_t secondsElapsed = (ob->control_loop_state.currentTime - ob->control_loop_state.prevVoltageChangeTime) + 1;
	return ob->staticBoardModel.chipDifficulty * goodNonces / secondsElapsed;
}

// displayControlState will check if enough time has passed, and then display
// the current state of the hashing board to the user.
static void displayControlState(ob_chain* ob) {
	time_t lastStatus = ob->control_loop_state.lastStatusOutput;
	time_t currentTime = ob->control_loop_state.currentTime;
	time_t totalTime = (ob->control_loop_state.currentTime - ob->control_loop_state.initTime) + 1;
	uint64_t goodNonces = ob->control_loop_state.currentGoodNonces;
	if (currentTime - lastStatus <= StatusOutputFrequency) {
		return;
	}

	// Display some string-wide stats.
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

	// Display some individual chip stats.
	for (int chipNum = 0; chipNum < ob->staticBoardModel.chipsPerBoard; chipNum++) {
		uint64_t goodNonces = ob->chipGoodNonces[chipNum];
		uint64_t badNonces = ob->chipBadNonces[chipNum];
		uint8_t divider = ob->control_loop_state.chipDividers[chipNum];
		int8_t bias = ob->control_loop_state.chipBiases[chipNum];
		applog(LOG_ERR, "Chip %i: bias=%u.%i  good=%lld  bad=%lld", chipNum, divider, bias, goodNonces, badNonces);
	}

	ob->control_loop_state.lastStatusOutput = currentTime;
}

// handleOvertemps will clock down the string if the string is overheating.
static void handleOvertemps(ob_chain* ob, double targetTemp) {
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
static void handleUndertemps(ob_chain* ob, double targetTemp) {
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
	if (ob->control_loop_state.currentTime - ob->control_loop_state.lastFanAdjustmentTime < 20) {
		return;
	}

	// Check for conditions where we change the fan speed.
	bool allHot = true;
	bool allBelowIdeal = true;
	bool anyCool = false;
	for (int i = 0; i < ob->staticTotalBoards; i++) {
		double chainTemp = chains[i].hotChipTemp;
		if (chainTemp <= chains[i].staticBoardModel.boardHotTemp) {
			allHot = false;
		}
		if (chainTemp >= chains[i].staticBoardModel.boardIdealTemp) {
			allBelowIdeal = false;
		}
		if (chainTemp < chains[i].staticBoardModel.boardCoolTemp) {
			anyCool = true;
		}
	}

	uint64_t maxSpeed = ob->staticRigModel.fanSpeedMax;
	uint64_t minSpeed = ob->staticRigModel.fanSpeedMin;
	uint64_t increment = ob->staticRigModel.fanSpeedIncrement;
	if (allHot && ob->fanSpeed != maxSpeed) {
		ob->fanSpeed = maxSpeed;
		ob1SetFanSpeeds(ob->fanSpeed);
	}
	if ((allBelowIdeal || anyCool) && ob->fanSpeed >= (minSpeed + increment)) {
		ob->fanSpeed -= increment;
		ob1SetFanSpeeds(ob->fanSpeed);
	}

	ob->control_loop_state.lastFanAdjustmentTime = ob->control_loop_state.currentTime;
}

// handleLowHashrateExit will exit if the hashrate of a board drops below the specified amount.
// We do this, because sometimes the hashrate drops for an unknown reason (perhaps we have lost
// some chips). This check is done every 30 minutes. The watchdog should revive cgminer.
static void handleLowHashrateExit(ob_chain* ob) {
	if (ob->control_loop_state.currentTime - ob->control_loop_state.lastHashrateCheckTime > 1800) {
		// If after at least 30 minutes, any board has fallen below the hashrate limit, then exit
		if (computeHashRate(ob) < (long long)opt_ob_reboot_min_hashrate * 1000000000LL) {
			exit(0);
		}

		// Reset the time so we check again in another 30 minutes
		ob->control_loop_state.lastHashrateCheckTime = ob->control_loop_state.currentTime;
	}
}

// Reboot the miner if it has been running for the user-specified interval in minutes
static void handlePeriodicReboot(ob_chain* ob) {
	// If user has set interval to zero, then periodic reboots are disabled
	if (ob->chain_id != 0 || opt_ob_reboot_interval_mins == 0) {
		return;
	}

	if (ob->control_loop_state.currentTime - ob->control_loop_state.bootTime > opt_ob_reboot_interval_mins * 60) {
		applog(LOG_ERR, "$$$$$$$$$$ PERIODIC REBOOT AFTER USER CONFIGURED INTERVAL OF %d MINUTES", opt_ob_reboot_interval_mins);
		doReboot();
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
	if (timeElapsed > resetTime && !ob->control_loop_state.hasReset) {
		ob->control_loop_state.hasReset = true;
		for (int i = 0; i < ob->staticBoardModel.chipsPerBoard; i++) {
			ob->chipBadNonces[i] = 0;
			ob->chipGoodNonces[i] = 0;
			cgtimer_time(&ob->chipResetTimes[i]);
		}
		ob->control_loop_state.prevVoltageChangeTime = ob->control_loop_state.currentTime;
		ob->control_loop_state.goodNoncesUponLastVoltageChange = ob->control_loop_state.currentGoodNonces;
		return;
	}

	// If we haven't found enough nonces and also not too much time has passed,
	// no changes are made to voltage or bias.
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
	// Only run the genetic algo if the user has not disabled it
	if (opt_ob_disable_genetic_algo == 0) {
		geneticAlgoIter(&ob->control_loop_state);
	}

	// Commit the new settings and mark that an adjustment has taken place.
	setVoltageLevel(ob, ob->control_loop_state.currentVoltageLevel);
	commitBoardBias(ob);

	// Set the curChild voltage and bias levels to equal what they've been
	// changed to by any no-ops that occurred after the voltage was updated.
	ob->control_loop_state.curChild.voltageLevel = ob->control_loop_state.currentVoltageLevel;
	ob->control_loop_state.hasReset = false;
}

// control_loop runs the hashing boards and attempts to work towards an optimal
// hashrate.
static void control_loop(ob_chain* ob) {
	// Get a static view of the current state of the hashing board.
	updateControlState(ob);
	displayControlState(ob);

	// Determine the target temperature for the temperature chip.
	double hottestDelta = getHottestDelta(ob);
	double targetTemp = ob->staticBoardModel.hotChipTargetTemp - ob->staticBoardModel.chipTempVariance - hottestDelta;
	handleOvertemps(ob, targetTemp);
	handleUndertemps(ob, targetTemp);
	handleFanChange(ob);

	// Perform any adjustments to the voltage and bias that may be required.
	handleVoltageAndBiasTuning(ob);

	// Exit if hashrate drops too low, as this usually means we have lost several chips.
	handleLowHashrateExit(ob);

	handlePeriodicReboot(ob);
}

///////////////////////////////////////////////////
// Scanwork specific helper functions start here //
///////////////////////////////////////////////////

// checkBufferedWork will try and fetch new buffered work if bufferedWork is
// NULL.
static bool bufferedWorkReady(ob_chain* ob) {
	if (ob->bufferedWork != NULL && !ob->bufferWork) {
		// The existing buffered work is sufficient, nothing to do.
		return true;
	}

	// Buffer a new job.
	ApiError error = bufferGlobalChipJob(ob);
	if (error != SUCCESS || ob->bufferedWork == NULL) {
		// applog(LOG_ERR, "Board %u: error buffering a global chip job.", ob->staticBoardNumber);
		cgsleep_ms(25);
		return false;
	}

	// Clear the request to have new work buffered.
	ob->bufferWork = false;
	return true;
}

// ratelimitIterations will sleep if the previous iteration happened too
// quickly.
static void ratelimitIterations(ob_chain* ob) {
	// Determine how many ms the last iteration took.
	int minMSPerIter = 100;
	cgtimer_t currentTime, timeSinceLastIter;
	cgtimer_time(&currentTime);
	cgtimer_sub(&currentTime, &ob->iterationStartTime, &timeSinceLastIter);
	int msSinceLastIter = cgtimer_to_ms(&timeSinceLastIter);

	// Sleep until a full 100ms have passed since the previous iteration, this
	// keeps SPI congestion to a minimum.
	if (msSinceLastIter < minMSPerIter) {
		cgsleep_ms(minMSPerIter - msSinceLastIter);
	}

	// Set the timer for the current iteration to now.
	cgtimer_time(&ob->iterationStartTime);
}

// isChipReady returns whether or not the chip is ready to be checked for being
// done.
static bool isChipReady(ob_chain* ob, uint64_t chipNum) {
	// Determine how many ms have passed since the last chip job was started.
	cgtimer_t currentTime, lastChipStart;
	cgtimer_time(&currentTime);
	cgtimer_sub(&currentTime, &ob->chipStartTimes[chipNum], &lastChipStart);
	int msLastChipStart = cgtimer_to_ms(&lastChipStart);

	// The chip is not ready if less than 5000ms have passed since the chip was
	// started.
	if (msLastChipStart < 5000) {
		return false;
	}
	return true;
}

// resetChipIfRequired checks whether or not the chip needs to be reset, and
// then performs the reset if required. 'true' is returned if the chip was
// reset, and 'false' is returned if the chip was not reset.
static bool resetChipIfRequired(ob_chain* ob, uint64_t chipNum) {
	// Determine how much time is supposed to pass to get to 10 nonces.
	uint64_t minNonces = 10;
	uint64_t msToReachMinNonces = 1000 * minNonces * ob->staticBoardModel.chipDifficulty / ob->staticBoardModel.chipSpeed / ob->staticBoardModel.enginesPerChip;

	// Determine how many ms have passed since the last reset.
	cgtimer_t currentTime, lastReset;
	cgtimer_time(&currentTime);
	cgtimer_sub(&currentTime, &ob->chipResetTimes[chipNum], &lastReset);
	int msLastReset = cgtimer_to_ms(&lastReset);

	// The chip does not need a reset if not enough time has passed to be
	// statistically significant. At least 30 seconds is also required.
	if (msLastReset < msToReachMinNonces || msLastReset < 30000) {
		return false;
	}

	// The chip does not need a reset if there are enough good nonces.
	uint64_t goodNonces = ob->chipGoodNonces[chipNum];
	uint64_t expectedNonces = msLastReset * ob->staticBoardModel.chipSpeed / 1000 / ob->staticBoardModel.nonceRange;
	if (goodNonces >= expectedNonces) {
		return false;
	}

	// Reset the chip.
	applog(LOG_ERR, "Performing a chip reset due to performance issues: %u.%u.%lld.%lld.%i", ob->staticBoardNumber, chipNum, goodNonces, expectedNonces, msLastReset);
	ob->setChipNonceRange(ob, chipNum, 1);
	for (uint8_t engineNum = 0; engineNum < ob->staticBoardModel.enginesPerChip; engineNum++) {
		ob->startNextEngineJob(ob, chipNum, engineNum);
	}
	ob->chipWork[chipNum] = ob->bufferedWork;
	cgtimer_time(&ob->chipStartTimes[chipNum]);
	cgtimer_time(&ob->chipResetTimes[chipNum]);
	ob->chipGoodNonces[chipNum] = 0;

	return true;
}

// obelisk_scanwork is called repeatedly by cgminer to perform scanning work.
static int64_t obelisk_scanwork(__maybe_unused struct thr_info* thr) {
	struct cgpu_info* cgpu = thr->cgpu;
	ob_chain* ob = cgpu->device_data;
	pthread_cond_signal(&ob->work_cond);

	// First make sure that we have buffered work, if not we can't give new jobs
	// to chips.
	bool workReady = bufferedWorkReady(ob);
	if (!workReady) {
		return 0;
	}
	// Check if chips need starting.
	if (!ob->chipsStarted) {
		for (uint8_t chipNum = 0; chipNum < ob->staticBoardModel.chipsPerBoard; chipNum++) {
			applog(LOG_ERR, "Starting chip: %u.%u", ob->staticBoardNumber, chipNum);
			ob->setChipNonceRange(ob, chipNum, 1);
			for (uint8_t engineNum = 0; engineNum < ob->staticBoardModel.enginesPerChip; engineNum++) {
				ob->startNextEngineJob(ob, chipNum, engineNum);
			}
			ob->chipWork[chipNum] = ob->bufferedWork;
			cgtimer_time(&ob->chipStartTimes[chipNum]);
			cgtimer_time(&ob->chipResetTimes[chipNum]);
			ob->chipGoodNonces[chipNum] = 0;
		}
		ob->chipsStarted = true;
	}

	// We wait at least a little bit between each iteration to make sure we
	// aren't blasting the SPI and blocking the other boards from getting access
	// to global resources.
	ratelimitIterations(ob);

	cgtimer_t lastStart, lastEnd, lastDuration;
	cgtimer_t doneStart, doneEnd, doneDuration;
	cgtimer_t readStart, readEnd, readDuration;
	cgtimer_t loadStart, loadEnd, loadDuration;
	int lastTotal = 0;
	int doneTotal = 0;
	int readTotal = 0;
	int loadTotal = 0;

	// Look for done engines, and read their nonces
	cgtimer_t currentTime;
	cgtimer_time(&currentTime);
	int64_t hashesConfirmed = 0;
	for (uint8_t chipNum = 0; chipNum < ob->staticBoardModel.chipsPerBoard; chipNum++) {
		// Check whether the chip is ready to be checked for completeion.
		bool chipReady = isChipReady(ob, chipNum);
		if (!chipReady) {
			continue;
		}

		// Check if the chip needs to be reset.
		bool chipReset = resetChipIfRequired(ob, chipNum);
		if (chipReset) {
			continue;
		}

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
			applog(LOG_ERR, "a chip is reporting itself as partially done: %u.%u.%i", ob->staticBoardNumber, chipNum, msLastCheck);
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
