#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

static int i2c_dev_open(int i2cbus)
{
    char filename[sizeof("/dev/i2c-%d") + sizeof(int)*3];
    int fd;

    sprintf(filename, "/dev/i2c-%d", i2cbus);
    if((fd = open(filename, O_RDWR)) < 0)
        printf("I2C open: Can't open '%s'", filename);
    return fd;
}

/*
 * i2cget()
 *
 * For SC1Miner:
 */
int i2cget(int device_addr, int num_bytes, u_int8_t *buffer);
int i2cget(int device_addr, int num_bytes, u_int8_t *buffer)
{
    int status = 0;
    int fd;

    if((fd = i2c_dev_open(2)) > 0)
    {
        if((num_bytes <= 0) || (buffer == NULL))
        {
            printf("Invalid arguments\n");
            return -1;
        }
        ioctl(fd, I2C_SLAVE, device_addr);
        if(read(fd, buffer, num_bytes) != num_bytes)
        {
            printf("Error reading from i2c %d:%s\n", errno, strerror(errno));
            status = -1;
        }
    }
    else
    {
        printf("I2C Read: Error opening i2c device %d:%s\n", errno, strerror(errno));
        status = -1;
    }
    close(fd);
    return status;
}

/*
 * i2cget_reg()
 *
 * For SC1Miner:
 */
int i2cget_reg(int device_addr, u_int8_t reg, int num_bytes, u_int8_t *buffer);
int i2cget_reg(int device_addr, u_int8_t reg, int num_bytes, u_int8_t *buffer)
{
    int status = 0;
    int fd;

    if((fd = i2c_dev_open(2)) > 0)
    {
        if((num_bytes <= 0) || (buffer == NULL))
        {
            printf("Invalid arguments\n");
            return -1;
        }
        ioctl(fd, I2C_SLAVE, device_addr);
        write(fd, &reg, 1);
        if(read(fd, buffer, num_bytes) != num_bytes)
        {
            printf("Error reading from i2c %d:%s\n", errno, strerror(errno));
            status = -1;
        }
    }
    else
    {
        printf("I2C Read: Error opening i2c device %d:%s\n", errno, strerror(errno));
        status = -1;
    }
    close(fd);
    return status;
}

/*
 * i2cset()
 *
 * For SC1Miner:
 */
int i2cset(int device_addr, int num_bytes, u_int8_t *buffer);
int i2cset(int device_addr, int num_bytes, u_int8_t *buffer)
{
    int status = 0;
    int fd;

    if((fd = i2c_dev_open(2)) > 0)
    {
        if((num_bytes <= 0) || (buffer == NULL))
        {
            printf("Invalid arguments\n");
            return -1;
        }
        ioctl(fd, I2C_SLAVE, device_addr);
        if(write(fd, buffer, num_bytes) != num_bytes)
        {
            printf("Error writing to i2c %d:%s\n", errno, strerror(errno));
            status = -1;
        }
    }
    else
    {
        printf("I2C Write: Error opening i2c device %d:%s\n", errno, strerror(errno));
        status = -1;
    }
    close(fd);
    return status;
}

int main(void)
{
    u_int8_t buffer[16];
    
    //int i2cget(int device_addr, int num_bytes, u_int8_t *buffer);
    //int i2cget_reg(int device_addr, u_int8_t reg, int num_bytes, u_int8_t *buffer);
    //int i2cset(int device_addr, int num_bytes, u_int8_t *buffer);

    printf("Single byte get example.  Read TCA9546A: ");
    if(i2cget(0x70, 1, buffer) == 0)
    {
        printf("Value read: %d\n", buffer[0]);
    }
    else
    {
        printf("ERROR READING VALUE(1)\n");
    }
    
    printf("Single byte SET example.  WRITE TCA9546A: Value = 0x01: ");
    buffer[0] = 1;
    if(i2cset(0x70, 1, buffer) == 0)
    {
        printf("Value written successfully\n");
    }
    else
    {
        printf("ERROR WRITING VALUE(1)\n");
    }

    printf("Single byte get example.  READ MCP4017: ");
    if(i2cget(0x2f, 1, buffer) == 0)
    {
        printf("Value read: %d\n", buffer[0]);
    }
    else
    {
        printf("ERROR READING VALUE(1a)\n");
    }
    
    printf("Single byte SET example.  WRITE MCP4017: Value = 0x7f: ");
    buffer[0] = 0x7f;
    if(i2cset(0x2f, 1, buffer) == 0)
    {
        printf("Value written successfully\n");
    }
    else
    {
        printf("ERROR WRITING VALUE(1a)\n");
    }

    printf("Single byte read-back example.  READ MCP4017: ");
    if(i2cget(0x2f, 1, buffer) == 0)
    {
        printf("Value read: %d\n", buffer[0]);
    }
    else
    {
        printf("ERROR READING VALUE(1b)\n");
    }
    
    printf("Multi byte get example.  Read MCP24AA02 UID: ");
    if(i2cget_reg(0x50, 0xfa, 6, buffer) == 0)
    {
        printf("Values read: %02X %02X %02X %02X %02X %02X\n",
               buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
    }
    else
    {
        printf("ERROR READING VALUES(2)\n");
    }
    
    printf("Multi byte SET example.  Write ADS1015: ");
    buffer[0] = 0x01; // REGISTER ADDRESS
    buffer[1] = 0xb5;
    buffer[2] = 0x83;
    if(i2cset(0x48, 3, buffer) == 0)
    {
        printf("Values written successfully\n");
    }
    else
    {
        printf("ERROR WRITING VALUES(2)\n");
    }

    return 0;
}
