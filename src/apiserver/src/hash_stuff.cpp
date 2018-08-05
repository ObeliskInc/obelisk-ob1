
// These really should come from reading the board id and metadata, not hard-coded constants
#define MAX_BOARDS 3
#define NUM_ASICS_PER_BOARD 15
#define NUM_ENGINES_PER_SC1 64
#define NUM_ENGINES_PER_DCR1 128

#define MAX_HASHRATE_SAMPLES 10
#define NANOS_PER_SEC 1000000000UL

struct HashJobInfo {
  struct timespec start;
  uint64_t startingNonce;
  uint64_t endingNonce;
};

// A HashSample is created from what we measure when an ASIC finishes a job.
struct HashSample {
  uint64_t numHashes;
  struct timespec elapsedTime;
};

// Circular buffer that contains a set of hash samples.
// head and tail start out equal, which means buffer is empty.  Any other values mean there are
// values in the buffer.  Buffer is full when
class HashSampleSet {
  HashSample samples[MAX_HASHRATE_SAMPLES];
  uint head;
  uint tail;

  uint increment(uint i) { return (i + 1) % MAX_HASHRATE_SAMPLES; }

public:
  HashSampleSet() : head(0), tail(0) {}

  bool isEmpty() { return head == tail; }
  bool isFull() { return (head + 1) % MAX_HASHRATE_SAMPLES == tail; }

  insert(HashSample &sample) {
    if (!isFull()) {
      samples[head].numHashes = sample.numHashes;
      samples[head.elapsedTime = sample.elapsedTime;
      head = increment(head);
    }
  }

  reset() {
    head = 0;
    tail = 0;
  }

  // Calculate the hashrate over a given period of time.
  // Just keep accumulating samples until we hit the requested periodInSecs, then calculate
  // the number of hashes.  This will never be exact, but this is as close as we can come.
  // Calling getHashrate(s, 60) would return the hashrate in hashes per second over the last
  // 60 seconds (assuming we have that many samples).
  int64_t getHashrate(HashSampleSet &sampleSet, uint periodInSecs) {
    uint64_t totalElapsed = 0;
    uint64_t totalHashes = 0;

    int currIndex = sampleSet.tail;
    while (currIndex != sampleSet.head) {
      totalHashes += sampleSet.samples[currElapsed].numHashes;

      uint64_t currElapsed = asNanos(sampleSet.samples[currIndex].elapsedTime);
      totalElapsed += currElapsed;

      // Return number of hashes
      if (totalElapsed / NANOS_PER_SEC > periodInSecs) {
        return totalHashes / (totalElapsed / NANOS_PER_SEC);
      }

      currIndex = (currIndex + 1) % MAX_HASHRATE_SAMPLES;
    }

    // We reached the end of the samples without having reached the target periodInSecs, but
    // we will return the overall hashrate anyway.
    return totalHashes / (totalElapsed / NANOS_PER_SEC);
  }
};

// TODO: Make the array sizing dynamic - perhaps this is a class with parameters for the
//       values instead of just constants.  Add functions for getting per-board hashrates
//       and whole unit hashrate.
HashSampleSet allHashSamples[MAX_BOARDS][NUM_ASICS_PER_BOARD][NUM_ENGINES_PER_SC1];

// The main API server can poll the cgminer API periodically and can keep a hashrate history
// for each board and the unit as a whole.  That data would most likely be lost on each reboot.
