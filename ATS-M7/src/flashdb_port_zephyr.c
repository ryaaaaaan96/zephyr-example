#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/flash/stm32_flash_api_extensions.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <soc.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "flashdb_port_zephyr.h"

/* 对应 overlay 中定义的 qspi 前半区分区 */
#define FLASHDB_PARTITION_NODE DT_NODELABEL(flashdb_partition)
BUILD_ASSERT(DT_NODE_HAS_STATUS(FLASHDB_PARTITION_NODE, okay),
	"flashdb_partition is missing or disabled");

/* Zephyr flash_map 分区 ID */
#define FLASHDB_AREA_ID PARTITION_ID(flashdb_partition)
#define FLASH_CMD_RDID_9F 0x9FU
#define FLASH_CMD_RDID_9E 0x9EU
#define FLASH_CMD_READ_SFDP 0x5AU
#define SFDP_MAGIC_LE 0x50444653UL

static const struct flash_area *flashdb_area;
static size_t flashdb_erase_size;
static size_t flashdb_write_align;

#if defined(CONFIG_FLASH_EX_OP_ENABLED) && defined(CONFIG_FLASH_STM32_QSPI_GENERIC_READ)
static int qspi_read_id_raw(const struct device *dev,
			    uint8_t opcode,
			    uint32_t instr_mode,
			    uint32_t data_mode,
			    uint8_t *out,
			    size_t out_len)
{
	QSPI_CommandTypeDef cmd = {
		.Instruction = opcode,
		.AddressSize = QSPI_ADDRESS_NONE,
		.AddressMode = QSPI_ADDRESS_NONE,
		.DummyCycles = 0U,
		.InstructionMode = instr_mode,
		.DataMode = data_mode,
		.NbData = out_len,
	};

	return flash_ex_op(dev, FLASH_STM32_QSPI_EX_OP_GENERIC_READ,
			   (uintptr_t)&cmd, out);
}

static int qspi_read_sfdp_raw_custom(const struct device *dev,
				     uint32_t addr,
				     uint8_t dummy_cycles,
				     uint32_t addr_size,
				     uint8_t *out,
				     size_t out_len)
{
	QSPI_CommandTypeDef cmd = {
		.Instruction = FLASH_CMD_READ_SFDP,
		.Address = addr,
		.AddressSize = addr_size,
		.AddressMode = QSPI_ADDRESS_1_LINE,
		.DummyCycles = dummy_cycles,
		.InstructionMode = QSPI_INSTRUCTION_1_LINE,
		.DataMode = QSPI_DATA_1_LINE,
		.NbData = out_len,
	};

	return flash_ex_op(dev, FLASH_STM32_QSPI_EX_OP_GENERIC_READ,
			   (uintptr_t)&cmd, out);
}

static int qspi_read_sfdp_raw(const struct device *dev,
			      uint32_t addr,
			      uint8_t *out,
			      size_t out_len)
{
	return qspi_read_sfdp_raw_custom(dev, addr, 8U, QSPI_ADDRESS_24_BITS, out, out_len);
}
#endif

static uint32_t u32_from_le_bytes(const uint8_t *p)
{
	return ((uint32_t)p[0]) |
	       ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

/* 越界保护：所有读写擦都先经过该检查 */
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

	/* 根据分区起始偏移查询擦除页大小 */
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

	/* flash 写入长度必须满足对齐约束 */
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

int flashdb_port_print_jedec_id(void)
{
	if (flashdb_area == NULL) {
		return -EACCES;
	}

#if defined(CONFIG_FLASH_EX_OP_ENABLED) && defined(CONFIG_FLASH_STM32_QSPI_GENERIC_READ)
	uint8_t id_spi_9f[4] = { 0 };
	uint8_t id_spi_9e[4] = { 0 };
	int ret_9f;
	int ret_spi_9e;

	ret_9f = qspi_read_id_raw(flashdb_area->fa_dev,
				  FLASH_CMD_RDID_9F,
				  QSPI_INSTRUCTION_1_LINE,
				  QSPI_DATA_1_LINE,
				  id_spi_9f, sizeof(id_spi_9f));
	ret_spi_9e = qspi_read_id_raw(flashdb_area->fa_dev,
				      FLASH_CMD_RDID_9E,
				      QSPI_INSTRUCTION_1_LINE,
				      QSPI_DATA_1_LINE,
				      id_spi_9e, sizeof(id_spi_9e));

	if (ret_9f == 0) {
		printk("RDID QSPI(1-1-1) 0x9F: %02x %02x %02x %02x\n",
		       id_spi_9f[0], id_spi_9f[1], id_spi_9f[2], id_spi_9f[3]);
		printk("RDID 0x9F decode: MID=%02x TYPE=%02x CAP=%02x DID3=%02x\n",
		       id_spi_9f[0], id_spi_9f[1], id_spi_9f[2], id_spi_9f[3]);
	} else {
		printk("RDID QSPI(1-1-1) 0x9F read failed: %d\n", ret_9f);
	}

	if (ret_spi_9e == 0) {
		printk("RDID QSPI(1-1-1) 0x9E: %02x %02x %02x %02x\n",
		       id_spi_9e[0], id_spi_9e[1], id_spi_9e[2], id_spi_9e[3]);
	} else {
		printk("RDID QSPI(1-1-1) 0x9E read failed: %d\n", ret_spi_9e);
	}

	if ((ret_9f == 0) || (ret_spi_9e == 0)) {
		return 0;
	}

	printk("raw RDID reads all failed\n");
#endif

#if defined(CONFIG_FLASH_JESD216_API)
	{
		uint8_t id[3] = { 0 };
		int ret;

		ret = flash_read_jedec_id(flashdb_area->fa_dev, id);
		if (ret != 0) {
			printk("flash_read_jedec_id failed: %d\n", ret);
			return ret;
		}

		printk("QSPI JEDEC ID (fallback, 3B): %02x %02x %02x\n",
		       id[0], id[1], id[2]);
		return 0;
	}
#else
	printk("flash_read_jedec_id unavailable: CONFIG_FLASH_JESD216_API is disabled\n");
	return -ENOTSUP;
#endif
}

int flashdb_port_print_sfdp_dw15(void)
{
	if (flashdb_area == NULL) {
		return -EACCES;
	}

#if defined(CONFIG_FLASH_EX_OP_ENABLED) && defined(CONFIG_FLASH_STM32_QSPI_GENERIC_READ)
	uint8_t sfdp_hdr[64] = { 0 };
	uint8_t bfp[80] = { 0 };
	uint32_t magic;
	uint32_t bfp_addr = 0U;
	size_t bfp_len_dw = 0U;
	uint8_t nph;
	size_t ph_count;
	size_t i;
	int ret;

	/* Manual-compatible mode: 5Ah + 24-bit address + 8 dummy + 1-1-1 lines */
	ret = qspi_read_sfdp_raw(flashdb_area->fa_dev, 0U, sfdp_hdr, sizeof(sfdp_hdr));
	if (ret != 0) {
		printk("SFDP raw read failed at 0x000000: %d\n", ret);
		return ret;
	}

	magic = u32_from_le_bytes(&sfdp_hdr[0]);
	nph = sfdp_hdr[6];
	ph_count = (size_t)nph + 1U;
	printk("SFDP header: magic=%02x %02x %02x %02x (0x%08lx) rev=%u.%u nph=%u access=0x%02x\n",
	       sfdp_hdr[0], sfdp_hdr[1], sfdp_hdr[2], sfdp_hdr[3], (unsigned long)magic,
	       sfdp_hdr[5], sfdp_hdr[4], nph, sfdp_hdr[7]);
	if (magic != SFDP_MAGIC_LE) {
		printk("SFDP magic mismatch\n");
		return -EINVAL;
	}

	for (i = 0; i < ph_count; i++) {
		size_t off = 8U + (i * 8U);
		uint16_t id;
		uint8_t len_dw;
		uint32_t addr;

		if ((off + 7U) >= sizeof(sfdp_hdr)) {
			printk("SFDP PH truncated at index %zu\n", i);
			break;
		}

		id = ((uint16_t)sfdp_hdr[off + 7U] << 8) | sfdp_hdr[off + 0U];
		len_dw = sfdp_hdr[off + 3U];
		addr = ((uint32_t)sfdp_hdr[off + 4U]) |
		       ((uint32_t)sfdp_hdr[off + 5U] << 8) |
		       ((uint32_t)sfdp_hdr[off + 6U] << 16);
		printk("SFDP PH%zu: id=%04x len_dw=%u addr=0x%06lx rev=%u.%u\n",
		       i, id, len_dw, (unsigned long)addr, sfdp_hdr[off + 2U], sfdp_hdr[off + 1U]);

		/* Basic Flash Parameter table (BFP) */
		if ((id == 0xFF00U) && (bfp_len_dw == 0U)) {
			bfp_addr = addr;
			bfp_len_dw = len_dw;
		}
	}

	if (bfp_len_dw < 15U) {
		printk("BFP length too short for DW15: %zu DW\n", bfp_len_dw);
		return -ENOTSUP;
	}

	ret = qspi_read_sfdp_raw(flashdb_area->fa_dev, bfp_addr, bfp, sizeof(bfp));
	if (ret != 0) {
		printk("BFP raw read failed at 0x%06lx: %d\n", (unsigned long)bfp_addr, ret);
		return ret;
	}

	{
		const size_t dw15_off = (15U - 1U) * 4U;
		uint32_t dw15 = u32_from_le_bytes(&bfp[dw15_off]);
		uint32_t qer = (dw15 >> 20) & 0x7U;
		printk("BFP DW15 @0x%06lx: %02x %02x %02x %02x (LE32=0x%08lx QER=%lu)\n",
		       (unsigned long)(bfp_addr + dw15_off),
		       bfp[dw15_off + 0U], bfp[dw15_off + 1U], bfp[dw15_off + 2U], bfp[dw15_off + 3U],
		       (unsigned long)dw15, (unsigned long)qer);
	}

	return 0;
#else
	printk("SFDP raw read unavailable: need CONFIG_FLASH_EX_OP_ENABLED + CONFIG_FLASH_STM32_QSPI_GENERIC_READ\n");
	return -ENOTSUP;
#endif
}
