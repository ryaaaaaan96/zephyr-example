#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "modbus_service.h"
#include "flashdb_port_zephyr.h"

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
