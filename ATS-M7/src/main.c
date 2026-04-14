#include <zephyr/kernel.h>
#include <string.h>
#include "usart_service.h"
#include "qspi_service.h"

#define rx_buff_SIZE 64
/* 3️⃣ 循环读取 */
static uint8_t rx_buff[128];
int main(void)
{
    size_t rx_len = 0;
    int ret;

    if (rs485_init() != 0) return 0;
    if (qspi_service_init() != 0) return 0;

    printk("QSPI before erase:\n");
    qspi_service_dump(0, 64);

    qspi_service_erase();

    uint8_t tx[] = "Hello RS485";

    /* 2️⃣ 发送数据 */
    ret = rs485_write(tx, sizeof(tx));
    if (ret != 0) {
        printk("write failed: %d\n", ret);
    }


    while (1) {

        rx_len = rs485_read(rx_buff,
                            sizeof(rx_buff),
                            K_MSEC(200));

        if (rx_len > 0) {
            // printk("RX(%d): ", rx_len);

            // for (int i = 0; i < rx_len; i++) {
            //     printk("%02X ", rx_buff[i]);
            // }

            // printk("\n");
        }

        k_sleep(K_MSEC(100));
    }

    return 0;
}