#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <errno.h>
#include <string.h>

#include "flashdb_port_zephyr.h"
#include "modbus_points_db.h"

/* 记录头标识，用于判断 flash 中数据是否为本模块格式 */
#define MODBUS_POINTS_MAGIC   0x4D425044u /* "MBPD" */
/* 记录版本，后续格式升级时可据此做兼容 */
#define MODBUS_POINTS_VER     1u

/* 持久化记录布局：
 * - 整体写在 flashdb 分区偏移 0
 * - checksum 覆盖除自身外的全部字段
 */
struct modbus_points_record {
	uint32_t magic;
	uint16_t version;
	uint16_t reserved;
	uint16_t holding[MODBUS_POINT_HR_COUNT];
	uint8_t coils;
	uint8_t pad[3];
	uint32_t checksum;
};

/* 读-改-写流程需要互斥，防止并发请求导致脏写 */
static struct k_mutex points_db_lock;
static bool points_db_ready;

static uint32_t calc_checksum(const struct modbus_points_record *rec)
{
	const uint8_t *p = (const uint8_t *)rec;
	const size_t len = sizeof(*rec) - sizeof(rec->checksum);
	uint32_t sum = 0u;

	/* 简单滚动校验，开销低，足够用于掉电/坏数据检测 */
	for (size_t i = 0; i < len; i++) {
		sum = (sum * 33u) + p[i];
	}

	return sum;
}

static void set_defaults(struct modbus_points_record *rec)
{
	memset(rec, 0, sizeof(*rec));
	/* 默认参数可按现场需求调整 */
	rec->magic = MODBUS_POINTS_MAGIC;
	rec->version = MODBUS_POINTS_VER;
	rec->holding[MODBUS_POINT_HR_DEVICE_ID] = 1u;
	rec->holding[MODBUS_POINT_HR_THRESHOLD] = 100u;
	rec->holding[MODBUS_POINT_HR_RETRY_MS] = 500u;
	rec->holding[MODBUS_POINT_HR_USER_WORD] = 0x1234u;
	rec->coils = BIT(MODBUS_POINT_COIL_ENABLE);
	rec->checksum = calc_checksum(rec);
}

static bool is_record_valid(const struct modbus_points_record *rec)
{
	/* 先验头信息，再验 checksum，避免误判 */
	if (rec->magic != MODBUS_POINTS_MAGIC || rec->version != MODBUS_POINTS_VER) {
		return false;
	}

	return rec->checksum == calc_checksum(rec);
}

static int load_record(struct modbus_points_record *rec)
{
	/* 固定从分区起始地址读取一条记录 */
	int ret = flashdb_port_read(0, rec, sizeof(*rec));

	if (ret != 0) {
		return ret;
	}

	if (!is_record_valid(rec)) {
		return -EINVAL;
	}

	return 0;
}

static int save_record(const struct modbus_points_record *rec)
{
	uint8_t write_buf[128];
	size_t align = flashdb_port_align();
	size_t erase_size = flashdb_port_erase_size();
	size_t wr_len;
	int ret;

	if (align == 0U) {
		align = 1U;
	}
	if (erase_size == 0U) {
		return -ENODEV;
	}

	/* 写入长度必须按 flash 写对齐补齐到边界 */
	wr_len = ROUND_UP(sizeof(*rec), align);
	if (wr_len > sizeof(write_buf)) {
		return -ENOMEM;
	}

	memset(write_buf, 0xFF, sizeof(write_buf));
	memcpy(write_buf, rec, sizeof(*rec));

	/* NOR Flash 写入前必须先擦除扇区 */
	ret = flashdb_port_erase(0, erase_size);
	if (ret != 0) {
		return ret;
	}

	return flashdb_port_write(0, write_buf, wr_len);
}

static int load_or_reset_record(struct modbus_points_record *rec)
{
	int ret = load_record(rec);

	if (ret == 0) {
		return 0;
	}

	/* 首次启动或数据损坏：自动恢复默认值并回写 */
	set_defaults(rec);
	ret = save_record(rec);
	if (ret == 0) {
		printk("Modbus points DB initialized with defaults\n");
	}

	return ret;
}

int modbus_points_db_init(void)
{
	struct modbus_points_record rec;
	int ret;

	k_mutex_init(&points_db_lock);

	/* 仅在 init 成功后允许外部读写接口工作 */
	k_mutex_lock(&points_db_lock, K_FOREVER);
	ret = load_or_reset_record(&rec);
	if (ret == 0) {
		points_db_ready = true;
	}
	k_mutex_unlock(&points_db_lock);

	return ret;
}

int modbus_points_db_get_holding(uint16_t addr, uint16_t *value)
{
	struct modbus_points_record rec;
	int ret;

	if (!points_db_ready) {
		return -EACCES;
	}
	if (addr >= MODBUS_POINT_HR_COUNT) {
		return -ENOTSUP;
	}

	/* 每次读取都从 flash 取最新值，确保掉电恢复后可见 */
	k_mutex_lock(&points_db_lock, K_FOREVER);
	ret = load_or_reset_record(&rec);
	if (ret == 0) {
		*value = rec.holding[addr];
	}
	k_mutex_unlock(&points_db_lock);

	return ret;
}

int modbus_points_db_set_holding(uint16_t addr, uint16_t value)
{
	struct modbus_points_record rec;
	int ret;

	if (!points_db_ready) {
		return -EACCES;
	}
	if (addr >= MODBUS_POINT_HR_COUNT) {
		return -ENOTSUP;
	}

	/* 典型读改写流程：读记录 -> 改字段 -> 重算校验 -> 回写 */
	k_mutex_lock(&points_db_lock, K_FOREVER);
	ret = load_or_reset_record(&rec);
	if (ret == 0) {
		rec.holding[addr] = value;
		rec.checksum = calc_checksum(&rec);
		ret = save_record(&rec);
	}
	k_mutex_unlock(&points_db_lock);

	return ret;
}

int modbus_points_db_get_coil(uint16_t addr, bool *state)
{
	struct modbus_points_record rec;
	int ret;

	if (!points_db_ready) {
		return -EACCES;
	}
	if (addr >= MODBUS_POINT_COIL_COUNT) {
		return -ENOTSUP;
	}

	/* coil 使用位图存储，按 bit 位取值 */
	k_mutex_lock(&points_db_lock, K_FOREVER);
	ret = load_or_reset_record(&rec);
	if (ret == 0) {
		*state = (rec.coils & BIT(addr)) != 0U;
	}
	k_mutex_unlock(&points_db_lock);

	return ret;
}

int modbus_points_db_set_coil(uint16_t addr, bool state)
{
	struct modbus_points_record rec;
	int ret;

	if (!points_db_ready) {
		return -EACCES;
	}
	if (addr >= MODBUS_POINT_COIL_COUNT) {
		return -ENOTSUP;
	}

	/* 修改对应 bit 后整体回写记录 */
	k_mutex_lock(&points_db_lock, K_FOREVER);
	ret = load_or_reset_record(&rec);
	if (ret == 0) {
		if (state) {
			rec.coils |= BIT(addr);
		} else {
			rec.coils &= (uint8_t)(~BIT(addr));
		}
		rec.checksum = calc_checksum(&rec);
		ret = save_record(&rec);
	}
	k_mutex_unlock(&points_db_lock);

	return ret;
}
