#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <string.h>
#include <stdbool.h>

#include "fal.h"
#include "flashdb_port_zephyr.h"

/* FlashDB 初始化路径使用 "flashdb"，需与这里分区名一致 */
#define ATS_FAL_FLASH_NAME     "zephyr_qspi"
#define ATS_FAL_PART_NAME      "flashdb"
#define ATS_FAL_PART_MAGIC     0x45504641u /* "AFPE" */

static struct fal_flash_dev g_flash_dev;
static struct fal_partition g_partition;
static bool g_fal_inited;

static void copy_name(char *dst, size_t dst_size, const char *src)
{
	if (dst_size == 0U) {
		return;
	}

	strncpy(dst, src, dst_size - 1U);
	dst[dst_size - 1U] = '\0';
}

static bool part_is_valid(const struct fal_partition *part)
{
	return (part == &g_partition) && g_fal_inited;
}

int fal_init(void)
{
	int ret;
	size_t area_size;
	size_t erase_size;

	if (g_fal_inited) {
		return 1;
	}

	/* main.c 已初始化过时，这里可复用；未初始化时再补一次 */
	if (flashdb_port_size() == 0U) {
		ret = flashdb_port_init();
		if (ret != 0) {
			printk("fal_init: flashdb_port_init failed: %d\n", ret);
			return -1;
		}
	}

	area_size = flashdb_port_size();
	erase_size = flashdb_port_erase_size();
	if (area_size == 0U || erase_size == 0U) {
		printk("fal_init: invalid flashdb area size=%zu erase=%zu\n", area_size, erase_size);
		return -1;
	}

	memset(&g_flash_dev, 0, sizeof(g_flash_dev));
	copy_name(g_flash_dev.name, sizeof(g_flash_dev.name), ATS_FAL_FLASH_NAME);
	g_flash_dev.addr = 0U;
	g_flash_dev.len = area_size;
	g_flash_dev.blk_size = erase_size;
	g_flash_dev.write_gran = 1U;

	memset(&g_partition, 0, sizeof(g_partition));
	g_partition.magic_word = ATS_FAL_PART_MAGIC;
	copy_name(g_partition.name, sizeof(g_partition.name), ATS_FAL_PART_NAME);
	copy_name(g_partition.flash_name, sizeof(g_partition.flash_name), ATS_FAL_FLASH_NAME);
	g_partition.offset = 0;
	g_partition.len = area_size;
	g_partition.reserved = 0U;

	g_fal_inited = true;
	return 1;
}

const struct fal_flash_dev *fal_flash_device_find(const char *name)
{
	if (name == NULL) {
		return NULL;
	}

	if (!g_fal_inited && fal_init() < 0) {
		return NULL;
	}

	if (strcmp(name, g_flash_dev.name) == 0) {
		return &g_flash_dev;
	}

	return NULL;
}

const struct fal_partition *fal_partition_find(const char *name)
{
	if (name == NULL) {
		return NULL;
	}

	if (!g_fal_inited && fal_init() < 0) {
		return NULL;
	}

	if (strcmp(name, g_partition.name) == 0) {
		return &g_partition;
	}

	return NULL;
}

const struct fal_partition *fal_get_partition_table(size_t *len)
{
	if (!g_fal_inited && fal_init() < 0) {
		if (len != NULL) {
			*len = 0U;
		}
		return NULL;
	}

	if (len != NULL) {
		*len = 1U;
	}

	return &g_partition;
}

void fal_set_partition_table_temp(struct fal_partition *table, size_t len)
{
	ARG_UNUSED(table);
	ARG_UNUSED(len);
	/* 该简化适配层不支持运行时替换分区表 */
}

int fal_partition_read(const struct fal_partition *part, uint32_t addr, uint8_t *buf, size_t size)
{
	int ret;

	if (!part_is_valid(part) || buf == NULL) {
		return -1;
	}
	if ((size_t)addr > part->len || size > (part->len - (size_t)addr)) {
		return -1;
	}

	ret = flashdb_port_read((off_t)addr, buf, size);
	return (ret == 0) ? (int)size : -1;
}

int fal_partition_write(const struct fal_partition *part, uint32_t addr, const uint8_t *buf, size_t size)
{
	int ret;

	if (!part_is_valid(part) || buf == NULL) {
		return -1;
	}
	if ((size_t)addr > part->len || size > (part->len - (size_t)addr)) {
		return -1;
	}

	ret = flashdb_port_write((off_t)addr, buf, size);
	return (ret == 0) ? (int)size : -1;
}

int fal_partition_erase(const struct fal_partition *part, uint32_t addr, size_t size)
{
	int ret;

	if (!part_is_valid(part)) {
		return -1;
	}
	if ((size_t)addr > part->len || size > (part->len - (size_t)addr)) {
		return -1;
	}

	ret = flashdb_port_erase((off_t)addr, size);
	return (ret == 0) ? (int)size : -1;
}

int fal_partition_erase_all(const struct fal_partition *part)
{
	if (!part_is_valid(part)) {
		return -1;
	}

	return fal_partition_erase(part, 0U, part->len);
}

void fal_show_part_table(void)
{
	if (!g_fal_inited && fal_init() < 0) {
		printk("fal_show_part_table: not initialized\n");
		return;
	}

	printk("FAL partition: name=%s flash=%s offset=0x%lx len=0x%zx blk=0x%zx\n",
	       g_partition.name, g_partition.flash_name,
	       (unsigned long)g_partition.offset, g_partition.len, g_flash_dev.blk_size);
}
