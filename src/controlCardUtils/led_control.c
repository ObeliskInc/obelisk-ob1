#include "gpio.h"
#include "led_control.h"

// Turn on the green light.
bool greenLEDOn(void) {
	if(gpio_set_output_pin_low(CONTROLLER_GREEN_LED) != GPIO_RET_SUCCESS) {
		return false;
	}
	return true;
}

bool greenLEDOff(void) {
	if(gpio_set_output_pin_high(CONTROLLER_GREEN_LED) != GPIO_RET_SUCCESS) {
		return false;
	}
	return true;
}

bool redLEDOn(void) {
	if(gpio_set_output_pin_low(CONTROLLER_RED_LED) != GPIO_RET_SUCCESS) {
		return false;
	}
	return true;
}

bool redLEDOff(void) {
	if(gpio_set_output_pin_high(CONTROLLER_RED_LED) != GPIO_RET_SUCCESS) {
		return false;
	}
	return true;
}
