#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include "usart_service.h"

#define UART_NODE DT_NODELABEL(usart2)

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

int usart_service_init(void)
{
    if (!device_is_ready(uart_dev)) {
        printk("UART not ready\n");
        return -ENODEV;
    }

    printk("UART init OK\n");
    return 0;
}

int usart_service_poll(uint8_t *c)
{
    return uart_poll_in(uart_dev, c);
}