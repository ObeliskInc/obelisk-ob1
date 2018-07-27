#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "gpio_bsp.h"

/* Derive value file string from PIN */
static char *STATIC_VAL_STR[21] = {
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
    "/sys/class/gpio/PA10/value", // CONTROLLER_RED_LED
    "/sys/class/gpio/PB1/value", // CONTROLLER_GREEN_LED
    "/sys/class/gpio/PA9/value", // CONTROLLER_SECOND_ETH
    "/sys/class/gpio/PA12/value", // SPI_ADDR0
    "/sys/class/gpio/PA13/value", // SPI_ADDR1
    "/sys/class/gpio/PA17/value", // SPI_SS1
    "/sys/class/gpio/PA7/value",  // SPI_SS2
    "/sys/class/gpio/PA8/value",  // SPI_SS3
};

char *GPIO_PIN_TO_VALUE_STRING(gpio_pin_t gpio_pin_id)
{
    switch(gpio_pin_id)
    {
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
    case CONTROLLER_RED_LED:
        return STATIC_VAL_STR[13];
        break;
    case CONTROLLER_GREEN_LED:
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

gpio_ret_t gpio_set_output_pin_low(gpio_pin_t gpio_pin_id)
{
    int valuefd;
    char t_str[16], *p_val_str = NULL;

    if((p_val_str = GPIO_PIN_TO_VALUE_STRING(gpio_pin_id)) == NULL)
    {
        GPIO_LOG("Unable to derive value file name for pin %d\n", gpio_pin_id);
        return GPIO_RET_ERROR;
    }
    
    if((valuefd = open(p_val_str, O_WRONLY)) < 0)
    {
        GPIO_LOG("Unable to open GPIO value file for pin %d. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        return GPIO_RET_ERROR;
    }

    sprintf(t_str, "0");
    if(write(valuefd, t_str, (strlen(t_str)+1)) < 0)
    {
        GPIO_LOG("Unable to set gpio pin %d to LOW. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        close(valuefd);
        return GPIO_RET_ERROR;
    }

    close(valuefd);
    return GPIO_RET_SUCCESS;
}

gpio_ret_t gpio_set_output_pin_high(gpio_pin_t gpio_pin_id)
{
    int valuefd;
    char t_str[16], *p_val_str = NULL;

    if((p_val_str = GPIO_PIN_TO_VALUE_STRING(gpio_pin_id)) == NULL)
    {
        GPIO_LOG("Unable to derive value file name for pin %d\n", gpio_pin_id);
        return GPIO_RET_ERROR;
    }
    
    if((valuefd = open(p_val_str, O_WRONLY)) < 0)
    {
        GPIO_LOG("Unable to open GPIO value file for pin %d. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        return GPIO_RET_ERROR;
    }

    sprintf(t_str, "1");
    if(write(valuefd, t_str, (strlen(t_str)+1)) < 0)
    {
        GPIO_LOG("Unable to set gpio pin %d to HIGH. %d:%s\n", gpio_pin_id, errno, strerror(errno));
        close(valuefd);
        return GPIO_RET_ERROR;
    }

    close(valuefd);
    return GPIO_RET_SUCCESS;
}

int main(int argc, char **argv)
{
    int speed;
    
    if (argc != 2)
    {
        printf("Usage: %s <speed>\n", argv[0]);
        return -1;
    }
    
    speed = atoi(argv[1]);
    printf("Setting speed to: %d\n", speed);
    double upper, lower;
    upper = (double)speed;
    upper = upper * 100;
    lower = 10000 - upper;
    printf("Upper: %g Lower: %g\n", upper, lower);
    
    while(1)
    {
        gpio_set_output_pin_low(FAN_PWM_CONTROL);
        usleep(upper);
        if(lower > 0)
        {
            gpio_set_output_pin_high(FAN_PWM_CONTROL);
            usleep(lower);
        }
    }
    return 0;
}
