#include <stdio.h>
#include "gpio.h"

// Initialize the gpios.
int main(void) {
	if (gpio_init() != GPIO_RET_SUCCESS) {
		printf("Error initializing GPIO pins.\n");
		return -1;
	} else {
		printf("GPIO pins set correctly.\n");
		return 0;
	}
}
