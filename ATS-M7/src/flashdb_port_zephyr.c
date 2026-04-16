#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <errno.h>
#include <string.h>

#include "flashdb_port_zephyr.h"

#define FLASHDB_PARTITION_NODE DT_NODELABEL(flashdb_partition)
BUILD_ASSERT(DT_NODE_HAS_STATUS(FLASHDB_PARTITION_NODE, okay),
	"flashdb_partition is missing or disabled");

#define FLASHDB_AREA_ID PARTITION_ID(flashdb_partition)

static const struct flash_area *flashdb_area;
static size_t flashdb_erase_size;
static size_t flashdb_write_align;

static int flashdb_check_range(off_t off, size_t len)
{
	if (flashdb_area == NULL) {
		return -EACCES;
	}

	if (off < 0 || (size_t)off > flashdb_area->fa_size ||
	    len > (flashdb_area->fa_size - (size_t)off)) {
		return -EINVAL;
	}

	return 0;
}

int flashdb_port_init(void)
{
	struct flash_pages_info page;
	int ret;

	ret = flash_area_open(FLASHDB_AREA_ID, &flashdb_area);
	if (ret != 0) {
		printk("flash_area_open(flashdb) failed: %d\n", ret);
		return ret;
	}

	if (!flash_area_device_is_ready(flashdb_area)) {
		printk("flashdb flash device not ready\n");
		return -ENODEV;
	}

	ret = flash_get_page_info_by_offs(flashdb_area->fa_dev,
					  flashdb_area->fa_off, &page);
	if (ret != 0) {
		printk("flash_get_page_info_by_offs failed: %d\n", ret);
		return ret;
	}

	flashdb_erase_size = page.size;
	flashdb_write_align = flash_area_align(flashdb_area);
	if (flashdb_write_align == 0U) {
		flashdb_write_align = 1U;
	}

	printk("FlashDB area ready: id=%u off=0x%lx size=0x%zx erase=0x%zx align=%zu\n",
		FLASHDB_AREA_ID,
		(unsigned long)flashdb_area->fa_off,
		flashdb_area->fa_size,
		flashdb_erase_size,
		flashdb_write_align);

	return 0;
}

size_t flashdb_port_size(void)
{
	return flashdb_area ? flashdb_area->fa_size : 0U;
}

size_t flashdb_port_align(void)
{
	return flashdb_write_align;
}

size_t flashdb_port_erase_size(void)
{
	return flashdb_erase_size;
}

int flashdb_port_read(off_t off, void *buf, size_t len)
{
	int ret = flashdb_check_range(off, len);

	if (ret != 0) {
		return ret;
	}

	return flash_area_read(flashdb_area, off, buf, len);
}

int flashdb_port_write(off_t off, const void *buf, size_t len)
{
	int ret = flashdb_check_range(off, len);

	if (ret != 0) {
		return ret;
	}

	return flash_area_write(flashdb_area, off, buf, len);
}

int flashdb_port_erase(off_t off, size_t len)
{
	int ret = flashdb_check_range(off, len);

	if (ret != 0) {
		return ret;
	}

	return flash_area_erase(flashdb_area, off, len);
}

int flashdb_port_self_test(void)
{
	uint8_t tx[64];
	uint8_t rx[64];
	const char msg[] = "flashdb-qspi-test";
	size_t align;
	size_t wr_len;
	int ret;

	if (flashdb_area == NULL) {
		return -EACCES;
	}

	align = flash_area_align(flashdb_area);
	if (align == 0U) {
		align = 1U;
	}

	wr_len = ROUND_UP(sizeof(msg), align);
	if (wr_len > sizeof(tx)) {
		return -ENOMEM;
	}

	memset(tx, 0xFF, sizeof(tx));
	memcpy(tx, msg, sizeof(msg));
	memset(rx, 0, sizeof(rx));

	ret = flashdb_port_erase(0, flashdb_erase_size);
	if (ret != 0) {
		printk("flashdb erase failed: %d\n", ret);
		return ret;
	}

	ret = flashdb_port_write(0, tx, wr_len);
	if (ret != 0) {
		printk("flashdb write failed: %d\n", ret);
		return ret;
	}

	ret = flashdb_port_read(0, rx, wr_len);
	if (ret != 0) {
		printk("flashdb read failed: %d\n", ret);
		return ret;
	}

	if (memcmp(tx, rx, wr_len) != 0) {
		printk("flashdb verify failed\n");
		return -EIO;
	}

	printk("FlashDB partition smoke test passed\n");
	return 0;
}
