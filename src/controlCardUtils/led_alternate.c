#include <stdio.h>
#include <unistd.h>

#include "led_control.h"

// led_alternate is a program which alternates between a green light and a red
// light, printing messages to let you know that the API is implemented
// correctly.
//
// TODO: Error handling
int main(void) {
	printf("Turn off both lights.\n");
	redLEDOff();
	greenLEDOff();

    while(1) {
        printf("Turn on green light.");
		greenLEDOn();
        sleep(1);

		printf("Turn off green light.");
		greenLEDOff();
        sleep(1);

		printf("Turn on red light.");
		redLEDOn();
        sleep(1);

		printf("Turn off red light.");
		redLEDOff();
        sleep(1);
    }
    return 0;
}
