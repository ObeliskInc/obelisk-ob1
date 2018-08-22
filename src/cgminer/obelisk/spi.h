/*
 * SPI Support Definitions
 *
 * (C) 2018 TechEn Inc
 */

#include <time.h>

extern int fileSPI;

extern void transfer(int fd, uint8_t const *tx, uint8_t const *rx, size_t len, clock_t* transfer_time);
extern int spi_setup(void);
extern int spi_main(void);