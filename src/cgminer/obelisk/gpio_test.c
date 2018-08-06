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
//#include <linux/gpio.h>

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

/* Derive direction file string from PIN */
static char* STATIC_DIR_STR[21] = {
    "/sys/class/gpio/PB23/direction", // FAN_POWER_CONTROL
    "/sys/class/gpio/PA31/direction", // FAN_PWM_CONTROL
    "/sys/class/gpio/PC13/direction", // HASH_BOARD_ONE_PRESENT
    "/sys/class/gpio/PC14/direction", // HASH_BOARD_TWO_PRESENT
    "/sys/class/gpio/PC15/direction", // HASH_BOARD_THREE_PRESENT
    "/sys/class/gpio/PC16/direction", // HASH_BOARD_ONE_DONE
    "/sys/class/gpio/PC17/direction", // HASH_BOARD_TWO_DONE
    "/sys/class/gpio/PC23/direction", // HASH_BOARD_THREE_DONE
    "/sys/class/gpio/PC24/direction", // HASH_BOARD_ONE_NONCE
    "/sys/class/gpio/PC25/direction", // HASH_BOARD_TWO_NONCE
    "/sys/class/gpio/PC26/direction", // HASH_BOARD_THREE_NONCE
    "/sys/class/gpio/PB26/direction", // CONTROLLER_USER_SWITCH - not presently used, but available
    "/sys/class/gpio/PD8/direction", // CONTROLLER_POWER_SENSE - not used
    "/sys/class/gpio/PA10/direction", // CONTROLLER_GREEN_LED
    "/sys/class/gpio/PB1/direction", // CONTROLLER_RED_LED
    "/sys/class/gpio/PA9/direction", // CONTROLLER_SECOND_ETH - no used
    "/sys/class/gpio/PA12/direction", // SPI_ADDR0
    "/sys/class/gpio/PA13/direction", // SPI_ADDR1
    "/sys/class/gpio/PA17/direction", // SPI_SS1
    "/sys/class/gpio/PA7/direction", // SPI_SS2
    "/sys/class/gpio/PA8/direction", // SPI_SS3
};

char* GPIO_PIN_TO_DIRECTION_STRING(gpio_pin_t gpio_pin_id)
{
    switch (gpio_pin_id) {
    default:
        return NULL;
        break;
    case FAN_POWER_CONTROL:
        return STATIC_DIR_STR[0];
        break;
    case FAN_PWM_CONTROL:
        return STATIC_DIR_STR[1];
        break;
    case HASH_BOARD_ONE_PRESENT:
        return STATIC_DIR_STR[2];
        break;
    case HASH_BOARD_TWO_PRESENT:
        return STATIC_DIR_STR[3];
        break;
    case HASH_BOARD_THREE_PRESENT:
        return STATIC_DIR_STR[4];
        break;
    case HASH_BOARD_ONE_DONE:
        return STATIC_DIR_STR[5];
        break;
    case HASH_BOARD_TWO_DONE:
        return STATIC_DIR_STR[6];
        break;
    case HASH_BOARD_THREE_DONE:
        return STATIC_DIR_STR[7];
        break;
    case HASH_BOARD_ONE_NONCE:
        return STATIC_DIR_STR[8];
        break;
    case HASH_BOARD_TWO_NONCE:
        return STATIC_DIR_STR[9];
        break;
    case HASH_BOARD_THREE_NONCE:
        return STATIC_DIR_STR[10];
        break;
    case CONTROLLER_USER_SWITCH:
        return STATIC_DIR_STR[11];
        break;
    case CONTROLLER_POWER_SENSE:
        return STATIC_DIR_STR[12];
        break;
    case CONTROLLER_GREEN_LED:
        return STATIC_DIR_STR[13];
        break;
    case CONTROLLER_RED_LED:
        return STATIC_DIR_STR[14];
        break;
    case CONTROLLER_SECOND_ETH:
        return STATIC_DIR_STR[15];
        break;
    case SPI_ADDR0:
        return STATIC_DIR_STR[16];
        break;
    case SPI_ADDR1:
        return STATIC_DIR_STR[17];
        break;
    case SPI_SS1:
        return STATIC_DIR_STR[18];
        break;
    case SPI_SS2:
        return STATIC_DIR_STR[19];
        break;
    case SPI_SS3:
        return STATIC_DIR_STR[20];
        break;
    }
    return NULL;
}

/* Set GPIO pin as input for reading */
gpio_ret_t gpio_set_pin_as_input(gpio_pin_t gpio_pin_id)
{
    int directionfd;
    char t_str[16], *p_dir_str = NULL;

    if ((p_dir_str = GPIO_PIN_TO_DIRECTION_STRING(gpio_pin_id)) == NULL) {
        GPIO_LOG("Unable to derive direction file name for pin %d\n", gpio_pin_id);
        return GPIO_RET_ERROR;
    }

    if ((directionfd = open(p_dir_str, O_WRONLY)) < 0) {
        GPIO_LOG("Unable to open GPIO direction file for pin %d. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        return GPIO_RET_ERROR;
    }

    sprintf(t_str, "in");
    if (write(directionfd, t_str, (strlen(t_str) + 1)) < 0) {
        GPIO_LOG("Unable to set gpio pin %d as input. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        close(directionfd);
        return GPIO_RET_ERROR;
    }

    close(directionfd);
    return GPIO_RET_SUCCESS;
}

/* Set GPIO pin as output for modifying */
gpio_ret_t gpio_set_pin_as_output(gpio_pin_t gpio_pin_id)
{
    int directionfd;
    char t_str[16], *p_dir_str = NULL;

    if ((p_dir_str = GPIO_PIN_TO_DIRECTION_STRING(gpio_pin_id)) == NULL) {
        GPIO_LOG("Unable to derive direction file name for pin %d\n", gpio_pin_id);
        return GPIO_RET_ERROR;
    }

    if ((directionfd = open(p_dir_str, O_WRONLY)) < 0) {
        GPIO_LOG("Unable to open GPIO direction file for pin %d. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        return GPIO_RET_ERROR;
    }

    sprintf(t_str, "out");
    if (write(directionfd, t_str, (strlen(t_str) + 1)) < 0) {
        GPIO_LOG("Unable to set gpio pin %d as output. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        close(directionfd);
        return GPIO_RET_ERROR;
    }

    close(directionfd);
    return GPIO_RET_SUCCESS;
}

/* Derive value file string from PIN */
static char* STATIC_VAL_STR[21] = {
    "/sys/class/gpio/PB23/value", // FAN_POWER_CONTROL
    "/sys/class/gpio/PA31/value", // FAN_PWM_CONTROL
    "/sys/class/gpio/PC13/value", // HASH_BOARD_ONE_PRESENT
    "/sys/class/gpio/PC14/value", // HASH_BOARD_TWO_PRESENT
    "/sys/class/gpio/PC15/value", // HASH_BOARD_THREE_PRESENT
    "/sys/class/gpio/PC16/value", // HASH_BOARD_ONE_DONE
    "/sys/class/gpio/PC17/value", // HASH_BOARD_TWO_DONE
    "/sys/class/gpio/PC23/value", // HASH_BOARD_THREE_DONE
    "/sys/class/gpio/PC24/value", // HASH_BOARD_ONE_NONCE
    "/sys/class/gpio/PC25/value", // HASH_BOARD_TWO_NONCE
    "/sys/class/gpio/PC26/value", // HASH_BOARD_THREE_NONCE
    "/sys/class/gpio/PB26/value", // CONTROLLER_USER_SWITCH
    "/sys/class/gpio/PD8/value", // CONTROLLER_POWER_SENSE
    "/sys/class/gpio/PA10/value", // CONTROLLER_GREEN_LED
    "/sys/class/gpio/PB1/value", // CONTROLLER_RED_LED
    "/sys/class/gpio/PA9/value", // CONTROLLER_SECOND_ETH
    "/sys/class/gpio/PA12/value", // SPI_ADDR0
    "/sys/class/gpio/PA13/value", // SPI_ADDR1
    "/sys/class/gpio/PA17/value", // SPI_SS1
    "/sys/class/gpio/PA7/value", // SPI_SS2
    "/sys/class/gpio/PA8/value", // SPI_SS3
};

char* GPIO_PIN_TO_VALUE_STRING(gpio_pin_t gpio_pin_id)
{
    switch (gpio_pin_id) {
    default:
        return NULL;
        break;
    case FAN_POWER_CONTROL:
        return STATIC_VAL_STR[0];
        break;
    case FAN_PWM_CONTROL:
        return STATIC_VAL_STR[1];
        break;
    case HASH_BOARD_ONE_PRESENT:
        return STATIC_VAL_STR[2];
        break;
    case HASH_BOARD_TWO_PRESENT:
        return STATIC_VAL_STR[3];
        break;
    case HASH_BOARD_THREE_PRESENT:
        return STATIC_VAL_STR[4];
        break;
    case HASH_BOARD_ONE_DONE:
        return STATIC_VAL_STR[5];
        break;
    case HASH_BOARD_TWO_DONE:
        return STATIC_VAL_STR[6];
        break;
    case HASH_BOARD_THREE_DONE:
        return STATIC_VAL_STR[7];
        break;
    case HASH_BOARD_ONE_NONCE:
        return STATIC_VAL_STR[8];
        break;
    case HASH_BOARD_TWO_NONCE:
        return STATIC_VAL_STR[9];
        break;
    case HASH_BOARD_THREE_NONCE:
        return STATIC_VAL_STR[10];
        break;
    case CONTROLLER_USER_SWITCH:
        return STATIC_VAL_STR[11];
        break;
    case CONTROLLER_POWER_SENSE:
        return STATIC_VAL_STR[12];
        break;
    case CONTROLLER_GREEN_LED:
        return STATIC_VAL_STR[13];
        break;
    case CONTROLLER_RED_LED:
        return STATIC_VAL_STR[14];
        break;
    case CONTROLLER_SECOND_ETH:
        return STATIC_VAL_STR[15];
        break;
    case SPI_ADDR0:
        return STATIC_VAL_STR[16];
        break;
    case SPI_ADDR1:
        return STATIC_VAL_STR[17];
        break;
    case SPI_SS1:
        return STATIC_VAL_STR[18];
        break;
    case SPI_SS2:
        return STATIC_VAL_STR[19];
        break;
    case SPI_SS3:
        return STATIC_VAL_STR[20];
        break;
    }
    return NULL;
}

/* Read GPIO pin value (input or output) */
int gpio_read_pin(gpio_pin_t gpio_pin_id)
{
    int valuefd, value = -1;
    char t_str[16], *p_val_str = NULL;

    if ((p_val_str = GPIO_PIN_TO_VALUE_STRING(gpio_pin_id)) == NULL) {
        GPIO_LOG("Unable to derive value file name for pin %d\n", gpio_pin_id);
        return (int)GPIO_RET_ERROR;
    }

    if ((valuefd = open(p_val_str, O_RDONLY)) < 0) {
        GPIO_LOG("Unable to open GPIO value file for pin %d. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        return (int)GPIO_RET_ERROR;
    }

    memset(t_str, 0, sizeof(t_str));
    if (read(valuefd, t_str, sizeof(t_str)) < 0) {
        GPIO_LOG("Unable to read value of gpio pin %d. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        close(valuefd);
        return GPIO_RET_ERROR;
    }

    close(valuefd);

    /* If we successfully read a zero or positive value, then return it */
    value = atoi(t_str);
    if (value >= 0) {
        return value;
    }

    return (int)GPIO_RET_ERROR;
}

/* Set GPIO output pin value to LOW */
gpio_ret_t gpio_set_output_pin_low(gpio_pin_t gpio_pin_id)
{
    int valuefd;
    char t_str[16], *p_val_str = NULL;

    if ((p_val_str = GPIO_PIN_TO_VALUE_STRING(gpio_pin_id)) == NULL) {
        GPIO_LOG("Unable to derive value file name for pin %d\n", gpio_pin_id);
        return GPIO_RET_ERROR;
    }

    if ((valuefd = open(p_val_str, O_WRONLY)) < 0) {
        GPIO_LOG("Unable to open GPIO value file for pin %d. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        return GPIO_RET_ERROR;
    }

    sprintf(t_str, "0");
    if (write(valuefd, t_str, (strlen(t_str) + 1)) < 0) {
        GPIO_LOG("Unable to set gpio pin %d to LOW. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        close(valuefd);
        return GPIO_RET_ERROR;
    }

    close(valuefd);
    return GPIO_RET_SUCCESS;
}

/* Set GPIO output pin value to HIGH */
gpio_ret_t gpio_set_output_pin_high(gpio_pin_t gpio_pin_id)
{
    int valuefd;
    char t_str[16], *p_val_str = NULL;

    if ((p_val_str = GPIO_PIN_TO_VALUE_STRING(gpio_pin_id)) == NULL) {
        GPIO_LOG("Unable to derive value file name for pin %d\n", gpio_pin_id);
        return GPIO_RET_ERROR;
    }

    if ((valuefd = open(p_val_str, O_WRONLY)) < 0) {
        GPIO_LOG("Unable to open GPIO value file for pin %d. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        return GPIO_RET_ERROR;
    }

    sprintf(t_str, "1");
    if (write(valuefd, t_str, (strlen(t_str) + 1)) < 0) {
        GPIO_LOG("Unable to set gpio pin %d to HIGH. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        close(valuefd);
        return GPIO_RET_ERROR;
    }

    close(valuefd);
    return GPIO_RET_SUCCESS;
}

/* Initialize GPIO */
gpio_ret_t gpio_init(void)
{
    gpio_ret_t retval = GPIO_RET_SUCCESS;

    GPIO_LOG("Initializing GPIO subsystem\nExporting pins\n");

    /* Set up pins for export */
    if (gpio_init_pin(FAN_POWER_CONTROL) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up FAN_POWER_CONTROL pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(FAN_PWM_CONTROL) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up FAN_PWM_CONTROL pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(HASH_BOARD_ONE_PRESENT) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up HASH_BOARD_ONE_PRESENT pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(HASH_BOARD_TWO_PRESENT) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up HASH_BOARD_TWO_PRESENT pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(HASH_BOARD_THREE_PRESENT) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up HASH_BOARD_THREE_PRESENT pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(HASH_BOARD_ONE_DONE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up HASH_BOARD_ONE_DONE pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(HASH_BOARD_TWO_DONE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up HASH_BOARD_TWO_DONE pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(HASH_BOARD_THREE_DONE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up HASH_BOARD_THREE_DONE pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(HASH_BOARD_ONE_NONCE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up HASH_BOARD_ONE_NONCE pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(HASH_BOARD_TWO_NONCE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up HASH_BOARD_TWO_NONCE pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(HASH_BOARD_THREE_NONCE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up HASH_BOARD_THREE_NONCE pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(CONTROLLER_USER_SWITCH) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up CONTROLLER_USER_SWITCH pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(CONTROLLER_POWER_SENSE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up CONTROLLER_POWER_SENSE pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(CONTROLLER_RED_LED) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up CONTROLLER_RED_LED pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(CONTROLLER_GREEN_LED) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up CONTROLLER_GREEN_LED pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(CONTROLLER_SECOND_ETH) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up CONTROLLER_SECOND_ETH pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(SPI_ADDR0) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up SPI_ADDR0 pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(SPI_ADDR1) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up SPI_ADDR1 pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(SPI_SS1) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up SPI_SS1 pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(SPI_SS2) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up SPI_SS2 pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_init_pin(SPI_SS3) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error Setting up SPI_SS3 pin\n");
        retval = GPIO_RET_ERROR;
    }

    /* Set proper pins as inputs */
    GPIO_LOG("Setting up input pins\n");

    if (gpio_set_pin_as_input(HASH_BOARD_ONE_PRESENT) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing HASH_BOARD_ONE_PRESENT pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_set_pin_as_input(HASH_BOARD_TWO_PRESENT) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing HASH_BOARD_TWO_PRESENT pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_set_pin_as_input(HASH_BOARD_THREE_PRESENT) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing HASH_BOARD_THREE_PRESENT pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_set_pin_as_input(HASH_BOARD_ONE_DONE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing HASH_BOARD_ONE_DONE pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_set_pin_as_input(HASH_BOARD_TWO_DONE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing HASH_BOARD_TWO_DONE pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_set_pin_as_input(HASH_BOARD_THREE_DONE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing HASH_BOARD_THREE_DONE pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_set_pin_as_input(HASH_BOARD_ONE_NONCE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing HASH_BOARD_ONE_NONCE pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_set_pin_as_input(HASH_BOARD_TWO_NONCE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing HASH_BOARD_TWO_NONCE pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_set_pin_as_input(HASH_BOARD_THREE_NONCE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing HASH_BOARD_THREE_NONCE pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_set_pin_as_input(CONTROLLER_USER_SWITCH) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing CONTROLLER_USER_SWITCH pin\n");
        retval = GPIO_RET_ERROR;
    }

    if (gpio_set_pin_as_input(CONTROLLER_POWER_SENSE) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing CONTROLLER_POWER_SENSE pin\n");
        retval = GPIO_RET_ERROR;
    }

    GPIO_LOG("Setting up output pins\n");

    if (gpio_set_pin_as_output(FAN_POWER_CONTROL) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing FAN_POWER_CONTROL pin\n");
        retval = GPIO_RET_ERROR;
    } else {
        if (gpio_set_output_pin_high(FAN_POWER_CONTROL) != GPIO_RET_SUCCESS) {
            GPIO_LOG("Error setting FAN_POWER_CONTROL pin high\n");
            retval = GPIO_RET_ERROR;
        }
    }

    if (gpio_set_pin_as_output(FAN_PWM_CONTROL) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing FAN_PWM_CONTROL pin\n");
        retval = GPIO_RET_ERROR;
    } else {
        if (gpio_set_output_pin_low(FAN_PWM_CONTROL) != GPIO_RET_SUCCESS) {
            GPIO_LOG("Error setting FAN_PWM_CONTROL pin low\n");
            retval = GPIO_RET_ERROR;
        }
    }

    if (gpio_set_pin_as_output(CONTROLLER_RED_LED) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing CONTROLLER_RED_LED pin\n");
        retval = GPIO_RET_ERROR;
    } else {
        if (gpio_set_output_pin_low(CONTROLLER_RED_LED) != GPIO_RET_SUCCESS) {
            GPIO_LOG("Error setting CONTROLLER_RED_LED pin low\n");
            retval = GPIO_RET_ERROR;
        }
    }

    if (gpio_set_pin_as_output(CONTROLLER_GREEN_LED) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing CONTROLLER_GREEN_LED pin\n");
        retval = GPIO_RET_ERROR;
    } else {
        if (gpio_set_output_pin_high(CONTROLLER_GREEN_LED) != GPIO_RET_SUCCESS) {
            GPIO_LOG("Error setting CONTROLLER_GREEN_LED pin low\n");
            retval = GPIO_RET_ERROR;
        }
    }

    if (gpio_set_pin_as_output(CONTROLLER_SECOND_ETH) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing CONTROLLER_SECOND_ETH pin\n");
        retval = GPIO_RET_ERROR;
    } else {
        if (gpio_set_output_pin_low(CONTROLLER_SECOND_ETH) != GPIO_RET_SUCCESS) {
            GPIO_LOG("Error setting CONTROLLER_SECOND_ETH pin low\n");
            retval = GPIO_RET_ERROR;
        }
    }

    if (gpio_set_pin_as_output(SPI_ADDR0) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing SPI_ADDR0 pin\n");
        retval = GPIO_RET_ERROR;
    } else {
        if (gpio_set_output_pin_low(SPI_ADDR0) != GPIO_RET_SUCCESS) {
            GPIO_LOG("Error setting SPI_ADDR0 pin low\n");
            retval = GPIO_RET_ERROR;
        }
    }

    if (gpio_set_pin_as_output(SPI_ADDR1) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing SPI_ADDR1 pin\n");
        retval = GPIO_RET_ERROR;
    } else {
        if (gpio_set_output_pin_low(SPI_ADDR1) != GPIO_RET_SUCCESS) {
            GPIO_LOG("Error setting SPI_ADDR1 pin low\n");
            retval = GPIO_RET_ERROR;
        }
    }

    if (gpio_set_pin_as_output(SPI_SS1) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing SPI_SS1 pin\n");
        retval = GPIO_RET_ERROR;
    } else {
        if (gpio_set_output_pin_low(SPI_SS1) != GPIO_RET_SUCCESS) {
            GPIO_LOG("Error setting SPI_SS1 pin low\n");
            retval = GPIO_RET_ERROR;
        }
    }

    if (gpio_set_pin_as_output(SPI_SS2) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing SPI_SS2 pin\n");
        retval = GPIO_RET_ERROR;
    } else {
        if (gpio_set_output_pin_low(SPI_SS2) != GPIO_RET_SUCCESS) {
            GPIO_LOG("Error setting SPI_SS2 pin low\n");
            retval = GPIO_RET_ERROR;
        }
    }

    if (gpio_set_pin_as_output(SPI_SS3) != GPIO_RET_SUCCESS) {
        GPIO_LOG("Error initializing SPI_SS3 pin\n");
        retval = GPIO_RET_ERROR;
    } else {
        if (gpio_set_output_pin_low(SPI_SS3) != GPIO_RET_SUCCESS) {
            GPIO_LOG("Error setting SPI_SS3 pin low\n");
            retval = GPIO_RET_ERROR;
        }
    }

    int value;
    if ((value = gpio_read_pin(HASH_BOARD_ONE_PRESENT)) != (int)GPIO_RET_ERROR) {
        GPIO_LOG("HASH_BOARD_ONE_PRESENT pin value is: %d\n", value);
    } else {
        GPIO_LOG("Unable to read HASH_BOARD_ONE_PRESENT pin\n");
    }

    if ((value = gpio_read_pin(HASH_BOARD_TWO_PRESENT)) != (int)GPIO_RET_ERROR) {
        GPIO_LOG("HASH_BOARD_TWO_PRESENT pin value is: %d\n", value);
    } else {
        GPIO_LOG("Unable to read HASH_BOARD_TWO_PRESENT pin\n");
    }

    if ((value = gpio_read_pin(HASH_BOARD_THREE_PRESENT)) != (int)GPIO_RET_ERROR) {
        GPIO_LOG("HASH_BOARD_THREE_PRESENT pin value is: %d\n", value);
    } else {
        GPIO_LOG("Unable to read HASH_BOARD_THREE_PRESENT pin\n");
    }

    //	printf("-E- gpio_init retval: %d\n", retval);

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
