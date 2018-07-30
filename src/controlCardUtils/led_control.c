#include "gpio.h"
#include "led_control.h"

// Turn on the green light.
bool greenLEDOn(void) {
	return gpio_set_output_pin_high(CONTROLLER_RED_LED) == GPIO_RET_SUCCESS;
}

bool greenLEDOff(void) {
	return gpio_set_output_pin_low(CONTROLLER_RED_LED) == GPIO_RET_SUCCESS;
}

bool redLEDOn(void) {
	return gpio_set_output_pin_high(CONTROLLER_GREEN_LED) == GPIO_RET_SUCCESS;
}

bool redLEDOff(void) {
	return gpio_set_output_pin_low(CONTROLLER_GREEN_LED) == GPIO_RET_SUCCESS;
}

bool greenLEDisOn() {
	return gpio_read_pin(CONTROLLER_RED_LED) != 0;
}

bool redLEDisOn() {
	return gpio_read_pin(CONTROLLER_GREEN_LED) != 0;
}