#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <string.h>
#include "usart_service.h"
#include "qspi_service.h"
#include "can_service.h"

#define UART_TO_CAN_TX_ID 0x123U
#define UART_TO_CAN_EXT_ID 0
#define BRIDGE_RX_POLL_MS 20

static uint8_t rx_buff[128];

static void dump_bytes_limited(const uint8_t *data, size_t len)
{
    size_t show = MIN(len, (size_t)8);

    for (size_t i = 0; i < show; i++) {
        printk("%02X ", data[i]);
    }

    if (len > show) {
        printk("... ");
    }
}

int main(void)
{
    size_t rx_len = 0;
    struct can_frame can_rx_frame;
    size_t can_len;
    size_t offset;
    size_t chunk_len;
    int ret;

    if (rs485_init() != 0) return 0;
    if (qspi_service_init() != 0) return 0;
    if (can_service_init() != 0) return 0;

    printk("QSPI before erase:\n");
    qspi_service_dump(0, 64);

    qspi_service_erase();

    uint8_t tx[] = "Hello RS485";

    /* 2️⃣ 发送数据 */
    ret = rs485_write(tx, sizeof(tx));
    if (ret != 0) {
        printk("write failed: %d\n", ret);
    }

    printk("Bridge start: UART->CAN id=0x%03x (%s), CAN(all IDs)->RS485\n",
           UART_TO_CAN_TX_ID, UART_TO_CAN_EXT_ID ? "EXT" : "STD");

    while (1) {
        rx_len = rs485_read(rx_buff,
                            sizeof(rx_buff),
                            K_MSEC(BRIDGE_RX_POLL_MS));

        if (rx_len > 0) {
            printk("RS485->CAN rx_len=%d id=0x%03x data=",
                   (int)rx_len, UART_TO_CAN_TX_ID);
            dump_bytes_limited(rx_buff, rx_len);
            printk("\n");

            offset = 0;
            while (offset < rx_len) {
                chunk_len = MIN((size_t)CAN_MAX_DLC, rx_len - offset);

                if (UART_TO_CAN_EXT_ID) {
                    ret = can_service_send_ext(UART_TO_CAN_TX_ID,
                                               rx_buff + offset,
                                               chunk_len);
                } else {
                    ret = can_service_send(UART_TO_CAN_TX_ID,
                                           rx_buff + offset,
                                           chunk_len);
                }

                if (ret != 0) {
                    printk("RS485->CAN send failed ret=%d chunk=%d offset=%d\n",
                           ret, (int)chunk_len, (int)offset);
                    break;
                }

                printk("RS485->CAN tx ok id=0x%03x len=%d\n",
                       UART_TO_CAN_TX_ID, (int)chunk_len);
                offset += chunk_len;
            }
        }

        while (can_service_read(&can_rx_frame, K_NO_WAIT) == 0) {
            can_len = can_dlc_to_bytes(can_rx_frame.dlc);

            printk("CAN->RS485 rx id=0x%08x %s len=%d data=",
                   can_rx_frame.id,
                   (can_rx_frame.flags & CAN_FRAME_IDE) ? "EXT" : "STD",
                   (int)can_len);
            dump_bytes_limited(can_rx_frame.data, can_len);
            printk("\n");

            ret = rs485_write(can_rx_frame.data, can_len);
            if (ret != 0) {
                printk("CAN->RS485 write failed ret=%d id=0x%08x\n",
                       ret, can_rx_frame.id);
            } else {
                printk("CAN->RS485 tx ok len=%d\n", (int)can_len);
            }
        }

        k_sleep(K_MSEC(10));
    }

    return 0;
}
