#include <stdio.h>
#include <unistd.h>
#include "led_control.h"

// led_flash_green strobes the green LED for 1 second intervals.
//
// TODO: Error handling
int main(void) {
	printf("Turn off both lights.\n");
	redLEDOff();
	greenLEDOff();

	while (1) {
		printf("Turn on green light.\n");
		greenLEDOn();
		sleep(1);

		printf("Turn off green light.\n");
		greenLEDOff();
		sleep(1);
	}
	return 0;
}
