/*
 * SPI testing utility (using spidev driver)
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <linux/spi/spidev.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

int fileSPI = 0;

uint8_t default_tx[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x95,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xF0, 0x0D,
};

uint8_t default_rx[ARRAY_SIZE(default_tx)] = {0, };

static const char *device = "/dev/spidev32766.0";
static uint32_t mode = 0;
static uint16_t delay = 0;
static uint8_t bits = 8;
static uint32_t speed = 1000000;

void transfer(int fd, uint8_t const *tx, uint8_t const *rx, size_t len, clock_t *transfer_time)
{
    int ret;

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = len,
        .delay_usecs = delay,
        .speed_hz = speed,
        .bits_per_word = bits,
    };
    
    clock_t before = clock();
    if((ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr)) < 1)
    {
        printf("Failed to send SPI message\n");
    }
    clock_t after = clock();
    clock_t cpus = CLOCKS_PER_SEC / 1000000;
    *transfer_time = (after - before) / cpus;
}

int spi_setup(void)
{
	int ret = 0;
	
	//mode |= SPI_NO_CS;
	
	if((fileSPI = open(device, O_RDWR)) < 0)
    {
        printf("-E- Can't open device\n");
        //return fd;
		return -1;
    }
	
    /* spi mode */
    if((ret = ioctl(fileSPI, SPI_IOC_WR_MODE, &mode)) == -1)
    {
        printf("-E- Can't set SPI mode\n");
        return ret;
    }
        
    /* bits per word */
    if((ret = ioctl(fileSPI, SPI_IOC_WR_BITS_PER_WORD, &bits)) == -1)
    {
        printf("-E- Can't set bits per word\n");
        return ret;
    }

    /* max speed hz */
    if((ret = ioctl(fileSPI, SPI_IOC_WR_MAX_SPEED_HZ, &speed)) == -1)
    {
        printf("-E- Can't set max speed hz\n");
        return ret;
    }
	
	printf("-I- SPI mode: 0x%x\n", mode);
    printf("-I- Bits per word: %d\n", bits);
    printf("-I- Max speed: %d Hz (%d KHz)\n", speed, speed/1000);
	
	return ret;
}

int spi_main(void)
{
    int fd, ret = 0;

    if((fd = open(device, O_RDWR)) < 0)
    {
        printf("Can't open device\n");
        return fd;
    }

    /* spi mode */
    if((ret = ioctl(fd, SPI_IOC_WR_MODE, &mode)) == -1)
    {
        printf("Can't set SPI mode\n");
        return ret;
    }
        
    /* bits per word */
    if((ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits)) == -1)
    {
        printf("Can't set bits per word\n");
        return ret;
    }

    /* max speed hz */
    if((ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed)) == -1)
    {
        printf("Can't set max speed hz\n");
        return ret;
    }

    printf("SPI mode: 0x%x\n", mode);
    printf("Bits per word: %d\n", bits);
    printf("Max speed: %d Hz (%d KHz)\n", speed, speed/1000);

    clock_t transfer_time;
    transfer(fd, default_tx, default_rx, sizeof(default_tx), &transfer_time);

    close(fd);

    return ret;
}