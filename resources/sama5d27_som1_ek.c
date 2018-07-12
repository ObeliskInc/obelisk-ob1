/* ----------------------------------------------------------------------------
 *         Microchip Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2017, Microchip Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Microchip's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "common.h"
#include "ddramc.h"
#include "debug.h"
#include "gpio.h"
#include "hardware.h"
#include "l2cc.h"
#include "matrix.h"
#include "pmc.h"
#include "sama5d27_som1_ek.h"
#include "string.h"
#include "timer.h"
#include "usart.h"
#include "watchdog.h"
#include "arch/at91_ddrsdrc.h"
#include "arch/at91_pio.h"
#include "arch/at91_pmc.h"
#include "arch/at91_rstc.h"
#include "arch/at91_sfr.h"
#include "arch/tz_matrix.h"

static void at91_dbgu_hw_init(void)
{
	const struct pio_desc dbgu_pins[] = {
		{"RXD1", CONFIG_SYS_DBGU_RXD_PIN, 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"TXD1", CONFIG_SYS_DBGU_TXD_PIN, 0, PIO_DEFAULT, PIO_PERIPH_A},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};

	pio_configure(dbgu_pins);
	pmc_sam9x5_enable_periph_clk(CONFIG_SYS_DBGU_ID);
}

static void initialize_dbgu(void)
{
	unsigned int baudrate = 115200;

	at91_dbgu_hw_init();

	if (pmc_check_mck_h32mxdiv())
		usart_init(BAUDRATE(MASTER_CLOCK / 2, baudrate));
	else
		usart_init(BAUDRATE(MASTER_CLOCK, baudrate));
}

#if defined(CONFIG_MATRIX)
static int matrix_configure_slave(void)
{
	unsigned int ddr_port;
	unsigned int ssr_setting, sasplit_setting, srtop_setting;

	/*
	 * Matrix 0 (H64MX)
	 */

	/*
	 * 0: Bridge from H64MX to AXIMX
	 * (Internal ROM, Crypto Library, PKCC RAM): Always Secured
	 */

	/* 1: H64MX Peripheral Bridge */

	/* 2 ~ 9 DDR2 Port1 ~ 7: Non-Secure */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128M);
	sasplit_setting = MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_128M);
	ssr_setting = MATRIX_LANSECH_NS(0) |
		      MATRIX_RDNSECH_NS(0) |
		      MATRIX_WRNSECH_NS(0);
	/* DDR port 0 not used from NWd */
	for (ddr_port = 1; ddr_port < 8; ddr_port++) {
		matrix_configure_slave_security(AT91C_BASE_MATRIX64,
					(H64MX_SLAVE_DDR2_PORT_0 + ddr_port),
					srtop_setting,
					sasplit_setting,
					ssr_setting);
	}

	/*
	 * 10: Internal SRAM 128K
	 * TOP0 is set to 128K
	 * SPLIT0 is set to 64K
	 * LANSECH0 is set to 0, the low area of region 0 is the Securable one
	 * RDNSECH0 is set to 0, region 0 Securable area is secured for reads.
	 * WRNSECH0 is set to 0, region 0 Securable area is secured for writes
	 */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128K);
	sasplit_setting = MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_64K);
	ssr_setting = MATRIX_LANSECH_S(0) |
		      MATRIX_RDNSECH_S(0) |
		      MATRIX_WRNSECH_S(0);
	matrix_configure_slave_security(AT91C_BASE_MATRIX64,
					H64MX_SLAVE_INTERNAL_SRAM,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	/* 11:  Internal SRAM 128K (Cache L2) */
	/* 12:  QSPI0 */
	/* 13:  QSPI1 */
	/* 14:  AESB */

	/*
	 * Matrix 1 (H32MX)
	 */

	/* 0: Bridge from H32MX to H64MX: Not Secured */

	/* 1: H32MX Peripheral Bridge 0: Not Secured */

	/* 2: H32MX Peripheral Bridge 1: Not Secured */

	/*
	 * 3: External Bus Interface
	 * EBI CS0 Memory(256M) ----> Slave Region 0, 1
	 * EBI CS1 Memory(256M) ----> Slave Region 2, 3
	 * EBI CS2 Memory(256M) ----> Slave Region 4, 5
	 * EBI CS3 Memory(128M) ----> Slave Region 6
	 * NFC Command Registers(128M) -->Slave Region 7
	 *
	 * NANDFlash(EBI CS3) --> Slave Region 6: Non-Secure
	 */
	srtop_setting =	MATRIX_SRTOP(6, MATRIX_SRTOP_VALUE_128M) |
			MATRIX_SRTOP(7, MATRIX_SRTOP_VALUE_128M);
	sasplit_setting = MATRIX_SASPLIT(6, MATRIX_SASPLIT_VALUE_128M) |
			  MATRIX_SASPLIT(7, MATRIX_SASPLIT_VALUE_128M);
	ssr_setting = MATRIX_LANSECH_NS(6) |
		      MATRIX_RDNSECH_NS(6) |
		      MATRIX_WRNSECH_NS(6) |
		      MATRIX_LANSECH_NS(7) |
		      MATRIX_RDNSECH_NS(7) |
		      MATRIX_WRNSECH_NS(7);
	matrix_configure_slave_security(AT91C_BASE_MATRIX32,
					H32MX_EXTERNAL_EBI,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	/* 4: NFC SRAM (4K): Non-Secure */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_8K);
	sasplit_setting = MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_8K);
	ssr_setting = MATRIX_LANSECH_NS(0) |
		      MATRIX_RDNSECH_NS(0) |
		      MATRIX_WRNSECH_NS(0);
	matrix_configure_slave_security(AT91C_BASE_MATRIX32,
					H32MX_NFC_SRAM,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	/* 5:
	 * USB Device High Speed Dual Port RAM (DPR): 1M
	 * USB Host OHCI registers: 1M
	 * USB Host EHCI registers: 1M
	 */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_1M) |
			MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_1M) |
			MATRIX_SRTOP(2, MATRIX_SRTOP_VALUE_1M);
	sasplit_setting = MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_1M) |
			  MATRIX_SASPLIT(1, MATRIX_SASPLIT_VALUE_1M) |
			  MATRIX_SASPLIT(2, MATRIX_SASPLIT_VALUE_1M);
	ssr_setting = MATRIX_LANSECH_NS(0) |
		      MATRIX_LANSECH_NS(1) |
		      MATRIX_LANSECH_NS(2) |
		      MATRIX_RDNSECH_NS(0) |
		      MATRIX_RDNSECH_NS(1) |
		      MATRIX_RDNSECH_NS(2) |
		      MATRIX_WRNSECH_NS(0) |
		      MATRIX_WRNSECH_NS(1) |
		      MATRIX_WRNSECH_NS(2);
	matrix_configure_slave_security(AT91C_BASE_MATRIX32,
					H32MX_USB,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	return 0;
}

static unsigned int security_ps_peri_id[] = {
	0,
};

static int matrix_config_periheral(void)
{
	unsigned int *peri_id = security_ps_peri_id;
	unsigned int array_size = sizeof(security_ps_peri_id) /
				  sizeof(unsigned int);
	int ret;

	ret = matrix_configure_peri_security(peri_id, array_size);
	if (ret)
		return -1;

	return 0;
}

static int matrix_init(void)
{
	int ret;

	matrix_write_protect_disable(AT91C_BASE_MATRIX64);
	matrix_write_protect_disable(AT91C_BASE_MATRIX32);

	ret = matrix_configure_slave();
	if (ret)
		return -1;

	ret = matrix_config_periheral();
	if (ret)
		return -1;

	return 0;
}
#endif

#ifdef CONFIG_DDR2
static void ddramc_reg_config(struct ddramc_register *ddramc_config)
{
	ddramc_config->mdr = AT91C_DDRC2_DBW_16_BITS |
			     AT91C_DDRC2_MD_DDR2_SDRAM;

	ddramc_config->cr = AT91C_DDRC2_NC_DDR10_SDR9 |
			    AT91C_DDRC2_NR_13 |
			    AT91C_DDRC2_CAS_3 |
			    AT91C_DDRC2_DISABLE_RESET_DLL |
			    AT91C_DDRC2_WEAK_STRENGTH_RZQ7 |
			    AT91C_DDRC2_ENABLE_DLL |
			    AT91C_DDRC2_NB_BANKS_8 |
			    AT91C_DDRC2_NDQS_ENABLED |
			    AT91C_DDRC2_DECOD_INTERLEAVED |
			    AT91C_DDRC2_UNAL_SUPPORTED;

	ddramc_config->rtr = 0x511;

	ddramc_config->t0pr = AT91C_DDRC2_TRAS_(7) |
			      AT91C_DDRC2_TRCD_(3) |
			      AT91C_DDRC2_TWR_(3) |
			      AT91C_DDRC2_TRC_(9) |
			      AT91C_DDRC2_TRP_(3) |
			      AT91C_DDRC2_TRRD_(2) |
			      AT91C_DDRC2_TWTR_(2) |
			      AT91C_DDRC2_TMRD_(2);

	ddramc_config->t1pr = AT91C_DDRC2_TRFC_(22) |
			      AT91C_DDRC2_TXSNR_(23) |
			      AT91C_DDRC2_TXSRD_(200) |
			      AT91C_DDRC2_TXP_(2);

	ddramc_config->t2pr = AT91C_DDRC2_TXARD_(2) |
			      AT91C_DDRC2_TXARDS_(8) |
			      AT91C_DDRC2_TRPA_(4) |
			      AT91C_DDRC2_TRTP_(2) |
			      AT91C_DDRC2_TFAW_(8);
}

static void ddr2_init(void)
{
	struct ddramc_register ddramc_reg;
	unsigned int reg;

	ddramc_reg_config(&ddramc_reg);

	pmc_enable_periph_clock(AT91C_ID_MPDDRC);
	pmc_enable_system_clock(AT91C_PMC_DDR);

	reg = AT91C_MPDDRC_RD_DATA_PATH_ONE_CYCLES;
	writel(reg, (AT91C_BASE_MPDDRC + MPDDRC_RD_DATA_PATH));

	reg = readl(AT91C_BASE_MPDDRC + MPDDRC_IO_CALIBR);
	reg &= ~AT91C_MPDDRC_RDIV;
	reg &= ~AT91C_MPDDRC_TZQIO;
	reg |= AT91C_MPDDRC_RDIV_DDR2_RZQ_50;
	reg |= AT91C_MPDDRC_TZQIO_(101);
	writel(reg, (AT91C_BASE_MPDDRC + MPDDRC_IO_CALIBR));

	ddram_initialize(AT91C_BASE_MPDDRC, AT91C_BASE_DDRCS, &ddramc_reg);

	ddramc_dump_regs(AT91C_BASE_MPDDRC);
}
#endif

/**
 * The MSBs [bits 31:16] of the CAN Message RAM for CAN0 and CAN1
 * are configured in 0x210000, instead of the default configuration
 * 0x200000, to avoid conflict with SRAM map for PM.
 */
#define CAN_MESSAGE_RAM_MSB	0x21

void at91_init_can_message_ram(void)
{
	writel(AT91C_CAN0_MEM_ADDR_(CAN_MESSAGE_RAM_MSB) |
	       AT91C_CAN1_MEM_ADDR_(CAN_MESSAGE_RAM_MSB),
	       (AT91C_BASE_SFR + SFR_CAN));
}

static void at91_red_led_on(void)
{
	pio_set_gpio_output(AT91C_PIN_PA(27), 0);
}

static void at91_red_led_off(void)
{
	pio_set_gpio_output(AT91C_PIN_PA(27), 1);
}

#if 1 // RTM - added boot fuse code
typedef struct {
	volatile unsigned int BUSRAM_LOWER[1024]; /**< \brief (Securam Offset: 0x0000) Lower 4KB auto-erased */
	volatile unsigned int BUSRAM_HIGHER[256]; /**< \brief (Securam Offset: 0x1000) Higher 1KB not auto-erased */
	volatile unsigned int
	BUREG[8];           /**< \brief (Securam Offset: 0x1400) BUREG 256 bits auto-erased */
} Securam;

#define SECURAM    ((Securam  *)0xF8044000U) /**< \brief (SECURAM   ) Base Address */

typedef struct {
	volatile unsigned int BSC_CR; /**< \brief (Bsc Offset: 0x0) Boot Sequence Control Configuration Register */
} Bsc;

#define BSC        ((Bsc      *)0xF8048054U) /**< \brief (BSC       ) Base Address */
#define BSC_CR_WPKEY_Pos 16
#define BSC_CR_WPKEY_Msk (0xffffu << BSC_CR_WPKEY_Pos) /**< \brief (BSC_CR) Write Protect Key */
#define BSC_CR_WPKEY (0x6683u << 16) /**< \brief (BSC_CR) valid key to write BSC_CR register */
#define BSC_CR_BUREG_VALID (1 << 2) /**< \brief (BSC_CR) Validate the data in BUREG_INDEX field */

typedef struct {
	volatile unsigned int SFC_KR;     /**< \brief (Sfc Offset: 0x00) SFC Key Register */
	volatile unsigned int SFC_MR;     /**< \brief (Sfc Offset: 0x04) SFC Mode Register */
	volatile unsigned int  Reserved1[2];
	volatile unsigned int SFC_IER;    /**< \brief (Sfc Offset: 0x10) SFC Interrupt Enable Register */
	volatile unsigned int SFC_IDR;    /**< \brief (Sfc Offset: 0x14) SFC Interrupt Disable Register */
	volatile unsigned int SFC_IMR;    /**< \brief (Sfc Offset: 0x18) SFC Interrupt Mask Register */
	volatile unsigned int SFC_SR;     /**< \brief (Sfc Offset: 0x1C) SFC Status Register */
	volatile unsigned int SFC_DR[24];  /**< \brief (Sfc Offset: 0x20) SFC Data Register */
} Sfc;
#define ID_SFC          (50) /**< \brief Fuse Controller (SFC) */

#define SFC        ((Sfc      *)0xF804C000U) /**< \brief (SFC       ) Base Address */
#define SFC_SR_PGMC (0x1u << 0) /**< \brief (SFC_SR) Programming Sequence Completed (cleared on read) */
#define SFC_KR_KEY_Pos 0
#define SFC_KR_KEY_Msk (0xffu << SFC_KR_KEY_Pos) /**< \brief (SFC_KR) Key Code */
#define SFC_KR_KEY(value) ((SFC_KR_KEY_Msk & ((value) << SFC_KR_KEY_Pos)))

#define PMC 0xF0014000 /**< \brief (PMC       ) Base Address */

#define PMC_PCR_PID_Pos 0
#define PMC_PCR_PID_Msk (0x7fu << PMC_PCR_PID_Pos) /**< \brief (PMC_PCR) Peripheral ID */
#define PMC_PCR_PID(value) ((PMC_PCR_PID_Msk & ((value) << PMC_PCR_PID_Pos)))
#define PMC_PCR_CMD (0x1u << 12) /**< \brief (PMC_PCR) Command */
#define PMC_PCR_EN (0x1u << 28) /**< \brief (PMC_PCR) Enable */

#define FUSE_BOOTCONFIG_WORD_POS   16

/* Final fuse information */
#define BCW_EXT_MEM_BOOT_ENABLE (1 << 18)
#define BCW_UART_CONSOLE_Pos 12
#define BCW_UART_CONSOLE_Msk (0xfu << BCW_UART_CONSOLE_Pos)
#define BCW_UART_CONSOLE_UART1_IOSET_1 (0x0u << 12)
#define BCW_JTAG_IO_SET_Pos 16
#define BCW_JTAG_IO_SET_Msk (0x3u << BCW_JTAG_IO_SET_Pos)
#define BCW_JTAG_IOSET_1 (0x0u << 16)
#define BCW_JTAG_IOSET_2 (0x1u << 16)
#define BCW_JTAG_IOSET_3 (0x2u << 16)
#define BCW_JTAG_IOSET_4 (0x3u << 16)
#define BCW_SDMMC_0_DISABLED (1 << 10)
#define BCW_SDMMC_1_DISABLED (1 << 11)
#define BCW_NFC_Pos 8
#define BCW_NFC_Msk (0x3u << BCW_NFC_Pos)
#define BCW_NFC(value) (BCW_NFC_Msk & ((value) << BCW_NFC_Pos)
#define BCW_NFC_DISABLED (0x3u << 8)
#define BCW_SPI_0_Pos 4
#define BCW_SPI_0_Msk (0x3u << BCW_SPI_0_Pos)
#define BCW_SPI_0(value) (BCW_SPI_0_Msk & ((value) << BCW_SPI_0_Pos)
#define   BCW_SPI_0_IOSET_1 (0x0u << 4)
#define   BCW_SPI_0_IOSET_2 (0x1u << 4)
#define   BCW_SPI_0_DISABLED (0x3u << 4)
#define BCW_SPI_1_Pos 6
#define BCW_SPI_1_Msk (0x3u << BCW_SPI_1_Pos)
#define BCW_SPI_1(value) (BCW_SPI_1_Msk & ((value) << BCW_SPI_1_Pos)
#define   BCW_SPI_1_IOSET_1 (0x0u << 6)
#define   BCW_SPI_1_IOSET_2 (0x1u << 6)
#define   BCW_SPI_1_IOSET_3 (0x2u << 6)
#define   BCW_SPI_1_DISABLED (0x3u << 6)
#define BCW_QSPI_0_Pos 0
#define BCW_QSPI_0_Msk (0x3u << BCW_QSPI_0_Pos)
#define BCW_QSPI_0(value) (BCW_QSPI_0_Msk & ((value) << BCW_QSPI_0_Pos)
#define BCW_QSPI_0_DISABLED (0x3u << 0)
#define BCW_QSPI_1_Pos 2
#define BCW_QSPI_1_Msk (0x3u << BCW_QSPI_1_Pos)
#define BCW_QSPI_1(value) (BCW_QSPI_1_Msk & ((value) << BCW_QSPI_1_Pos)
#define BCW_QSPI_1_IOSET_2 (0x1u << 2)
#define BCW_DISABLE_BSCR (1 << 22)

int burn_fuses(int flag);
int burn_fuses(int flag)
{
    unsigned int fuse, newfuse, status;
    unsigned int *pmcPtr;

    /* Read in the boot configuration values */
#if 0
    bscr = BSC->BSC_CR;
    bureg0 = SECURAM->BUREG[0];
    bureg1 = SECURAM->BUREG[1];
    bureg2 = SECURAM->BUREG[2];
    bureg3 = SECURAM->BUREG[3];
#endif
    fuse = SFC->SFC_DR[FUSE_BOOTCONFIG_WORD_POS];

    /* Write the values, and cause the fuse to be enabled */
    if(flag == 1)
    {
        BSC->BSC_CR = 0;
        SECURAM->BUREG[0] = 0;
        SECURAM->BUREG[1] = 0;
        SECURAM->BUREG[2] = 0;
        SECURAM->BUREG[3] = 0;
    
        //pmc_enable_peripheral(ID_SFC);
        // select peripheral
	pmcPtr = (unsigned int *)PMC;
	pmcPtr += PMC_PCR;
        *pmcPtr = PMC_PCR_PID(ID_SFC);

        volatile unsigned int pcr = *pmcPtr;
        *pmcPtr = pcr | PMC_PCR_CMD | PMC_PCR_EN;

        //sfc_write(FUSE_BOOTCONFIG_WORD_POS, value);
        /* write key */
        SFC->SFC_KR = SFC_KR_KEY(0xFB);
    }

    /* write data to be fused */
    /*
      BCW.fromText("EXT_MEM_BOOT,UART1_IOSET1,JTAG_IOSET1," +
      "SDMMC1_DISABLED,SDMMC0_DISABLED,NFC_DISABLED," +
      "SPI1_DISABLED,SPI0_DISABLED," +
      "QSPI1_IOSET2,QSPI0_DISABLED"))

      Equates to:
      value & BCW_QSPI_0_Msk = BCW_QSPI0_DISABLED
      value & BCW_QSPI_1_Msk = BCW_QSPI_1_IOSET_2
      value & BCW_SPI_0_Msk = BCW_SPI_0_DISABLED
      value & BCW_SPI_1_Msk = BCW_SPI_1_DISABLED
      value & BCW_NFC_Msk = BCW_NFC_DISABLED
      value & BCW_SDMMC_0_DISABLED
      value & BCW_SDMMC_1_ENABLED - no define, leave as zero
      value & BCW_UART_CONSOLE_Msk = BCW_UART_CONSOLE_UART1_IOSET_1
      value & BCW_JTAG_IO_SET_Msk = BCW_JTAG_IOSET_3
      value & BCW_EXT_MEM_BOOT_ENABLE
    */
    newfuse = 0;
    newfuse |= BCW_QSPI_0_DISABLED;
    newfuse |= BCW_QSPI_1_IOSET_2;
    newfuse |= BCW_SPI_0_DISABLED;
    newfuse |= BCW_SPI_1_DISABLED;
    newfuse |= BCW_NFC_DISABLED;
    newfuse |= BCW_SDMMC_0_DISABLED;
    //newfuse |= BCW_SDMMC_1_ENABLED; - no define, leave as zero
    newfuse |= BCW_UART_CONSOLE_UART1_IOSET_1;
    newfuse |= BCW_JTAG_IOSET_3;
    newfuse |= BCW_EXT_MEM_BOOT_ENABLE;
    newfuse |= BCW_DISABLE_BSCR;

    //printf("Fuse value to be set: %#X\n", fuse);
    //if(newfuse == fuse) {
    //return 0;
    //}

    if((flag == 1) && (newfuse != fuse))
    {
#if 0
        SECURAM->BUREG[0] = newfuse;
#else
    	//usart_puts("Burning new boot fuses\n");
        SFC->SFC_DR[FUSE_BOOTCONFIG_WORD_POS] = newfuse;

        /* wait for completion */
        do {
            status = SFC->SFC_SR;
        } while ((status & SFC_SR_PGMC) == 0);

        //pmc_disable_peripheral(ID_SFC);
        // select peripheral
        *pmcPtr = PMC_PCR_PID(ID_SFC);

        // disable it but keep previous configuration
        *pmcPtr = (*pmcPtr & ~PMC_PCR_EN) | PMC_PCR_CMD;
#endif
    }
    
    return 0;
}
#endif

#ifdef CONFIG_HW_INIT
void hw_init(void)
{
	at91_disable_wdt();

	at91_red_led_on();

	if(burn_fuses(1) != 0) {
	    at91_red_led_off();
	}

	pmc_cfg_plla(PLLA_SETTINGS);

	/* Initialize PLLA charge pump */
	/* No need: we keep what is set in ROM code */
	//pmc_init_pll(0x3);
	pmc_cfg_mck(BOARD_PRESCALER_PLLA);

	writel(AT91C_RSTC_KEY_UNLOCK | AT91C_RSTC_URSTEN,
	       AT91C_BASE_RSTC + RSTC_RMR);

#ifdef CONFIG_MATRIX
	matrix_init();
#endif
	initialize_dbgu();

	timer_init();

#ifdef CONFIG_DDR2
	ddr2_init();
#endif
	l2cache_prepare();

	at91_init_can_message_ram();
}
#endif

#ifdef CONFIG_QSPI
void at91_qspi_hw_init(void)
{
#if defined(CONFIG_QSPI_BUS0)
#if defined(CONFIG_QSPI0_IOSET_1)
	const struct pio_desc qspi_pins[] = {
		{"QSPI0_SCK",	AT91C_PIN_PA(0), 0, PIO_DEFAULT, PIO_PERIPH_B},
		{"QSPI0_CS",	AT91C_PIN_PA(1), 0, PIO_DEFAULT, PIO_PERIPH_B},
		{"QSPI0_IO0",	AT91C_PIN_PA(2), 0, PIO_DEFAULT, PIO_PERIPH_B},
		{"QSPI0_IO1",	AT91C_PIN_PA(3), 0, PIO_DEFAULT, PIO_PERIPH_B},
		{"QSPI0_IO2",	AT91C_PIN_PA(4), 0, PIO_DEFAULT, PIO_PERIPH_B},
		{"QSPI0_IO3",	AT91C_PIN_PA(5), 0, PIO_DEFAULT, PIO_PERIPH_B},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};
#elif defined(CONFIG_QSPI0_IOSET_2)
	const struct pio_desc qspi_pins[] = {
		{"QSPI0_SCK",	AT91C_PIN_PA(14), 0, PIO_DEFAULT, PIO_PERIPH_C},
		{"QSPI0_CS",	AT91C_PIN_PA(15), 0, PIO_DEFAULT, PIO_PERIPH_C},
		{"QSPI0_IO0",	AT91C_PIN_PA(16), 0, PIO_DEFAULT, PIO_PERIPH_C},
		{"QSPI0_IO1",	AT91C_PIN_PA(17), 0, PIO_DEFAULT, PIO_PERIPH_C},
		{"QSPI0_IO2",	AT91C_PIN_PA(18), 0, PIO_DEFAULT, PIO_PERIPH_C},
		{"QSPI0_IO3",	AT91C_PIN_PA(19), 0, PIO_DEFAULT, PIO_PERIPH_C},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};
#elif defined(CONFIG_QSPI0_IOSET_3)
	const struct pio_desc qspi_pins[] = {
		{"QSPI0_SCK",	AT91C_PIN_PA(22), 0, PIO_DEFAULT, PIO_PERIPH_F},
		{"QSPI0_CS",	AT91C_PIN_PA(23), 0, PIO_DEFAULT, PIO_PERIPH_F},
		{"QSPI0_IO0",	AT91C_PIN_PA(24), 0, PIO_DEFAULT, PIO_PERIPH_F},
		{"QSPI0_IO1",	AT91C_PIN_PA(25), 0, PIO_DEFAULT, PIO_PERIPH_F},
		{"QSPI0_IO2",	AT91C_PIN_PA(26), 0, PIO_DEFAULT, PIO_PERIPH_F},
		{"QSPI0_IO3",	AT91C_PIN_PA(27), 0, PIO_DEFAULT, PIO_PERIPH_F},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};
#else
#error "No QSPI0 IOSET defined"
#endif

#elif defined(CONFIG_QSPI_BUS1)

#if defined(CONFIG_QSPI1_IOSET_1)
	const struct pio_desc qspi_pins[] = {
		{"QSPI1_SCK",	AT91C_PIN_PA(6),  0, PIO_DEFAULT, PIO_PERIPH_B},
		{"QSPI1_CS",	AT91C_PIN_PA(11), 0, PIO_DEFAULT, PIO_PERIPH_B},
		{"QSPI1_IO0",	AT91C_PIN_PA(7),  0, PIO_DEFAULT, PIO_PERIPH_B},
		{"QSPI1_IO1",	AT91C_PIN_PA(8),  0, PIO_DEFAULT, PIO_PERIPH_B},
		{"QSPI1_IO2",	AT91C_PIN_PA(9),  0, PIO_DEFAULT, PIO_PERIPH_B},
		{"QSPI1_IO3",	AT91C_PIN_PA(10), 0, PIO_DEFAULT, PIO_PERIPH_B},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};
#elif defined(CONFIG_QSPI1_IOSET_2)
	const struct pio_desc qspi_pins[] = {
		{"QSPI1_SCK",	AT91C_PIN_PB(5),  0, PIO_DEFAULT, PIO_PERIPH_D},
		{"QSPI1_CS",	AT91C_PIN_PB(6),  0, PIO_DEFAULT, PIO_PERIPH_D},
		{"QSPI1_IO0",	AT91C_PIN_PB(7),  0, PIO_DEFAULT, PIO_PERIPH_D},
		{"QSPI1_IO1",	AT91C_PIN_PB(8),  0, PIO_DEFAULT, PIO_PERIPH_D},
		{"QSPI1_IO2",	AT91C_PIN_PB(9),  0, PIO_DEFAULT, PIO_PERIPH_D},
		{"QSPI1_IO3",	AT91C_PIN_PB(10), 0, PIO_DEFAULT, PIO_PERIPH_D},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};
#elif defined(CONFIG_QSPI1_IOSET_3)
	const struct pio_desc qspi_pins[] = {
		{"QSPI1_SCK",	AT91C_PIN_PB(14), 0, PIO_DEFAULT, PIO_PERIPH_E},
		{"QSPI1_CS",	AT91C_PIN_PB(15), 0, PIO_DEFAULT, PIO_PERIPH_E},
		{"QSPI1_IO0",	AT91C_PIN_PB(16), 0, PIO_DEFAULT, PIO_PERIPH_E},
		{"QSPI1_IO1",	AT91C_PIN_PB(17), 0, PIO_DEFAULT, PIO_PERIPH_E},
		{"QSPI1_IO2",	AT91C_PIN_PB(18), 0, PIO_DEFAULT, PIO_PERIPH_E},
		{"QSPI1_IO3",	AT91C_PIN_PB(19), 0, PIO_DEFAULT, PIO_PERIPH_E},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};
#else
#error "No QSPI1 IOSET defined"
#endif
#else
#error "No QSPI Bus defined"
#endif

	pio_configure(qspi_pins);
	pmc_sam9x5_enable_periph_clk(CONFIG_SYS_ID_QSPI);
}
#endif

#ifdef CONFIG_SDCARD
#ifdef CONFIG_OF_LIBFDT
void at91_board_set_dtb_name(char *of_name)
{
	strcpy(of_name, "at91-sama5d27_som1_ek.dtb");
}
#endif

#define ATMEL_SDHC_GCKDIV_VALUE		1

void at91_sdhc_hw_init(void)
{
#ifdef CONFIG_SDHC0
	const struct pio_desc sdmmc_pins[] = {
		{"SDMMC0_CK",   AT91C_PIN_PA(0), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SDMMC0_CMD",  AT91C_PIN_PA(1), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SDMMC0_DAT0", AT91C_PIN_PA(2), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SDMMC0_DAT1", AT91C_PIN_PA(3), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SDMMC0_DAT2", AT91C_PIN_PA(4), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SDMMC0_DAT3", AT91C_PIN_PA(5), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SDMMC0_DAT4", AT91C_PIN_PA(6), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SDMMC0_DAT5", AT91C_PIN_PA(7), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SDMMC0_DAT6", AT91C_PIN_PA(8), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SDMMC0_DAT7", AT91C_PIN_PA(9), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SDMMC0_RSTN", AT91C_PIN_PA(10), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SDMMC0_VDDSEL", AT91C_PIN_PA(11), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SDMMC0_WP",   AT91C_PIN_PA(12), 1, PIO_DEFAULT, PIO_PERIPH_A},
		{"SDMMC0_CD",   AT91C_PIN_PA(13), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};
#endif

#ifdef CONFIG_SDHC1
	const struct pio_desc sdmmc_pins[] = {
		{"SDMMC1_DAT0",	AT91C_PIN_PA(18), 0, PIO_DEFAULT, PIO_PERIPH_E},
		{"SDMMC1_DAT1",	AT91C_PIN_PA(19), 0, PIO_DEFAULT, PIO_PERIPH_E},
		{"SDMMC1_DAT2",	AT91C_PIN_PA(20), 0, PIO_DEFAULT, PIO_PERIPH_E},
		{"SDMMC1_DAT3",	AT91C_PIN_PA(21), 0, PIO_DEFAULT, PIO_PERIPH_E},
		{"SDMMC1_CK",	AT91C_PIN_PA(22), 0, PIO_DEFAULT, PIO_PERIPH_E},
		{"SDMMC1_CMD",	AT91C_PIN_PA(28), 0, PIO_DEFAULT, PIO_PERIPH_E},
		{"SDMMC1_CD",	AT91C_PIN_PA(30), 0, PIO_DEFAULT, PIO_PERIPH_E},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};
#endif

	pio_configure(sdmmc_pins);

	pmc_sam9x5_enable_periph_clk(CONFIG_SYS_ID_SDHC);
	pmc_enable_periph_generated_clk(CONFIG_SYS_ID_SDHC,
					GCK_CSS_UPLL_CLK,
					ATMEL_SDHC_GCKDIV_VALUE);
}
#endif
