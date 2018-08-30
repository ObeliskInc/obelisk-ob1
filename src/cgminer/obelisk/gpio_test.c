/*
 * GPIO Board Support Functions
 *
 * (C) 2018 TechEn Inc
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "gpio_bsp.h"
#include <linux/gpio.h>
#include <sys/ioctl.h>

// Define all the GPIOs
gpio_def_t gpios[] = {
    { FAN_POWER_CONTROL,        OUTPUT_PIN, PIN_HIGH, VALUE_PATH("PB23"), DIR_PATH("PB23") },
    { HASH_BOARD_ONE_PRESENT,   INPUT_PIN,  PIN_LOW,  VALUE_PATH("PC13"), DIR_PATH("PC13") },
    { HASH_BOARD_TWO_PRESENT,   INPUT_PIN,  PIN_LOW,  VALUE_PATH("PC14"), DIR_PATH("PC14") },
    { HASH_BOARD_THREE_PRESENT, INPUT_PIN,  PIN_LOW,  VALUE_PATH("PC15"), DIR_PATH("PC15") },
    { HASH_BOARD_ONE_DONE,      INPUT_PIN,  PIN_LOW,  VALUE_PATH("PC16"), DIR_PATH("PC16") },
    { HASH_BOARD_TWO_DONE,      INPUT_PIN,  PIN_LOW,  VALUE_PATH("PC17"), DIR_PATH("PC17") },
    { HASH_BOARD_THREE_DONE,    INPUT_PIN,  PIN_LOW,  VALUE_PATH("PC23"), DIR_PATH("PC23") },
    { HASH_BOARD_ONE_NONCE,     INPUT_PIN,  PIN_LOW,  VALUE_PATH("PC24"), DIR_PATH("PC24") },
    { HASH_BOARD_TWO_NONCE,     INPUT_PIN,  PIN_LOW,  VALUE_PATH("PC25"), DIR_PATH("PC25") },
    { HASH_BOARD_THREE_NONCE,   INPUT_PIN,  PIN_LOW,  VALUE_PATH("PC26"), DIR_PATH("PC26") },
    { CONTROLLER_USER_SWITCH,   INPUT_PIN,  PIN_LOW,  VALUE_PATH("PB26"), DIR_PATH("PB26") },
    { CONTROLLER_POWER_SENSE,   INPUT_PIN,  PIN_LOW,  VALUE_PATH("PD8"),  DIR_PATH("PD8") },
    { CONTROLLER_GREEN_LED,     OUTPUT_PIN, PIN_HIGH, VALUE_PATH("PA10"), DIR_PATH("PA10") },
    { CONTROLLER_RED_LED,       OUTPUT_PIN, PIN_LOW,  VALUE_PATH("PB1"),  DIR_PATH("PB1") },
    { CONTROLLER_SECOND_ETH,    OUTPUT_PIN, PIN_LOW,  VALUE_PATH("PA9"),  DIR_PATH("PA9") },
    { SPI_ADDR0,                OUTPUT_PIN, PIN_LOW,  VALUE_PATH("PA12"), DIR_PATH("PA12") },
    { SPI_ADDR1,                OUTPUT_PIN, PIN_LOW,  VALUE_PATH("PA13"), DIR_PATH("PA13") },
    { SPI_SS1,                  OUTPUT_PIN, PIN_LOW,  VALUE_PATH("PA17"), DIR_PATH("PA17") },
    { SPI_SS2,                  OUTPUT_PIN, PIN_LOW,  VALUE_PATH("PA7"),  DIR_PATH("PA7") },
    { SPI_SS3,                  OUTPUT_PIN, PIN_LOW,  VALUE_PATH("PA8"),  DIR_PATH("PA8") },
};


#define NUM_GPIOS (sizeof(gpios) / sizeof(gpio_def_t))


/* Fundamental functions */

/* Initialize pins by exporting them to the /sys/class/gpio export file */
gpio_ret_t gpio_init_pin(gpio_pin_t gpio_pin_id)
{
    int exportfd;
    char t_str[16];

    if ((exportfd = open("/sys/class/gpio/export", O_WRONLY)) < 0) {
        GPIO_LOG("Unable to open GPIO export file for pin %d. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        return GPIO_RET_ERROR;
    }

    sprintf(t_str, "%d", gpio_pin_id);
    if (write(exportfd, t_str, (strlen(t_str) + 1)) < 0) {
        if (errno != 16) {
            GPIO_LOG("Unable to export gpio pin %d. %d:%s\n", gpio_pin_id, errno, strerror(errno));
            close(exportfd);
            return GPIO_RET_ERROR;
        }
    }

    close(exportfd);
    return GPIO_RET_SUCCESS;
}


int GPIO_PIN_TO_INDEX(gpio_pin_t gpio_pin_id)
{
    switch (gpio_pin_id) {
    default:
        exit(-1);
        break;
    case FAN_POWER_CONTROL:
        return 0;
    case HASH_BOARD_ONE_PRESENT:
        return 1;
    case HASH_BOARD_TWO_PRESENT:
        return 2;
    case HASH_BOARD_THREE_PRESENT:
        return 3;
    case HASH_BOARD_ONE_DONE:
        return 4;
    case HASH_BOARD_TWO_DONE:
        return 5;
    case HASH_BOARD_THREE_DONE:
        return 6;
    case HASH_BOARD_ONE_NONCE:
        return 7;
    case HASH_BOARD_TWO_NONCE:
        return 8;
    case HASH_BOARD_THREE_NONCE:
        return 9;
    case CONTROLLER_USER_SWITCH:
        return 10;
    case CONTROLLER_POWER_SENSE:
        return 11;
    case CONTROLLER_GREEN_LED:
        return 12;
    case CONTROLLER_RED_LED:
        return 13;
    case CONTROLLER_SECOND_ETH:
        return 14;
    case SPI_ADDR0:
        return 15;
    case SPI_ADDR1:
        return 16;
    case SPI_SS1:
        return 17;
    case SPI_SS2:
        return 18;
    case SPI_SS3:
        return 19;
    }
}

// Set GPIO pin as input for reading
gpio_ret_t gpio_set_pin_as_input(gpio_pin_t gpio_pin_id)
{
    int directionfd;
    int index = GPIO_PIN_TO_INDEX(gpio_pin_id);
    char* direction_path = gpios[index].direction_path;

    if ((directionfd = open(direction_path, O_WRONLY)) < 0) {
        GPIO_LOG("Unable to open GPIO direction file for pin %d. %d:%s path=%s\n", gpio_pin_id, errno, strerror(errno), direction_path);
        return GPIO_RET_ERROR;
    }

    if (write(directionfd, "in", 3) < 0) {
        GPIO_LOG("Unable to set gpio pin %d as input. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        close(directionfd);
        return GPIO_RET_ERROR;
    }

    close(directionfd);
    return GPIO_RET_SUCCESS;
}

// Set GPIO pin as output for modifying
gpio_ret_t gpio_set_pin_as_output(gpio_pin_t gpio_pin_id)
{
    int directionfd;
    int index = GPIO_PIN_TO_INDEX(gpio_pin_id);
    char* direction_path = gpios[index].direction_path;

    if ((directionfd = open(direction_path, O_WRONLY)) < 0) {
        GPIO_LOG("Unable to open GPIO direction file for pin %d. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        return GPIO_RET_ERROR;
    }

    if (write(directionfd, "out", 4) < 0) {
        GPIO_LOG("Unable to set gpio pin %d as output. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        close(directionfd);
        return GPIO_RET_ERROR;
    }

    close(directionfd);
    return GPIO_RET_SUCCESS;
}

#define VALUE_STR_SIZE 16

// Read GPIO pin value (input or output)
int gpio_read_pin(gpio_pin_t gpio_pin_id)
{
    int valuefd;
    int value = -1;
    char value_str[VALUE_STR_SIZE], *p_val_str = NULL;
    // TODO: Avoid this lookup by changing the enum to use the index instead
    //       Create a parallel enum for the actual pin numbers - callers will all pass in the index
    int index = GPIO_PIN_TO_INDEX(gpio_pin_id);
    char* value_path = gpios[index].value_path;

    if ((valuefd = open(value_path, O_RDONLY)) < 0) {
        GPIO_LOG("Unable to open GPIO value file for pin %d. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        return (int)GPIO_RET_ERROR;
    }

    memset(value_str, 0, VALUE_STR_SIZE);
    if (read(valuefd, value_str, VALUE_STR_SIZE) < 0) {
        GPIO_LOG("Unable to read value of gpio pin %d. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        close(valuefd);
        return GPIO_RET_ERROR;
    }

    close(valuefd);

    // If we successfully read a zero or positive value, then return it
    value = atoi(value_str);
    if (value >= 0) {
        return value;
    }

    return GPIO_RET_ERROR;
}

// Set GPIO output pin value to LOW
gpio_ret_t gpio_set_output_pin_low(gpio_pin_t gpio_pin_id)
{
    int index = GPIO_PIN_TO_INDEX(gpio_pin_id);

    if (write(gpios[index].fd, "0", 2) < 0) {
        GPIO_LOG("Unable to set gpio pin %d to LOW. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        return GPIO_RET_ERROR;
    }

    return GPIO_RET_SUCCESS;
}

// Set GPIO output pin value to HIGH
gpio_ret_t gpio_set_output_pin_high(gpio_pin_t gpio_pin_id)
{
    int index = GPIO_PIN_TO_INDEX(gpio_pin_id);

    if (write(gpios[index].fd, "1", 2) < 0) {
        GPIO_LOG("Unable to set gpio pin %d to HIGH. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        return GPIO_RET_ERROR;
    }

    return GPIO_RET_SUCCESS;
}

/* Initialize GPIO */
gpio_ret_t gpio_init(void)
{
    gpio_ret_t retval = GPIO_RET_SUCCESS;

    GPIO_LOG("Initializing GPIO subsystem\nExporting pins\n");

    for (int i=0; i<NUM_GPIOS; i++) {
        gpio_def_t* curr_gpio = &gpios[i];

        // Export pins
        if (gpio_init_pin(curr_gpio->pin_id) != GPIO_RET_SUCCESS) {
            GPIO_LOG("Error Setting up pin %d pin\n", curr_gpio->pin_id);
            retval = GPIO_RET_ERROR;
        }

        // Set pins as inputs or outputs
        if (curr_gpio->is_output) {
            if (gpio_set_pin_as_output(curr_gpio->pin_id) != GPIO_RET_SUCCESS) {
                GPIO_LOG("Error initializing pin %d\n", curr_gpio->pin_id);
                retval = GPIO_RET_ERROR;
                continue;
            }

            // Open and save a file handle - only outputs do this, as inputs don't work properly if you leave them open
            if ((gpios[i].fd = open(curr_gpio->value_path, O_WRONLY)) < 0) {
                GPIO_LOG("Unable to open GPIO value file for pin %d. %d:%s\n", curr_gpio->pin_id, errno, strerror(errno));
                return GPIO_RET_ERROR;
            }
            GPIO_LOG("Pin %u = index %u: fd = %d\n", curr_gpio->pin_id, i, curr_gpio->fd);

            // Set the default value
            if (curr_gpio->default_value == PIN_HIGH) {
                if (gpio_set_output_pin_high(curr_gpio->pin_id) != GPIO_RET_SUCCESS) {
                    GPIO_LOG("Error setting pin %d to HIGH\n", curr_gpio->pin_id);
                    retval = GPIO_RET_ERROR;
                }
            } else {
                if (gpio_set_output_pin_low(curr_gpio->pin_id) != GPIO_RET_SUCCESS) {
                    GPIO_LOG("Error setting pin %d to LOW\n", curr_gpio->pin_id);
                    retval = GPIO_RET_ERROR;
                }
            }

        } else {
            if (gpio_set_pin_as_input(curr_gpio->pin_id) != GPIO_RET_SUCCESS) {
                GPIO_LOG("Error initializing pin %d\n", curr_gpio->pin_id);
                retval = GPIO_RET_ERROR;
            }
        }
    }

    return retval;
}

/* Print GPIO flags */
struct gpio_flag {
    char* name;
    unsigned long mask;
};

struct gpio_flag flagnames[] = {
    {
        .name = "kernel",
        .mask = GPIOLINE_FLAG_KERNEL,
    },
    {
        .name = "output",
        .mask = GPIOLINE_FLAG_IS_OUT,
    },
    {
        .name = "active-low",
        .mask = GPIOLINE_FLAG_ACTIVE_LOW,
    },
    {
        .name = "open-drain",
        .mask = GPIOLINE_FLAG_OPEN_DRAIN,
    },
    {
        .name = "open-source",
        .mask = GPIOLINE_FLAG_OPEN_SOURCE,
    },
};

void print_flags(unsigned long flags)
{
    unsigned int i;
    int printed = 0;
    for (i = 0; i < ARRAY_SIZE(flagnames); i++) {
        if (flags & flagnames[i].mask) {
            if (printed)
                printf(" ");
            printf("%s", flagnames[i].name);
            printed++;
        }
    }
}

/* Main test function */
int gpio_test(void)
{
#if 0
    /* Experiment with kernel space drivers */
    int fd = open("/dev/gpiochip0", 0);
    if(fd < 0)
    {
        printf("Error opening kernel space file, %d:%s\n", errno, strerror(errno));
        return -1;
    }

    struct gpiochip_info cinfo;
    struct gpioline_info linfo;

    ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &cinfo);
    printf("GPIO chip: %s, \"%s\", %u GPIO lines\n", cinfo.name, cinfo.label, cinfo.lines);

    for(unsigned int i = 0; i < cinfo.lines; i++)
    {
        memset(&linfo, 0, sizeof(linfo));
        linfo.line_offset = i;
        ioctl(fd, GPIO_GET_LINEINFO_IOCTL, &linfo);
        printf("line %2d: %s.", linfo.line_offset, linfo.name);
        if(linfo.consumer[0])
        {
            printf("Used by: Unused\n");
        }
        else
        {
            printf("Used by: %s\n", linfo.consumer);
        }
        
        printf("Flags: [");
        if (linfo.flags) {
            print_flags(linfo.flags);
        }
        printf("]\n");
    }

    close(fd);

#else
    /* Userspace drivers */
    if (gpio_init() != GPIO_RET_SUCCESS) {
        printf("Error initializing GPIO pins.\n");
    }
#endif
    return 0;
}

//Switch GPIO from High to Low or Low to High
//Returns 1 if pin is now HIGH
//Returns 0 if pin is now LOW
//Returns < 0 if ERROR
int gpio_toggle_pin_level(gpio_pin_t gpio_pin_id)
{
    int iPinLevel = gpio_read_pin(gpio_pin_id);

    if (0 < iPinLevel) {
        //printf("-I- Setting pin %d LOW", gpio_pin_id);
        if (0 == gpio_set_output_pin_low(gpio_pin_id)) {
            //printf("-I- GPIO %d set to LOW\n", gpio_pin_id);
        } else {
            printf("-E- Could not set GPIO %d to LOW\n", gpio_pin_id);
        }
        return 0;
    } else if (0 == iPinLevel) {
        //printf("-I- Setting pin %d HIGH", gpio_pin_id);
        if (0 == gpio_set_output_pin_high(gpio_pin_id)) {
            //printf("-I- GPIO %d set to HIGH\n", gpio_pin_id);
        } else {
            printf("-E- Could not set GPIO %d to HIGH\n", gpio_pin_id);
        }
        return 1;
    } else {
        printf("-E- Unable to determine status of GPIO: %d\n", gpio_pin_id);
        return iPinLevel;
    }
}

//Set pin high if true
//Set pin low if false
gpio_ret_t gpio_set_pin_level(gpio_pin_t gpio_pin_id, bool level)
{
    gpio_ret_t retVal;

    if (true == level) {
        retVal = gpio_set_output_pin_high(gpio_pin_id);
    } else {
        retVal = gpio_set_output_pin_low(gpio_pin_id);
    }

    return retVal;
}

//Ret false if low
//Ret true if high
bool gpio_get_pin_level(gpio_pin_t gpio_pin_id)
{
    gpio_ret_t retVal;
    retVal = gpio_read_pin(gpio_pin_id);

    if (0 == retVal) {
        return false;
    } else if (0 < retVal) {
        return true;
    } else {
        //TODO: How to handle this error
        return false;
    }
}

gpio_ret_t led_red_on(void)
{
    return gpio_set_output_pin_high(CONTROLLER_RED_LED);
}

gpio_ret_t led_red_off(void)
{
    return gpio_set_output_pin_low(CONTROLLER_RED_LED);
}

//led_red_toggle
//ret: false if turned OFF led
//ret: true if turned ON led
bool led_red_toggle(void)
{
    if (gpio_get_pin_level(CONTROLLER_RED_LED)) //red LED is on
    {
        led_red_off();
        return false;
    } else //red LED is off
    {
        led_red_on();
        return true;
    }
}

gpio_ret_t led_green_on(void)
{
    return gpio_set_output_pin_high(CONTROLLER_GREEN_LED);
}

gpio_ret_t led_green_off(void)
{
    return gpio_set_output_pin_low(CONTROLLER_GREEN_LED);
}

//led_green_toggle
//ret: false if turned OFF led
//ret: true if turned ON led
bool led_green_toggle(void)
{
    if (gpio_get_pin_level(CONTROLLER_GREEN_LED)) //green LED is on
    {
        led_green_off();
        return false;
    } else //green LED is off
    {
        led_green_on();
        return true;
    }
}

void led_alternate(void)
{
    led_red_toggle();
    if (gpio_get_pin_level(CONTROLLER_RED_LED)) //red LED is on
    {
        led_green_off();
    } else //red LED is off
    {
        led_green_on();
    }
}
