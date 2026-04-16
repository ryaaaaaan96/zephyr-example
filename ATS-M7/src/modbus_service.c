#include <zephyr/kernel.h>
#include <zephyr/modbus/modbus.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <errno.h>

#include "modbus_service.h"
#include "modbus_points_db.h"

/* 使用 overlay 中定义的 modbus0 节点（挂在 usart2 下） */
#define MODBUS_NODE DT_NODELABEL(modbus0)
BUILD_ASSERT(DT_NODE_HAS_STATUS(MODBUS_NODE, okay),
	"modbus0 node is missing or disabled");

/* 仅保留少量输入寄存器作为运行态只读信息 */
#define MB_INPUT_REG_NUM    4

static uint16_t input_reg[MB_INPUT_REG_NUM];
static int modbus_iface = -1;

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

/* Modbus Server 用户回调表 */
static struct modbus_user_callbacks mbs_cbs = {
	.coil_rd = coil_rd,
	.coil_wr = coil_wr,
	.input_reg_rd = input_reg_rd,
	.holding_reg_rd = holding_reg_rd,
	.holding_reg_wr = holding_reg_wr,
};

/* 串口参数：RTU、115200、8N1，站号 unit_id=1 */
static struct modbus_iface_param server_param = {
	.mode = MODBUS_MODE_RTU,
	.server = {
		.user_cb = &mbs_cbs,
		.unit_id = 1,
	},
	.serial = {
		.baud = 115200,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
	},
};

int modbus_service_init(void)
{
	const char iface_name[] = { DEVICE_DT_NAME(MODBUS_NODE) };
	int err;

	/* 通过设备名获取 Zephyr Modbus 接口索引 */
	modbus_iface = modbus_iface_get_by_name(iface_name);
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

	/* input_reg[0] 作为心跳计数寄存器 */
	input_reg[0] = 0;

	/* 启动 Modbus RTU Server */
	err = modbus_init_server(modbus_iface, server_param);
	if (err != 0) {
		printk("modbus_init_server failed: %d\n", err);
		return err;
	}

	printk("Modbus RTU server ready on %s (iface=%d, unit_id=1)\n",
		iface_name, modbus_iface);
	return 0;
}

void modbus_service_tick(void)
{
	/* 每秒自增一次，用于 FC04 读取在线心跳值 */
	input_reg[0]++;
}
