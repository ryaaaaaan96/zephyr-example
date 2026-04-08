#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

/* 使用默认控制台串口 */
#define UART_NODE DT_CHOSEN(zephyr_console)

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

int main(void)
{
    uint8_t c;
    struct uart_config cfg;
    struct device temp;
    memcpy((void *)&temp, (void *)uart_dev, sizeof(temp));

    if (uart_config_get(uart_dev, &cfg) == 0) {
        printk("baudrate: %d\n", cfg.baudrate);
        printk("parity: %d\n", cfg.parity);
        printk("stop bits: %d\n", cfg.stop_bits);
        printk("data bits: %d\n", cfg.data_bits);
    }

    if (!device_is_ready(uart_dev)) 
    {
        printk("aaaaaa\n");
    }

    while (1) {

        /* 接收（如果有数据） */
        if (uart_poll_in(uart_dev, &c) == 0) {

            /* 回显（发送出去） */
            uart_poll_out(uart_dev, c);
        }
    }

    return 0;
}