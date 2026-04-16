#ifndef ATS_FAL_H
#define ATS_FAL_H

#include <stdint.h>
#include <stddef.h>

/* 与 FlashDB/FAL 保持一致的设备名长度 */
#ifndef FAL_DEV_NAME_MAX
#define FAL_DEV_NAME_MAX 24
#endif

/* 简化版 flash 设备结构，仅保留 FlashDB 会用到的字段 */
struct fal_flash_dev {
	char name[FAL_DEV_NAME_MAX];
	uint32_t addr;
	size_t len;
	size_t blk_size;
	struct {
		int (*init)(void);
		int (*read)(long offset, uint8_t *buf, size_t size);
		int (*write)(long offset, const uint8_t *buf, size_t size);
		int (*erase)(long offset, size_t size);
	} ops;
	size_t write_gran;
};

/* 简化版分区结构，满足 FlashDB KVDB 的依赖 */
struct fal_partition {
	uint32_t magic_word;
	char name[FAL_DEV_NAME_MAX];
	char flash_name[FAL_DEV_NAME_MAX];
	long offset;
	size_t len;
	uint32_t reserved;
};

int fal_init(void);
const struct fal_flash_dev *fal_flash_device_find(const char *name);
const struct fal_partition *fal_partition_find(const char *name);
const struct fal_partition *fal_get_partition_table(size_t *len);
void fal_set_partition_table_temp(struct fal_partition *table, size_t len);
int fal_partition_read(const struct fal_partition *part, uint32_t addr, uint8_t *buf, size_t size);
int fal_partition_write(const struct fal_partition *part, uint32_t addr, const uint8_t *buf, size_t size);
int fal_partition_erase(const struct fal_partition *part, uint32_t addr, size_t size);
int fal_partition_erase_all(const struct fal_partition *part);
void fal_show_part_table(void);

#endif /* ATS_FAL_H */
