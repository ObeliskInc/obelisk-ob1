// Obelisk Fan Control and Speed utils
#include "Ob1Defines.h"
#include <stdint.h>
#include "miner.h"

ApiError ob1InitializeFanCtrl();

ApiError ob1SetFanSpeeds(uint8_t percent);

uint32_t ob1GetFanRPM(uint8_t fanNum);
