#include <stdio.h>
#include <unistd.h>
#include "led_control.h"

// led_flash_green strobes the green LED for 1 second intervals.
int main(void) {
	redLEDOff();
	greenLEDOff();

	while (1) {
		redLEDOn();
		sleep(1);

		redLEDOff();
		sleep(1);
	}
	return 0;
}
