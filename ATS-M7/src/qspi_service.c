#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <string.h>
#include "qspi_service.h"

#define QSPI_FLASH_NODE DT_NODELABEL(qspi_flash)

#define QSPI_BASE_OFF    0u
#define QSPI_REGION_SIZE 4096u
#define RX_BUF_SIZE      64u

static const struct device *qspi_dev = DEVICE_DT_GET(QSPI_FLASH_NODE);

static uint32_t qspi_erase_size;
static uint32_t qspi_write_block;
static uint32_t qspi_write_off;

void qspi_service_dump(uint32_t off, size_t len)
{
    uint8_t buf[16];

    for (size_t i = 0; i < len; i += sizeof(buf)) {
        size_t chunk = len - i;
        if (chunk > sizeof(buf)) chunk = sizeof(buf);

        if (flash_read(qspi_dev, off + i, buf, chunk) != 0) {
            printk("QSPI read failed\n");
            return;
        }

        printk("0x%08x: ", (unsigned int)(off + i));
        for (size_t j = 0; j < chunk; j++) {
            printk("%02x ", buf[j]);
        }
        printk("\n");
    }
}

int qspi_service_init(void)
{
    const struct flash_parameters *params;
    struct flash_pages_info page;

    if (!device_is_ready(qspi_dev)) {
        printk("QSPI not ready\n");
        return -ENODEV;
    }

    params = flash_get_parameters(qspi_dev);
    qspi_write_block = params->write_block_size;

    if (flash_get_page_info_by_offs(qspi_dev, QSPI_BASE_OFF, &page) != 0) {
        return -EIO;
    }

    qspi_erase_size = page.size;
    qspi_write_off = QSPI_BASE_OFF;

    printk("QSPI init OK\n");
    return 0;
}

int qspi_service_erase(void)
{
    return flash_erase(qspi_dev, QSPI_BASE_OFF, qspi_erase_size);
}

int qspi_service_write(const uint8_t *data, size_t len)
{
    uint8_t tmp[RX_BUF_SIZE];
    size_t padded;

    memcpy(tmp, data, len);

    padded = len;
    if (padded % qspi_write_block) {
        padded = ((padded + qspi_write_block - 1) / qspi_write_block) * qspi_write_block;
    }

    memset(&tmp[len], 0xFF, padded - len);

    if ((qspi_write_off + padded) > (QSPI_BASE_OFF + QSPI_REGION_SIZE)) {
        qspi_service_erase();
        qspi_write_off = QSPI_BASE_OFF;
    }

    if (flash_write(qspi_dev, qspi_write_off, tmp, padded) != 0) {
        return -EIO;
    }

    qspi_write_off += padded;
    return 0;
}