/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */
#ifndef ATMEL_START_PINS_H_INCLUDED
#define ATMEL_START_PINS_H_INCLUDED

//#include <hal_gpio.h>

// SAMG has 4 pin functions

#define GPIO_PIN_FUNCTION_A 0
#define GPIO_PIN_FUNCTION_B 1
#define GPIO_PIN_FUNCTION_C 2
#define GPIO_PIN_FUNCTION_D 3

#define UserButton1 GPIO(GPIO_PORTA, 2)
#define SPI5_SS1 GPIO(GPIO_PORTA, 5)
#define LED0 GPIO(GPIO_PORTA, 6)
#define SPI5_MISO GPIO(GPIO_PORTA, 12)
#define SPI5_MOSI GPIO(GPIO_PORTA, 13)
#define SPI5_SCK GPIO(GPIO_PORTA, 14)
#define UARTTXD_PA27 GPIO(GPIO_PORTA, 27)
#define UARTRXD_PA28 GPIO(GPIO_PORTA, 28)
#define SPISEL0 GPIO(GPIO_PORTA, 30)

//NOTE: Cant find GPIO_PORTB
/*
#define PB8 GPIO(GPIO_PORTB, 8)
#define PB9 GPIO(GPIO_PORTB, 9)
#define SPISEL1 GPIO(GPIO_PORTB, 15)
*/

#endif // ATMEL_START_PINS_H_INCLUDED
