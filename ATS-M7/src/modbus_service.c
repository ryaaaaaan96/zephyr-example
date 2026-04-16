#include <zephyr/kernel.h>
#include <zephyr/modbus/modbus.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <string.h>
#include <errno.h>

#include "modbus_service.h"
#include "modbus_points_db.h"
#include "usart_service.h"

/* 仅保留少量输入寄存器作为运行态只读信息 */
#define MB_INPUT_REG_NUM    4
#define MB_RAW_IFACE_NAME   "RAW_0"
#define MB_FRAME_MAX_LEN    CONFIG_MODBUS_BUFFER_SIZE
#define MB_POLL_BUF_LEN     64U
#define MB_FRAME_GAP_MS     2U
#define MB_ADU_PROTO_ID     0x0000U

static uint16_t input_reg[MB_INPUT_REG_NUM];
static int modbus_iface = -1;
static uint8_t mb_rx_frame[MB_FRAME_MAX_LEN];
static size_t mb_rx_len;
static int64_t mb_last_rx_ms;
static int64_t hb_last_ms;

/* Coil 读取：底层直接从持久化点位存储获取 */
static int coil_rd(uint16_t addr, bool *state)
{
	return modbus_points_db_get_coil(addr, state);
}

/* Coil 写入：写入后立即持久化到 flash 分区 */
static int coil_wr(uint16_t addr, bool state)
{
	return modbus_points_db_set_coil(addr, state);
}

/* Input Register 读取：当前采用内存态（不持久化） */
static int input_reg_rd(uint16_t addr, uint16_t *reg)
{
	if (addr >= MB_INPUT_REG_NUM) {
		return -ENOTSUP;
	}

	*reg = input_reg[addr];
	return 0;
}

/* Holding Register 读取：从持久化点位存储获取 */
static int holding_reg_rd(uint16_t addr, uint16_t *reg)
{
	return modbus_points_db_get_holding(addr, reg);
}

/* Holding Register 写入：更新后持久化 */
static int holding_reg_wr(uint16_t addr, uint16_t reg)
{
	return modbus_points_db_set_holding(addr, reg);
}

static int modbus_raw_tx_cb(const int iface, const struct modbus_adu *adu, void *user_data)
{
	uint8_t frame[MB_FRAME_MAX_LEN];
	uint16_t crc;
	size_t frame_len;
	int ret;

	ARG_UNUSED(iface);
	ARG_UNUSED(user_data);

	if (adu == NULL) {
		return -EINVAL;
	}

	if (adu->length > (MB_FRAME_MAX_LEN - 4U)) {
		return -EMSGSIZE;
	}

	frame[0] = adu->unit_id;
	frame[1] = adu->fc;
	if (adu->length > 0U) {
		memcpy(&frame[2], adu->data, adu->length);
	}

	crc = crc16_ansi(frame, adu->length + 2U);
	sys_put_le16(crc, &frame[2U + adu->length]);
	frame_len = adu->length + 4U;

	ret = rs485_write(frame, frame_len);
	if (ret != 0) {
		printk("rs485_write failed: %d\n", ret);
	}

	return ret;
}

/* Modbus Server 用户回调表 */
static struct modbus_user_callbacks mbs_cbs = {
	.coil_rd = coil_rd,
	.coil_wr = coil_wr,
	.input_reg_rd = input_reg_rd,
	.holding_reg_rd = holding_reg_rd,
	.holding_reg_wr = holding_reg_wr,
};

/* RAW 模式：协议栈仍使用标准 server 回调，串口由应用层自行管理 */
static struct modbus_iface_param server_param = {
	.mode = MODBUS_MODE_RAW,
	.server = {
		.user_cb = &mbs_cbs,
		.unit_id = 1,
	},
	.rawcb = {
		.raw_tx_cb = modbus_raw_tx_cb,
		.user_data = NULL,
	},
};

static void modbus_submit_frame(void)
{
	struct modbus_adu adu = { 0 };
	uint16_t rx_crc;
	uint16_t calc_crc;
	size_t payload_len;
	int err;

	if (mb_rx_len < 4U) {
		mb_rx_len = 0U;
		return;
	}

	rx_crc = sys_get_le16(&mb_rx_frame[mb_rx_len - 2U]);
	calc_crc = crc16_ansi(mb_rx_frame, mb_rx_len - 2U);
	if (rx_crc != calc_crc) {
		printk("Modbus RX CRC mismatch: rx=0x%04x calc=0x%04x len=%zu\n",
		       rx_crc, calc_crc, mb_rx_len);
		mb_rx_len = 0U;
		return;
	}

	payload_len = mb_rx_len - 4U;
	adu.proto_id = MB_ADU_PROTO_ID;
	adu.unit_id = mb_rx_frame[0];
	adu.fc = mb_rx_frame[1];
	adu.length = payload_len;
	adu.crc = rx_crc;
	if (payload_len > 0U) {
		memcpy(adu.data, &mb_rx_frame[2], payload_len);
	}

	err = modbus_raw_submit_rx(modbus_iface, &adu);
	if (err != 0) {
		printk("modbus_raw_submit_rx failed: %d\n", err);
	}

	mb_rx_len = 0U;
}

static void modbus_rx_poll(void)
{
	uint8_t tmp[MB_POLL_BUF_LEN];
	int rd;
	int64_t now;

	for (;;) {
		rd = rs485_read(tmp, sizeof(tmp), K_NO_WAIT);
		if (rd <= 0) {
			break;
		}

		if ((mb_rx_len + (size_t)rd) > sizeof(mb_rx_frame)) {
			printk("Modbus RX overflow, drop frame (len=%zu, add=%d)\n", mb_rx_len, rd);
			mb_rx_len = 0U;
		}

		if ((mb_rx_len + (size_t)rd) <= sizeof(mb_rx_frame)) {
			memcpy(&mb_rx_frame[mb_rx_len], tmp, (size_t)rd);
			mb_rx_len += (size_t)rd;
			mb_last_rx_ms = k_uptime_get();
		}
	}

	now = k_uptime_get();
	if (mb_rx_len >= 4U && (now - mb_last_rx_ms) >= MB_FRAME_GAP_MS) {
		modbus_submit_frame();
	}
}

int modbus_service_init(void)
{
	int err;

	/* RAW_0 为 Modbus RAW 接口名，由 CONFIG_MODBUS_NUMOF_RAW_ADU 提供 */
	modbus_iface = modbus_iface_get_by_name(MB_RAW_IFACE_NAME);
	if (modbus_iface < 0) {
		printk("Modbus iface lookup failed: %d\n", modbus_iface);
		return modbus_iface;
	}

	/* 初始化点位持久化数据（损坏时会自动恢复默认值） */
	err = modbus_points_db_init();
	if (err != 0) {
		printk("modbus_points_db_init failed: %d\n", err);
		return err;
	}

	err = rs485_init();
	if (err != 0) {
		printk("rs485_init failed: %d\n", err);
		return err;
	}

	/* input_reg[0] 作为心跳计数寄存器 */
	input_reg[0] = 0;
	mb_rx_len = 0U;
	mb_last_rx_ms = 0;
	hb_last_ms = k_uptime_get();

	/* 启动 Modbus RAW Server */
	err = modbus_init_server(modbus_iface, server_param);
	if (err != 0) {
		printk("modbus_init_server failed: %d\n", err);
		return err;
	}

	printk("Modbus RAW server ready on %s (iface=%d, unit_id=1)\n",
	       MB_RAW_IFACE_NAME, modbus_iface);
	return 0;
}

void modbus_service_tick(void)
{
	int64_t now = k_uptime_get();

	modbus_rx_poll();

	/* 每秒自增一次，用于 FC04 读取在线心跳值 */
	if ((now - hb_last_ms) >= 1000) {
		input_reg[0]++;
		hb_last_ms = now;
	}
}
