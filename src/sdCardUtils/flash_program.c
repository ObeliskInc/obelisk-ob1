#include <stdio.h>

#typedef unsigned int uint32_t;

static struct spi_flash_cfg _flash_cfg = {
    .type = SPI_FLASH_TYPE_QSPI,
    .mode = SPI_FLASH_MODE0,
};

static struct spi_flash flash;

static bool configure_instance_pio(uint32_t instance, uint32_t ioset, Qspi** addr)
{
    int i;
    for (i = 0; i < num_qspiflash_pin_defs; i++) {
        const struct qspiflash_pin_definition* def =
            &qspiflash_pin_defs[i];
        if (def->instance == instance && def->ioset == ioset) {
            *addr = def->addr;
            pio_configure(def->pins, def->num_pins);
            return true;
        }
    }

    trace_error("Invalid configuration: QSPI%u IOSet%u\r\n",
        (unsigned)instance, (unsigned)ioset);
    return false;
}

static uint32_t handle_cmd_initialize(uint32_t cmd)
{
    uint32_t instance = 0;
    uint32_t ioset = 2;
    uint32_t freq = 20000000;

    Qspi* addr;
    if (!configure_instance_pio(instance, ioset, &addr))
        return APPLET_FAIL;

    trace_warning_wp("Initializing QSPI%u IOSet%u at %uHz\r\n",
            (unsigned)instance, (unsigned)ioset,
            (unsigned)freq);

    /* initialize the QSPI */
    _flash_cfg.qspi.addr = addr;
    _flash_cfg.baudrate = freq;
    if (spi_nor_configure(&flash, &_flash_cfg) < 0) {
        trace_error("Error while detecting QSPI flash chip\r\n");
        return APPLET_DEV_UNKNOWN;
    }

    uint32_t page_size = flash.page_size;
    uint32_t mem_size = flash.size;

    trace_warning_wp("Found Device %s\r\n", flash.name);
    trace_warning_wp("Size: %u bytes\r\n", (unsigned)mem_size);
    trace_warning_wp("Page Size: %u bytes\r\n", (unsigned)page_size);

    erase_support = spi_flash_get_uniform_erase_map(&flash);

    /* round buffer size to a multiple of page size and check if
     * it's big enough for at least one page */
    buffer = applet_buffer;
    buffer_size = applet_buffer_size & ~(page_size - 1);
    if (buffer_size == 0) {
        trace_error("Not enough memory for buffer\r\n");
        return APPLET_FAIL;
    }
    trace_warning_wp("Buffer Address: 0x%08x\r\n", (unsigned)buffer);
    trace_warning_wp("Buffer Size: %u bytes\r\n", (unsigned)buffer_size);

    mbx->out.buf_addr = (uint32_t)buffer;
    mbx->out.buf_size = buffer_size;
    mbx->out.page_size = page_size;
    mbx->out.mem_size = mem_size / page_size;
    mbx->out.erase_support = erase_support;
    mbx->out.nand_header = 0;

    trace_warning_wp("QSPI applet initialized successfully.\r\n");

    return APPLET_SUCCESS;
}

int main(void)
{
    handle_cmd_initialize();
}
