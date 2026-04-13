#include <zephyr/kernel.h>
#include <string.h>
#include "usart_service.h"
#include "qspi_service.h"

#define RX_BUF_SIZE 64

int main(void)
{
	uint8_t c;
	uint8_t rx_buf[RX_BUF_SIZE];
	size_t rx_len = 0;

	if (usart_service_init() != 0) return 0;
	if (qspi_service_init() != 0) return 0;

	printk("QSPI before erase:\n");
	qspi_service_dump(0, 64);

	qspi_service_erase();

	while (1) {
		if (usart_service_poll(&c) == 0) {
			printk("%c", c);

			rx_buf[rx_len++] = c;

			if (c == '\n' || rx_len == RX_BUF_SIZE) {
				qspi_service_write(rx_buf, rx_len);
				rx_len = 0;
			}
		}
	}
}