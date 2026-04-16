#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "modbus_service.h"
#include "flashdb_port_zephyr.h"

/*
 * 应用主入口流程：
 * 1) 初始化 QSPI 分区访问层（供点位持久化使用）
 * 2) 初始化 Modbus RAW + 应用层 RS485 服务
 * 3) 高频执行 tick（驱动收包与心跳维护）
 */
int main(void)
{
	int ret;
	printk("flashdb_port_init start\n");
	ret = flashdb_port_init();
	if (ret != 0) {
		printk("flashdb_port_init failed: %d\n", ret);
		return 0;
	}

	ret = flashdb_port_print_jedec_id();
	if (ret != 0) {
		printk("flashdb_port_print_jedec_id failed: %d\n", ret);
	}

	ret = flashdb_port_print_sfdp_dw15();
	if (ret != 0) {
		printk("flashdb_port_print_sfdp_dw15 failed: %d\n", ret);
	}

	printk("flashdb_port_self_test start\n");
	ret = flashdb_port_self_test();
	if (ret != 0) {
		printk("flashdb_port_self_test failed: %d\n", ret);
		return 0;
	}

	ret = modbus_service_init();
	if (ret != 0) {
		printk("modbus_service_init failed: %d\n", ret);
		return 0;
	}

	while (1) {
		modbus_service_tick();
		k_sleep(K_MSEC(1));
	}

	return 0;
}
