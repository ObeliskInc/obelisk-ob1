#include <stdio.h>
#include <unistd.h>
#include "led_control.h"

// led_alternate is a program which alternates between a green light and a red
// light, printing messages to let you know that the API is implemented
// correctly.
int main(void) {
	redLEDOff();
	greenLEDOff();

	while (1) {
		greenLEDOn();
		sleep(1);

		greenLEDOff();
		sleep(1);

		redLEDOn();
		sleep(1);

		redLEDOff();
		sleep(1);
	}
	return 0;
}

