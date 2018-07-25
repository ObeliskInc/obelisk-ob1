/*
 * LED Erase Flash Error (Fast flashing)
 *
 * (C) 2018 TechEn Inc
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

/* Fundamental functions */

/* Turn GREEN LED ON, turn RED LED OFF */
/* NOTE: GREEN/RED reversed betwen program interface and hardware */
void red_led_off_green_led_off(void);
void red_led_off_green_led_off(void)
{
    int greenfd, redfd;
    static char off_str[] = "0";
    
    if((greenfd = open("/sys/class/leds/red/brightness", O_WRONLY)) >= 0)
    {
        if(write(greenfd, off_str, 1) < 0)
        {
            printf("Unable to set GREEN LED OFF. %d:%s\n", errno, strerror(errno));
        }
    }
    else
    {
        printf("Unable to open GREEN LED brightness file. %d:%s\n", errno, strerror(errno));
    }
    
    if((redfd = open("/sys/class/leds/green/brightness", O_WRONLY)) >= 0)
    {
        if(write(redfd, off_str, 1) < 0)
        {
            printf("Unable to set RED LED OFF. %d:%s\n", errno, strerror(errno));
        }
    }
    else
    {
        printf("Unable to open RED LED brightness file. %d:%s\n", errno, strerror(errno));
    }

    close(greenfd);
    close(redfd);
    return;
}

/* Turn RED LED ON, turn GREEN LED OFF */
/* NOTE: GREEN/RED reversed betwen program interface and hardware */
void red_led_on_green_led_off(void);
void red_led_on_green_led_off(void)
{
    int greenfd, redfd;
    static char off_str[] = "0", on_str[] = "255";
    
    if((greenfd = open("/sys/class/leds/red/brightness", O_WRONLY)) >= 0)
    {
        if(write(greenfd, off_str, 1) < 0)
        {
            printf("Unable to set GREEN LED OFF. %d:%s\n", errno, strerror(errno));
        }
    }
    else
    {
        printf("Unable to open GREEN LED brightness file. %d:%s\n", errno, strerror(errno));
    }
    
    if((redfd = open("/sys/class/leds/green/brightness", O_WRONLY)) >= 0)
    {
        if(write(redfd, on_str, 1) < 0)
        {
            printf("Unable to set RED LED ON. %d:%s\n", errno, strerror(errno));
        }
    }
    else
    {
        printf("Unable to open RED LED brightness file. %d:%s\n", errno, strerror(errno));
    }

    close(greenfd);
    close(redfd);
    return;
}

/* Main test function */
int main(void)
{
    red_led_on_green_led_off();
    while(1)
    {
        red_led_off_green_led_off();
        usleep(150000);
        red_led_on_green_led_off();
        usleep(150000);
    }
    return 0;
}
