/*
 * GPIO Board Support Definitions
 *
 * (C) 2018 TechEn Inc
 */

typedef enum _gpio_ret_ {
    GPIO_RET_SUCCESS = 0,
    GPIO_RET_ERROR = -1,
} gpio_ret_t;

/* GPIO Pin Definitions - names and pin numbers */
typedef enum _gpio_pin_ {
    /* Fans */
    FAN_POWER_CONTROL = 55, // Output LOW = OFF, HIGH = ON
    FAN_PWM_CONTROL = 31, // Output LOW = ON ALWAYS, HIGH = OFF, PULSE = VARIED SPEEDS

    /* Hash board detects */
    HASH_BOARD_ONE_PRESENT = 77, // Input LOW = PRESENT, HIGH = NOT PRESENT
    HASH_BOARD_TWO_PRESENT = 78, // Input LOW = PRESENT, HIGH = NOT PRESENT
    HASH_BOARD_THREE_PRESENT = 79, // Input LOW = PRESENT, HIGH = NOT PRESENT

    /* Hash board DONE signals */
    HASH_BOARD_ONE_DONE = 80, // Input LOW = NOT DONE, HIGH = DONE
    HASH_BOARD_TWO_DONE = 81, // Input LOW = NOT DONE, HIGH = DONE
    HASH_BOARD_THREE_DONE = 87, // Input LOW = NOT DONE, HIGH = DONE

    /* Hash board NONCE signals */
    HASH_BOARD_ONE_NONCE = 88, // Input LOW = NO NONCE, HIGH = NONCE
    HASH_BOARD_TWO_NONCE = 89, // Input LOW = NO NONCE, HIGH = NONCE
    HASH_BOARD_THREE_NONCE = 90, // Input LOW = NO NONCE, HIGH = NONCE

    /* Controller board user switch */
    CONTROLLER_USER_SWITCH = 58, // Input LOW = NOT PRESSED, HIGH = PRESSED

    /* Controller board power indication */
    CONTROLLER_POWER_SENSE = 104, // Input HIGH = power ON(?)

    /* Controller board LEDs */
    //    CONTROLLER_RED_LED = 10,	//NOTE: This controlled GREEN LED
    //    CONTROLLER_GREEN_LED = 33,	//NOTE:
    CONTROLLER_RED_LED = 33,
    CONTROLLER_GREEN_LED = 10,

    CONTROLLER_SECOND_ETH = 9, // Output LOW = OFF, HIGH = ON - may not be used

    SPI_ADDR0 = 12, // Output
    SPI_ADDR1 = 13, // Output
    SPI_SS1 = 17, // Output
    SPI_SS2 = 7, // Output
    SPI_SS3 = 8, // Output
} gpio_pin_t;

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Functions or MACROS to control/read pins */
#define GPIO_LOG(format, ...) (printf(format, ##__VA_ARGS__))

extern gpio_ret_t gpio_init(void);
extern gpio_ret_t gpio_init_pin(gpio_pin_t gpio_pin_id);
extern char* GPIO_PIN_TO_DIRECTION_STRING(gpio_pin_t gpio_pin_id);
extern char* GPIO_PIN_TO_VALUE_STRING(gpio_pin_t gpio_pin_id);
extern gpio_ret_t gpio_set_pin_as_input(gpio_pin_t gpio_pin_id);
extern gpio_ret_t gpio_set_pin_as_output(gpio_pin_t gpio_pin_id);
extern gpio_ret_t gpio_set_output_pin_low(gpio_pin_t gpio_pin_id);
extern gpio_ret_t gpio_set_output_pin_high(gpio_pin_t gpio_pin_id);
extern int gpio_read_pin(gpio_pin_t gpio_pin_id);
extern void print_flags(unsigned long flags);
extern int gpio_test(void);
extern int gpio_toggle_pin_level(gpio_pin_t gpio_pin_id);
extern gpio_ret_t gpio_set_pin_level(gpio_pin_t gpio_pin_id, bool level);
extern bool gpio_get_pin_level(gpio_pin_t gpio_pin_id);

//LED Specific Functions
extern gpio_ret_t led_red_on(void);
extern gpio_ret_t led_red_off(void);
extern bool led_red_toggle(void);
extern gpio_ret_t led_green_on(void);
extern gpio_ret_t led_green_off(void);
extern bool led_green_toggle(void);
extern void led_alternate(void);
