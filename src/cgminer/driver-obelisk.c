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

#include "driver-obelisk.h"
#include "compat.h"
#include "config.h"
#include "klist.h"
#include "miner.h"
#include "sha2.h"
#include "obelisk/siahash/siaverify.h"
#include <ctype.h>
#include <string.h>

#if (ALGO == BLAKE2B)
#include "obelisk/blake2.h"
#endif

static int num_chains = 0;
static ob_chain chains[MAX_CHAIN_NUM];

#if (MODEL == SC1)
Nonce SiaNonceLowerBound = 0;
Nonce SigNonceUpperBound = 0xFFFFFFFF;
#endif

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

void reset_ready_engines(chip_info* chip)
{
    memset(chip->ready_engines, 1, NUM_ENGINES_PER_CHIP * sizeof(uint8_t));
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
bool is_valid_nonce(ob_chain* ob, Nonce nonce)
{
#if (MODEL == SC1)
    // Make the header with the nonce inserted at the right spot
    uint8_t header[SIA_HEADER_SIZE];
    memcpy(header, ob->curr_work->midstate, SIA_HEADER_SIZE);
    memcpy(header + 32, &nonce, sizeof(Nonce));

    if (siaHeaderMeetsMinimumTarget(header)) {
        ob->good_nonces_found++;
        applog(LOG_ERR, "*********** GOOD NONCE FOUND: 0x%016llX", nonce);
    } else {
        applog(LOG_ERR, "*********** BAD NONCE FOUND: 0x%016llX", nonce);
        ob->bad_nonces_found++;
    }
#elif (MODEL == DCR1)
    // TODO: Implement for Decred
#endif
}

// Separate thread to handle nonces so that we are not polling for them
static void* ob_nonce_thread(void* arg)
{
    struct cgpu_info* cgpu = arg;
    ob_chain* ob = cgpu->device_data;
    char tname[16];

    sprintf(tname, "ob_nonce_%d", ob->chain_id);
    RenameThread(tname);

    mutex_lock(&ob->lock);

    while (!pthread_cond_wait(&ob->nonce_cond, &ob->lock)) {
        mutex_unlock(&ob->lock);

        int n = num_pending_nonces(ob);
        while (n > 0) {
            nonce_info info;
            if (pop_pending_nonce(ob, &info) == SUCCESS) {
                struct work* work = ob->chips[info.chip_num].work;

                // Check that the nonce is valid
                if (is_valid_nonce(ob, info.nonce)) { // TODO: Probably not using this function, but this may be wrong now
                    // If valid, submit to the pool
                    submit_nonce(cgpu->thr[0], work, info.nonce);
                } else {
                    // TODO: handle error in some way
                }
            }
            n--;
        }

        mutex_lock(&ob->lock);
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

void mark_engine_ready(ob_chain* ob, int chip_num, int engine_num)
{
    mutex_lock(&ob->lock);
    ob->chips[chip_num].ready_engines[engine_num] = 1;
    mutex_unlock(&ob->lock);
}

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

void add_hashes(ob_chain* ob, Nonce num_hashes)
{
    mutex_lock(&ob->lock);
    ob->num_hashes += num_hashes;
    mutex_unlock(&ob->lock);
}

// Get the number of hashes since we last checked, then reset to zero
Nonce get_and_reset_hashes(ob_chain* ob)
{
    Nonce n;
    mutex_lock(&ob->lock);
    n = ob->num_hashes;
    ob->num_hashes += 0;
    mutex_unlock(&ob->lock);
    return n;
}

void ob_nonce_handler(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, Nonce nonce, bool nonceLimitReached)
{
    applog(LOG_ERR, "ob_nonce_handler called!");
    // Take the info and put it into a queue that is service by a nonce thread and signal
    // the condition variable.
    ob_chain* ob = &chains[boardNum];
    push_pending_nonce(ob, chipNum, engineNum, nonce, nonceLimitReached);

    // TODO: API should clear interrupts after calling handler possibly multiple times
}

void ob_job_complete_handler(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum)
{
    applog(LOG_ERR, "ob_job_complete_handler called!");
    ob_chain* ob = &chains[boardNum];
    mark_engine_ready(ob, chipNum, engineNum);

    add_hashes(ob, NONCE_RANGE_SIZE);
    // TODO: API should clear interrupts after calling handler possibly multiple times
}

static void obelisk_detect(bool hotplug)
{
    applog(LOG_ERR, "***** obelisk_detect()\n");

    // TODO: Detect how many boards are installed and set a global variable
    // num_chains, so we know how many boards to talk to, how many threads
    // to start, etc.

    ob1RegisterNonceHandler(ob_nonce_handler);
    ob1RegisterJobCompleteHandler(ob_job_complete_handler);

    ob1Initialize();

    int num_chains = ob1GetNumPresentHashboards();
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
        cgtime(&cgpu->dev_start_tv);

        // TODO: Add this if necessary later
        //ob->lastshare = cgpu->dev_start_tv.tv_sec;

        // Initialize the ob chip fields
        for (int i = 0; i < NUM_CHIPS_PER_BOARD; i++) {
            struct chip_info* chip = &ob->chips[i];
            reset_ready_engines(chip);
            chip->start_of_next_nonce_range = 0;
            chip->work = NULL;
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
        pthread_create(&pth, NULL, ob_nonce_thread, cgpu);
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

/*
 * TODO: allow this to run through more than once - the second+
 * time not sending any new work unless a flush occurs since:
 * at the moment we have BAB_STD_WORK_mS latency added to earliest replies
 */
static int64_t obelisk_scanwork(__maybe_unused struct thr_info* thr)
{
    struct cgpu_info* cgpu = thr->cgpu;
    ob_chain* ob = cgpu->device_data;
    applog(LOG_ERR, "***** obelisk_scanwork(%d) - start", ob->chain_id);

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
        applog(LOG_ERR, "doneBitmask=0x%016llX (chip_num=%u)", doneBitmask, chip_num);
        if (error == SUCCESS) {
            for (uint8_t engine_num = 0; engine_num < ob1GetNumEnginesPerChip(); engine_num++) {
                // If engine is done
                if (doneBitmask & (1ULL << engine_num)) {
                    //
                    NonceSet nonce_set;
                    ob1ReadNonces(ob->chain_id, chip_num, engine_num, &nonce_set);
                    for (uint8_t i = 0; i < nonce_set.count; i++) {
                        // Check that the nonce is valid
                        if (is_valid_nonce(ob, nonce_set.nonces[i])) {
                            // If valid, submit to the pool
                            // submit_nonce(cgpu->thr[0], ob->curr_work, nonce_set.nonces[i]);
                        } else {
                            // TODO: handle error in some way
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

    // Get a single work item
    // TODO: We don;t need to get a new work item every time through - Sia has TONS of nonce space,
    // so we can keep reusing it.
    if (!ob->curr_work) {
        ob->curr_work = wq_dequeue(ob, true);
        applog(LOG_ERR, "Switching to new work: job_id=%s, nonce2=%lld", ob->curr_work->job_id, ob->curr_work->nonce2);
        hexdump(ob->curr_work->midstate, SIA_HEADER_SIZE);

        // Load the current job to all chips
        Job job;
        // TODO: Change ob1LoadJob() to take a byte pointer so we avoid this copy
        memcpy(&job, ob->curr_work->midstate, sizeof(Job));

        ApiError error = ob1LoadJob(ob->chain_id, ALL_CHIPS, ALL_ENGINES, &job);
        if (error != SUCCESS) {
            // TODO: do something
            goto scanwork_exit;
        }
    }

    if (ob->curr_work) {
        for (uint8_t chip_num = 0; chip_num < NUM_CHIPS_PER_BOARD; chip_num++) {
            // HACK: Set slow clocking for now
            // TODO: Move this outside of this loop to the control loop
            ApiError error = ob1SetClockDividerAndBias(ob->chain_id, chip_num, 8, NO_BIAS);

            for (uint8_t engine_num = 0; engine_num < ob1GetNumEnginesPerChip(); engine_num++) {
                // applog(LOG_ERR, "Checking if engine_num=%d is done", engine_num);
                if (!is_engine_busy(ob, chip_num, engine_num)) {

                    // TODO: We are not currently taking advantage of the queued job ability

                    // applog(LOG_ERR, "Set nonce range: lower=%llu  upper=%llu", ob->start_of_next_nonce, ob->start_of_next_nonce + NONCE_RANGE_SIZE - 1);

                    //  Set different nonce ranges
                    error = ob1SetNonceRange(ob->chain_id, chip_num, engine_num, ob->start_of_next_nonce, ob->start_of_next_nonce + NONCE_RANGE_SIZE - 1);
                    if (error != SUCCESS) {
                        // TODO: do something
                        goto scanwork_exit;
                    }
                    // Increment for next engine
                    ob->start_of_next_nonce += NONCE_RANGE_SIZE;

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
                }
            }
        }

        // Clear done flags - how?

#elif (MODEL == DCR1)
    // Iterate over all chips and assign jobs
    for (int i = 0; i < NUM_CHIPS_PER_BOARD; i++) {
        // Get a single work item
        struct work* work = wq_dequeue(ob, true);
        if (work) {
            applog(LOG_ERR, "Got work: work->job_id=%s nonce2=%lld", work->job_id, work->nonce2);
            hexdump(work->midstate, SIA_HEADER_SIZE);

            // Send a different job to each chip
            ApiError res = load_job(i, ALL_ENGINES, work->midstate);
            if (res != SUCCESS) {
                // TODO: do something
                goto scanwork_exit;
            }

            // Send a subrange of this job to each engine on this chip
            // TODO: Technically I could move this outside
            for (int e = 0; e < NUM_ENGINES_PER_CHIP; e++) {
                nonce_range_t nonce_range;
                nonce_range.lower_bound = ob->start_of_next_nonce;
                nonce_range.upper_bound = nonce_range.lower_bound + NONCE_RANGE_SIZE;
                ob->start_of_next_nonce = once_range.upper_bound + 1;

                res = set_nonce_range(i, e, nonce_range);
                if (res != SUCCESS) {
                    // TODO: do something
                    goto scanwork_exit;
                }
            }
        }
        // Start everything hashing!
        res = startJob({ ob_chain->chain_id, i, ALL_ENGINES });
        if (res != SUCCESS) {
            // TODO: do something
            goto scanwork_exit;
        }
    }

#endif
    }
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

    applog(LOG_ERR, "***** obelisk_scanwork(%d) - end", ob->chain_id);

scanwork_exit:
    cgsleep_ms(1000);

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
