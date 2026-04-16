#include <zephyr/kernel.h>
#include <zephyr/modbus/modbus.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <errno.h>

#include "modbus_service.h"
#include "modbus_points_db.h"

#define MODBUS_NODE DT_NODELABEL(modbus0)
BUILD_ASSERT(DT_NODE_HAS_STATUS(MODBUS_NODE, okay),
	"modbus0 node is missing or disabled");

#define MB_INPUT_REG_NUM    4

static uint16_t input_reg[MB_INPUT_REG_NUM];
static int modbus_iface = -1;

static int coil_rd(uint16_t addr, bool *state)
{
	return modbus_points_db_get_coil(addr, state);
}

static int coil_wr(uint16_t addr, bool state)
{
	return modbus_points_db_set_coil(addr, state);
}

static int input_reg_rd(uint16_t addr, uint16_t *reg)
{
	if (addr >= MB_INPUT_REG_NUM) {
		return -ENOTSUP;
	}

	*reg = input_reg[addr];
	return 0;
}

static int holding_reg_rd(uint16_t addr, uint16_t *reg)
{
	return modbus_points_db_get_holding(addr, reg);
}

static int holding_reg_wr(uint16_t addr, uint16_t reg)
{
	return modbus_points_db_set_holding(addr, reg);
}

static struct modbus_user_callbacks mbs_cbs = {
	.coil_rd = coil_rd,
	.coil_wr = coil_wr,
	.input_reg_rd = input_reg_rd,
	.holding_reg_rd = holding_reg_rd,
	.holding_reg_wr = holding_reg_wr,
};

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

	modbus_iface = modbus_iface_get_by_name(iface_name);
	if (modbus_iface < 0) {
		printk("Modbus iface lookup failed: %d\n", modbus_iface);
		return modbus_iface;
	}

	err = modbus_points_db_init();
	if (err != 0) {
		printk("modbus_points_db_init failed: %d\n", err);
		return err;
	}

	input_reg[0] = 0;

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
	/* Keep one input register moving so FC04 can observe live data. */
	input_reg[0]++;
}
