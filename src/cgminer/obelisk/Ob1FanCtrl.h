// Obelisk Fan Control and Speed utils
#include "Ob1Defines.h"
#include <stdint.h>

ApiError ob1InitializeFanCtrl();

ApiError ob1SetFanSpeeds(uint8_t percent);

void void ob1GetFanRPMs(uint32_t* fanRPM);
