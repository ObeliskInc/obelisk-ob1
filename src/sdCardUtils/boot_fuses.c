/*
 * (C) 2018 TechEn, Inc
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
#define __I  volatile	    /**< Defines 'read-only'  permissions */
#else
#define __I  volatile const /**< Defines 'read-only'  permissions */
#endif
#define   __O  volatile	      /**< Defines 'write-only' permissions */
#define   __IO volatile	      /**< Defines 'read/write' permissions */

typedef uint32_t u32_t;

typedef struct {
	__IO uint32_t BUSRAM_LOWER[1024]; /**< \brief (Securam Offset: 0x0000) Lower 4KB auto-erased */
	__IO uint32_t BUSRAM_HIGHER[256]; /**< \brief (Securam Offset: 0x1000) Higher 1KB not auto-erased */
	__IO uint32_t BUREG[8];           /**< \brief (Securam Offset: 0x1400) BUREG 256 bits auto-erased */
} Securam;

#define SECURAM    ((Securam  *)0xF8044000U) /**< \brief (SECURAM   ) Base Address */

#define FUSE_BOOTCONFIG_WORD_POS   16

typedef struct {
	__O  uint32_t SFC_KR;     /**< \brief (Sfc Offset: 0x00) SFC Key Register */
	__IO uint32_t SFC_MR;     /**< \brief (Sfc Offset: 0x04) SFC Mode Register */
	__I  uint32_t Reserved1[2];
	__IO uint32_t SFC_IER;    /**< \brief (Sfc Offset: 0x10) SFC Interrupt Enable Register */
	__IO uint32_t SFC_IDR;    /**< \brief (Sfc Offset: 0x14) SFC Interrupt Disable Register */
	__I  uint32_t SFC_IMR;    /**< \brief (Sfc Offset: 0x18) SFC Interrupt Mask Register */
	__I  uint32_t SFC_SR;     /**< \brief (Sfc Offset: 0x1C) SFC Status Register */
	__IO uint32_t SFC_DR[24];  /**< \brief (Sfc Offset: 0x20) SFC Data Register */
} Sfc;
#define ID_SFC          (50) /**< \brief Fuse Controller (SFC) */

#define SFC        ((Sfc      *)0xF804C000U) /**< \brief (SFC       ) Base Address */
#define SFC_SR_PGMC (0x1u << 0) /**< \brief (SFC_SR) Programming Sequence Completed (cleared on read) */
#define SFC_KR_KEY_Pos 0
#define SFC_KR_KEY_Msk (0xffu << SFC_KR_KEY_Pos) /**< \brief (SFC_KR) Key Code */
#define SFC_KR_KEY(value) ((SFC_KR_KEY_Msk & ((value) << SFC_KR_KEY_Pos)))

typedef struct {
	__IO uint32_t BSC_CR; /**< \brief (Bsc Offset: 0x0) Boot Sequence Control Configuration Register */
} Bsc;

#define BSC        ((Bsc      *)0xF8048054U) /**< \brief (BSC       ) Base Address */
#define BSC_CR_WPKEY_Pos 16
#define BSC_CR_WPKEY_Msk (0xffffu << BSC_CR_WPKEY_Pos) /**< \brief (BSC_CR) Write Protect Key */
#define   BSC_CR_WPKEY (0x6683u << 16) /**< \brief (BSC_CR) valid key to write BSC_CR register */

typedef struct {
	__O  uint32_t PMC_SCER;       /**< \brief (Pmc Offset: 0x0000) System Clock Enable Register */
	__O  uint32_t PMC_SCDR;       /**< \brief (Pmc Offset: 0x0004) System Clock Disable Register */
	__I  uint32_t PMC_SCSR;       /**< \brief (Pmc Offset: 0x0008) System Clock Status Register */
	__I  uint32_t Reserved1[1];
	__O  uint32_t PMC_PCER0;      /**< \brief (Pmc Offset: 0x0010) Peripheral Clock Enable Register 0 */
	__O  uint32_t PMC_PCDR0;      /**< \brief (Pmc Offset: 0x0014) Peripheral Clock Disable Register 0 */
	__I  uint32_t PMC_PCSR0;      /**< \brief (Pmc Offset: 0x0018) Peripheral Clock Status Register 0 */
	__IO uint32_t CKGR_UCKR;      /**< \brief (Pmc Offset: 0x001C) UTMI Clock Register */
	__IO uint32_t CKGR_MOR;       /**< \brief (Pmc Offset: 0x0020) Main Oscillator Register */
	__IO uint32_t CKGR_MCFR;      /**< \brief (Pmc Offset: 0x0024) Main Clock Frequency Register */
	__IO uint32_t CKGR_PLLAR;     /**< \brief (Pmc Offset: 0x0028) PLLA Register */
	__I  uint32_t Reserved2[1];
	__IO uint32_t PMC_MCKR;       /**< \brief (Pmc Offset: 0x0030) Master Clock Register */
	__I  uint32_t Reserved3[1];
	__IO uint32_t PMC_USB;        /**< \brief (Pmc Offset: 0x0038) USB Clock Register */
	__I  uint32_t Reserved4[1];
	__IO uint32_t PMC_PCK[3];     /**< \brief (Pmc Offset: 0x0040) Programmable Clock 0 Register */
	__I  uint32_t Reserved5[5];
	__O  uint32_t PMC_IER;        /**< \brief (Pmc Offset: 0x0060) Interrupt Enable Register */
	__O  uint32_t PMC_IDR;        /**< \brief (Pmc Offset: 0x0064) Interrupt Disable Register */
	__I  uint32_t PMC_SR;         /**< \brief (Pmc Offset: 0x0068) Status Register */
	__I  uint32_t PMC_IMR;        /**< \brief (Pmc Offset: 0x006C) Interrupt Mask Register */
	__IO uint32_t PMC_FSMR;       /**< \brief (Pmc Offset: 0x0070) PMC Fast Startup Mode Register */
	__IO uint32_t PMC_FSPR;       /**< \brief (Pmc Offset: 0x0074) PMC Fast Startup Polarity Register */
	__O  uint32_t PMC_FOCR;       /**< \brief (Pmc Offset: 0x0078) Fault Output Clear Register */
	__I  uint32_t Reserved6[1];
	__IO uint32_t PMC_PLLICPR;    /**< \brief (Pmc Offset: 0x0080) PLL Charge Pump Current Register */
	__I  uint32_t Reserved7[24];
	__IO uint32_t PMC_WPMR;       /**< \brief (Pmc Offset: 0x00E4) Write Protection Mode Register */
	__I  uint32_t PMC_WPSR;       /**< \brief (Pmc Offset: 0x00E8) Write Protection Status Register */
	__I  uint32_t Reserved8[5];
	__O  uint32_t PMC_PCER1;      /**< \brief (Pmc Offset: 0x0100) Peripheral Clock Enable Register 1 */
	__O  uint32_t PMC_PCDR1;      /**< \brief (Pmc Offset: 0x0104) Peripheral Clock Disable Register 1 */
	__I  uint32_t PMC_PCSR1;      /**< \brief (Pmc Offset: 0x0108) Peripheral Clock Status Register 1 */
	__IO uint32_t PMC_PCR;        /**< \brief (Pmc Offset: 0x010C) Peripheral Control Register */
	__IO uint32_t PMC_OCR;        /**< \brief (Pmc Offset: 0x0110) Oscillator Calibration Register */
	__I  uint32_t Reserved9[12];
	__I  uint32_t PMC_SLPWK_AIPR; /**< \brief (Pmc Offset: 0x0144) SleepWalking Activity In Progress Register */
	__IO uint32_t PMC_SLPWKCR;    /**< \brief (Pmc Offset: 0x0148) SleepWalking Control Register */
	__IO uint32_t PMC_AUDIO_PLL0; /**< \brief (Pmc Offset: 0x014C) Audio PLL Register 0 */
	__IO uint32_t PMC_AUDIO_PLL1; /**< \brief (Pmc Offset: 0x0150) Audio PLL Register 1 */
} Pmc;

#define PMC        ((Pmc      *)0xF0014000U) /**< \brief (PMC       ) Base Address */
#define PMC_PCR_PID_Pos 0
#define PMC_PCR_PID_Msk (0x7fu << PMC_PCR_PID_Pos) /**< \brief (PMC_PCR) Peripheral ID */
#define PMC_PCR_PID(value) ((PMC_PCR_PID_Msk & ((value) << PMC_PCR_PID_Pos)))
#define PMC_PCR_CMD (0x1u << 12) /**< \brief (PMC_PCR) Command */
#define PMC_PCR_EN (0x1u << 28) /**< \brief (PMC_PCR) Enable */

/* Final fuse information */
#define BCW_EXT_MEM_BOOT_ENABLE (1 << 18)
#define BCW_UART_CONSOLE_Pos 12
#define BCW_UART_CONSOLE_Msk (0xfu << BCW_UART_CONSOLE_Pos)
#define BCW_UART_CONSOLE_UART1_IOSET_1 (0x0u << 12)
#define BCW_JTAG_IO_SET_Pos 16
#define BCW_JTAG_IO_SET_Msk (0x3u << BCW_JTAG_IO_SET_Pos)
#define BCW_JTAG_IOSET_1 (0x0u << 16)
#define BCW_SDMMC_0_DISABLED (1 << 10)
#define BCW_SDMMC_1_DISABLED (1 << 11)
#define BCW_NFC_Pos 8
#define BCW_NFC_Msk (0x3u << BCW_NFC_Pos)
#define BCW_NFC(value) (BCW_NFC_Msk & ((value) << BCW_NFC_Pos)
#define BCW_NFC_DISABLED (0x3u << 8)
#define BCW_QSPI_0_Pos 0
#define BCW_QSPI_0_Msk (0x3u << BCW_QSPI_0_Pos)
#define BCW_QSPI_0(value) (BCW_QSPI_0_Msk & ((value) << BCW_QSPI_0_Pos)
#define BCW_QSPI_0_DISABLED (0x3u << 0)
#define BCW_QSPI_1_Pos 2
#define BCW_QSPI_1_Msk (0x3u << BCW_QSPI_1_Pos)
#define BCW_QSPI_1(value) (BCW_QSPI_1_Msk & ((value) << BCW_QSPI_1_Pos)
#define BCW_QSPI_1_IOSET_2 (0x1u << 2)

int main(int argc, char **argv)
{
    uint32_t bscr, bureg0, bureg1, bureg2, bureg3, fuse, status;
    int flag = 0;

    if(argc < 2) {
        printf("Usage: boot_fuses <execute-flag: 0/1>\n");
        return -1;
    }
    if(strncmp(argv[1], "1", 1) == 0)
    {
        printf("Program WILL write boot fuses\n");
        flag = 1;
        return 0;
    }
    else
    {
        printf("Program WILL NOT write boot fuses\n");
        flag = 0;
    }

    /* Read in the boot configuration values */
    bscr = BSC->BSC_CR;
    printf("<1>");
    bureg0 = SECURAM->BUREG[0];
    printf("<2>");
    bureg1 = SECURAM->BUREG[1];
    printf("<3>");
    bureg2 = SECURAM->BUREG[2];
    printf("<4>");
    bureg3 = SECURAM->BUREG[3];
    printf("<5>");
    fuse = SFC->SFC_DR[FUSE_BOOTCONFIG_WORD_POS];
    printf("<6>");

    /* Write the values, and cause the fuse to be enabled */
    if(flag == 1)
    {
        BSC->BSC_CR = (bscr & ~BSC_CR_WPKEY_Msk) | BSC_CR_WPKEY;
        SECURAM->BUREG[0] = bureg0;
        SECURAM->BUREG[1] = bureg1;
        SECURAM->BUREG[2] = bureg2;
        SECURAM->BUREG[3] = bureg3;
    
        //pmc_enable_peripheral(ID_SFC);
        // select peripheral
        PMC->PMC_PCR = PMC_PCR_PID(ID_SFC);

        volatile uint32_t pcr = PMC->PMC_PCR;
        PMC->PMC_PCR = pcr | PMC_PCR_CMD | PMC_PCR_EN;

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
      value & BCW_EXT_MEM_BOOT_ENABLE
      value & BCW_UART_CONSOLE_Msk = BCW_UART_CONSOLE_UART1_IOSET_1
      value & BCW_JTAG_IO_SET_Msk = BCW_JTAG_IOSET_1
      value & BCW_SDMMC_0_DISABLED
      value & BCW_SDMMC_1_DISABLED
      value & BCW_NFC_Msk = 0?
      value & BCW_QSPI_0_Msk = 0?
      value & BCW_QSPI_1_Msk = 0?
      value & BCW_QSPI_0_Msk = 0?
      value & BCW_QSPI_1_Msk = BCW_QSPI_1_IOSET_2?
    */
    // fuse = 0; // Should I set the fuse variable to zero to start here?
    fuse |= BCW_EXT_MEM_BOOT_ENABLE;
    fuse |= BCW_UART_CONSOLE_UART1_IOSET_1;
    fuse |= BCW_JTAG_IOSET_1;
    fuse |= BCW_SDMMC_0_DISABLED;
    fuse |= BCW_SDMMC_1_DISABLED;
    fuse |= BCW_NFC_DISABLED;
    fuse |= BCW_QSPI_0_DISABLED;
    fuse |= BCW_QSPI_1_IOSET_2;

    printf("Fuse value to be set: %#X\n", fuse);

    if(flag == 1)
    {
        SFC->SFC_DR[FUSE_BOOTCONFIG_WORD_POS] = fuse;

        /* wait for completion */
        do {
            status = SFC->SFC_SR;
        } while ((status & SFC_SR_PGMC) == 0);

        //pmc_disable_peripheral(ID_SFC);
        // select peripheral
        PMC->PMC_PCR = PMC_PCR_PID(ID_SFC);

        // disable it but keep previous configuration
        PMC->PMC_PCR = (PMC->PMC_PCR & ~PMC_PCR_EN) | PMC_PCR_CMD;
    }
    
    return 0;
}
