#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "modbus_service.h"
#include "flashdb_port_zephyr.h"

/*
 * 应用主入口流程：
 * 1) 初始化 QSPI 分区访问层（供点位持久化使用）
 * 2) 初始化 Modbus RTU 服务
 * 3) 周期执行轻量 tick（用于维护输入寄存器心跳）
 */
int main(void)
{
	int ret;

	ret = flashdb_port_init();
	if (ret != 0) {
		printk("flashdb_port_init failed: %d\n", ret);
		return 0;
	}

	ret = modbus_service_init();
	if (ret != 0) {
		printk("modbus_service_init failed: %d\n", ret);
		return 0;
	}

	while (1) {
		modbus_service_tick();
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
