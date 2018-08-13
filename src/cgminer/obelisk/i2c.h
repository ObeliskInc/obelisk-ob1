/*
 * I2C Support Definitions
 *
 * (C) 2018 TechEn Inc
 */

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

extern int i2cget(int device_addr, int num_bytes, u_int8_t *buffer);
extern int i2cget_reg(int device_addr, u_int8_t reg, int num_bytes, u_int8_t *buffer);
extern int i2cset(int device_addr, int num_bytes, u_int8_t *buffer);
extern int i2c_test(void);