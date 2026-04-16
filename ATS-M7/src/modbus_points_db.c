#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <flashdb.h>

#include "modbus_points_db.h"

/* KVDB 实例：用于保存 Modbus Holding/Coil 点位 */
static struct fdb_kvdb g_modbus_kvdb;
/* FlashDB 内部回调的互斥锁 */
static struct k_mutex g_db_lock;
static bool g_db_ready;

/* 点位与 KV 键名映射（地址不变，键名可读） */
static char g_key_hr0[] = "hr0";
static char g_key_hr1[] = "hr1";
static char g_key_hr2[] = "hr2";
static char g_key_hr3[] = "hr3";
static char g_key_coil0[] = "coil0";
static char g_key_coil1[] = "coil1";

static char *const g_holding_keys[MODBUS_POINT_HR_COUNT] = {
	g_key_hr0, g_key_hr1, g_key_hr2, g_key_hr3
};

static char *const g_coil_keys[MODBUS_POINT_COIL_COUNT] = {
	g_key_coil0, g_key_coil1
};

/* 默认值：首次格式化或数据缺失时写入 */
static uint16_t g_default_hr0 = 1u;
static uint16_t g_default_hr1 = 100u;
static uint16_t g_default_hr2 = 500u;
static uint16_t g_default_hr3 = 0x1234u;
static uint8_t g_default_coil0 = 1u;
static uint8_t g_default_coil1 = 0u;

static struct fdb_default_kv_node g_default_nodes[] = {
	{ .key = g_key_hr0, .value = &g_default_hr0, .value_len = sizeof(g_default_hr0) },
	{ .key = g_key_hr1, .value = &g_default_hr1, .value_len = sizeof(g_default_hr1) },
	{ .key = g_key_hr2, .value = &g_default_hr2, .value_len = sizeof(g_default_hr2) },
	{ .key = g_key_hr3, .value = &g_default_hr3, .value_len = sizeof(g_default_hr3) },
	{ .key = g_key_coil0, .value = &g_default_coil0, .value_len = sizeof(g_default_coil0) },
	{ .key = g_key_coil1, .value = &g_default_coil1, .value_len = sizeof(g_default_coil1) },
};

static struct fdb_default_kv g_default_kv = {
	.kvs = g_default_nodes,
	.num = ARRAY_SIZE(g_default_nodes),
};

static void kvdb_lock(fdb_db_t db)
{
	ARG_UNUSED(db);
	k_mutex_lock(&g_db_lock, K_FOREVER);
}

static void kvdb_unlock(fdb_db_t db)
{
	ARG_UNUSED(db);
	k_mutex_unlock(&g_db_lock);
}

static int fdb_to_errno(fdb_err_t err)
{
	switch (err) {
	case FDB_NO_ERR:
		return 0;
	case FDB_PART_NOT_FOUND:
		return -ENOENT;
	case FDB_SAVED_FULL:
		return -ENOSPC;
	case FDB_ERASE_ERR:
	case FDB_READ_ERR:
	case FDB_WRITE_ERR:
		return -EIO;
	case FDB_KV_NAME_ERR:
	case FDB_KV_NAME_EXIST:
		return -EINVAL;
	case FDB_INIT_FAILED:
	default:
		return -EFAULT;
	}
}

int modbus_points_db_init(void)
{
	fdb_err_t ret;

	if (g_db_ready) {
		return 0;
	}

	k_mutex_init(&g_db_lock);

	/* 设置 FlashDB 内部锁，保证多上下文访问安全 */
	fdb_kvdb_control(&g_modbus_kvdb, FDB_KVDB_CTRL_SET_LOCK, (void *)kvdb_lock);
	fdb_kvdb_control(&g_modbus_kvdb, FDB_KVDB_CTRL_SET_UNLOCK, (void *)kvdb_unlock);

	/*
	 * path 使用 "flashdb"，对应 FAL 分区名（由 fal_stub_zephyr.c 提供）。
	 * 初始化后，默认 KV 会自动落盘。
	 */
	ret = fdb_kvdb_init(&g_modbus_kvdb, "modbus", "flashdb", &g_default_kv, NULL);
	if (ret != FDB_NO_ERR) {
		printk("fdb_kvdb_init failed: %d\n", (int)ret);
		return fdb_to_errno(ret);
	}

	g_db_ready = true;
	return 0;
}

int modbus_points_db_get_holding(uint16_t addr, uint16_t *value)
{
	struct fdb_blob blob;
	size_t rd_len;
	uint16_t tmp = 0u;

	if (!g_db_ready) {
		return -EACCES;
	}
	if (value == NULL) {
		return -EINVAL;
	}
	if (addr >= MODBUS_POINT_HR_COUNT) {
		return -ENOTSUP;
	}

	rd_len = fdb_kv_get_blob(&g_modbus_kvdb, g_holding_keys[addr],
				 fdb_blob_make(&blob, &tmp, sizeof(tmp)));
	if (rd_len != sizeof(tmp)) {
		return -ENOENT;
	}

	*value = tmp;
	return 0;
}

int modbus_points_db_set_holding(uint16_t addr, uint16_t value)
{
	struct fdb_blob blob;
	fdb_err_t ret;

	if (!g_db_ready) {
		return -EACCES;
	}
	if (addr >= MODBUS_POINT_HR_COUNT) {
		return -ENOTSUP;
	}

	ret = fdb_kv_set_blob(&g_modbus_kvdb, g_holding_keys[addr],
			      fdb_blob_make(&blob, &value, sizeof(value)));
	return fdb_to_errno(ret);
}

int modbus_points_db_get_coil(uint16_t addr, bool *state)
{
	struct fdb_blob blob;
	size_t rd_len;
	uint8_t tmp = 0u;

	if (!g_db_ready) {
		return -EACCES;
	}
	if (state == NULL) {
		return -EINVAL;
	}
	if (addr >= MODBUS_POINT_COIL_COUNT) {
		return -ENOTSUP;
	}

	rd_len = fdb_kv_get_blob(&g_modbus_kvdb, g_coil_keys[addr],
				 fdb_blob_make(&blob, &tmp, sizeof(tmp)));
	if (rd_len != sizeof(tmp)) {
		return -ENOENT;
	}

	*state = (tmp != 0u);
	return 0;
}

int modbus_points_db_set_coil(uint16_t addr, bool state)
{
	struct fdb_blob blob;
	fdb_err_t ret;
	uint8_t value = state ? 1u : 0u;

	if (!g_db_ready) {
		return -EACCES;
	}
	if (addr >= MODBUS_POINT_COIL_COUNT) {
		return -ENOTSUP;
	}

	ret = fdb_kv_set_blob(&g_modbus_kvdb, g_coil_keys[addr],
			      fdb_blob_make(&blob, &value, sizeof(value)));
	return fdb_to_errno(ret);
}
