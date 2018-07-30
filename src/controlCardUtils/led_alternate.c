#include <stdio.h>
#include <unistd.h>
#include "led_control.h"

// led_alternate is a program which alternates between a green light and a red
// light, printing messages to let you know that the API is implemented
// correctly.
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

		printf("Turn on red light.\n");
		redLEDOn();
		sleep(1);

		printf("Turn off red light.\n");
		redLEDOff();
		sleep(1);
	}
	return 0;
}

