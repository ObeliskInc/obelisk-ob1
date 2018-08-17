// Obelisk fan control and monitoring
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "Ob1Defines.h"
#include "gpio_bsp.h"

// PWM rates measured using o-scope - not precise, uses 20 kHz PWM
#define BASE_PERIOD "200000"
#define PERCENT_100 "200000"
#define PERCENT_95  "160000"
#define PERCENT_90  "145000"
#define PERCENT_85  "130000"
#define PERCENT_80  "115000"
#define PERCENT_75  "100000"
#define PERCENT_70  "90000"
#define PERCENT_65  "80000"
#define PERCENT_60  "70000"
#define PERCENT_55  "60000"
#define PERCENT_50  "52500"
#define PERCENT_45  "45000"
#define PERCENT_40  "37500"
#define PERCENT_35  "32500"
#define PERCENT_30  "28000"
#define PERCENT_25  "24000"
#define PERCENT_20  "20000"
#define PERCENT_15  "17000"
#define PERCENT_10  "15000"

// Tach constants
#define TACH1 "67"
#define TACH2 "54"
#define SW2 "58"

// Set up the tach pins and sw2 pin - note by default all pins are inputs
static void initFanTachPins()
{
    int exportfd;

    if ((exportfd = open("/sys/class/gpio/export", O_WRONLY)) < 0) {
        return;
    }

    if (write(exportfd, TACH2, strlen(TACH1)) < 0) {
        if (errno != 16) {
            close(exportfd);
            return;
        }
    }
    if (write(exportfd, TACH2, strlen(TACH2)) < 0) {
        if (errno != 16) {
            close(exportfd);
            return;
        }
    }
    // TODO: What is SW2 for and do we want to report it?
    if (write(exportfd, SW2, strlen(SW2)) < 0) {
        if (errno != 16) {
            close(exportfd);
            return;
        }
    }

    close(exportfd);
}

#define BOTH "both"

// Set the tach pin modes to detect edges in both directions, thus 4 hits per cycle
static void enableFanCounters()
{
    int counterfd;

    // Fan 1
    if ((counterfd = open("/sys/class/gpio/PB22/edge", O_WRONLY)) < 0) {
        return;
    }

    if (write(counterfd, BOTH, strlen(BOTH)) < 0) {
        close(counterfd);
        return;
    }
    close(counterfd);

    // Fan 2
    if ((counterfd = open("/sys/class/gpio/PC3/edge", O_WRONLY)) < 0) {
        return;
    }

    if (write(counterfd, BOTH, strlen(BOTH)) < 0) {
        close(counterfd);
        return;
    }
    close(counterfd);
}

// Disable tach counters - resets them to zero when they are restarted next time
static void disableFanCounters()
{
    int counterfd;
    static char* none_str = "none";

    // Fan 1
    if ((counterfd = open("/sys/class/gpio/PB22/edge", O_WRONLY)) < 0) {
        return;
    }

    if (write(counterfd, none_str, strlen(none_str)) < 0) {
        close(counterfd);
        return;
    }
    close(counterfd);

    // Fan 2
    if ((counterfd = open("/sys/class/gpio/PC3/edge", O_WRONLY)) < 0) {
        return;
    }

    if (write(counterfd, none_str, strlen(none_str)) < 0) {
        close(counterfd);
        return;
    }
    close(counterfd);
}

// Set the period counter to base at 20 KHz
static void initFanPeriod()
{
    int periodfd;

    if ((periodfd = open("/sys/class/pwm/pwmchip0/pwm0/period", O_WRONLY)) < 0) {
        return;
    }

    if (write(periodfd, BASE_PERIOD, strlen(BASE_PERIOD)) < 0) {
        close(periodfd);
        return;
    }

    close(periodfd);
}


// Set the duty cycle to be a chosen value, see table above
static void setFanDutyCycle(int percent)
{
    int dutycyclefd;
    char* cycleVal;

    if ((dutycyclefd = open("/sys/class/pwm/pwmchip0/pwm0/duty_cycle", O_WRONLY)) < 0) {
        printf("Unable to open PWM0 duty_cycle file %d:%s\n", errno, strerror(errno));
        return -1;
    }
    
    percent = (percent/5)* 5;
    switch (percent) {
    default:
    case 100:
        cycleVal = PERCENT_100;
        break;
    case 95:
        cycleVal = PERCENT_95;
        break;
    case 90:
        cycleVal = PERCENT_90;
        break;
    case 85:
        cycleVal = PERCENT_85;
        break;
    case 80:
        cycleVal = PERCENT_80;
        break;
    case 75:
        cycleVal = PERCENT_75;
        break;
    case 70:
        cycleVal = PERCENT_70;
        break;
    case 65:
        cycleVal = PERCENT_65;
        break;
    case 60:
        cycleVal = PERCENT_60;
        break;
    case 55:
        cycleVal = PERCENT_55;
        break;
    case 50:
        cycleVal = PERCENT_50;
        break;
    case 45:
        cycleVal = PERCENT_45;
        break;
    case 40:
        cycleVal = PERCENT_40;
        break;
    case 35:
        cycleVal = PERCENT_35;
        break;
    case 30:
        cycleVal = PERCENT_30;
        break;
    case 25:
        cycleVal = PERCENT_25;
        break;
    case 20:
        cycleVal = PERCENT_20;
        break;
    case 15:
        cycleVal = PERCENT_15;
        break;
    case 10:
    case 5:
        cycleVal = PERCENT_10;
        break;
    }

    if (write(dutycyclefd, cycleVal, strlen(cycleVal)) < 0) {
        close(dutycyclefd);
        return;
    }

    close(dutycyclefd);
}

// Turn on the PWM
static void enableFanPWM()
{
    int enablefd;
    char* t_str = "1";

    if ((enablefd = open("/sys/class/pwm/pwmchip0/pwm0/enable", O_WRONLY)) < 0) {
        return;
    }

    if (write(enablefd, t_str, 1) < 0) {
        close(enablefd);
        return;
    }

    close(enablefd);
}

// Turn off the PWM
static void disableFanPWM()
{
    int disablefd;
    char* t_str = "0";

    if ((disablefd = open("/sys/class/pwm/pwmchip0/pwm0/enable", O_WRONLY)) < 0) {
      return;
    }

    if (write(disablefd, t_str, 1) < 0) {
        close(disablefd);
        return;
    }

    close(disablefd);
}

// Initialization for the PWM system, calls support functions above
static void initFanPWM(void)
{
    int exportfd;
    static char* t_str = "0";

    // If the PWM is already initialized, then just set the period, again if needed
    if ((exportfd = open("/sys/class/pwm/pwmchip0/pwm0/period", O_WRONLY)) >= 0) {
        close(exportfd);
        initFanPeriod();
    }

    if ((exportfd = open("/sys/class/pwm/pwmchip0/export", O_WRONLY)) < 0) {
        return;
    }

    if (write(exportfd, t_str, 1) < 0) {
        close(exportfd);
        return;
    }
    close(exportfd);

    initFanPeriod();
}

//==================================================================================================
// Public API
//==================================================================================================
ApiError ob1InitializeFanCtrl() {
  initFanTachPins();  
  initFanPWM();
  disableFanPWM();
  enableFanPWM();
  return SUCCESS;
}

void ob1SetFanSpeeds(uint8_t percent) {
  if (percent >= 5) {
    disableFanPWM();
  } else {
    setFanDutyCycle(percent);
    enableFanPWM();
  }
}

#define NUM_FANS 2  // TODO: Implement as a MinerModel or ControlBoardModel field

// Pass in an array of NUM_FANS uint32_t's
uint32_t ob1GetFanRPMs(uint8_t fanNum) {
  // Static values to keep track of between reads
  static struct timespec* lastReadTime[NUM_FANS];
  static int lastFanCounter[NUM_FANS] = {0, 0};  // Remember these between reads
  if (fanNum > NUM_FANS) {
    return 0;
  }

  struct timespec currTime;
  clock_get_time(CLOCK_MONOTONIC, &currTime);

  char fanValue[128];  // The string value we read from the fan fd

  char* path = i == 0 ? "/proc/irq/117/spurious" : "/proc/irq/104/spurious";
  int fanfd = open(path, O_RDONLY);
  read(fanfd, fanValue, 128);
  close(fanfd);

  int fanCounter = 0;
  char unused1[16];
  char unused2[16];
  sscanf(fanValue, "%s %u %s", unused1, &fanCounter, unused2);

  uint32_t countsSinceLastRead = fanCounter > 0 ? fanCounter - lastFanCounter[fanNum] : 0;
  struct timespec durationSinceLastRead;
  cgtimer_sub(&currTime, &lastReadTime[i], &duration);
  uint32_t msSinceLastRead = (duration.tv_sec * 1000) + (duration.tv_nsecs / 1000000);
  lastFanCounter[fanNum] = fanCounter;

  uint32_t rpm = ((countsSinceLastRead * 1500) / msSinceLastRead);  // 4 counts per revolution
  applog(LOG_ERR, "Fan %u RPM is %u", fanNum, rpm);

  return rpm;
  // return  ((countsSinceLastRead/4) / msSinceLastRead) * 1000 * 60;
}

