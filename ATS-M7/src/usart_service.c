#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/ring_buffer.h>
#include <errno.h>

#define RX_BUF_SIZE 256
#define RX_RING_SIZE 512
#define TX_BUF_SIZE 256
#define RX_TIMEOUT_US 304 

/* -------------------------- DMA 必须用 nocache --------------------------- */
static uint8_t __aligned(32) __attribute__((section(".nocache.data"))) rx_buf[2][RX_BUF_SIZE];
static uint8_t __aligned(32) __attribute__((section(".nocache.data"))) tx_buf[TX_BUF_SIZE];

static int rx_buf_idx = 1;
RING_BUF_DECLARE(rx_ring, RX_RING_SIZE);

#define RS485_NODE DT_NODELABEL(rs485_0)
static const struct device *uart_dev = DEVICE_DT_GET(DT_PHANDLE(RS485_NODE, uart));
static const struct gpio_dt_spec de_gpio = GPIO_DT_SPEC_GET(RS485_NODE, de_gpios);

static struct k_sem tx_done_sem;

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
    switch (evt->type) {
        case UART_TX_DONE:
        case UART_TX_ABORTED:
            gpio_pin_set_dt(&de_gpio, 0);
            k_sem_give(&tx_done_sem);
            break;

        case UART_RX_RDY:
            printk("RX_RDY len=%d offset=%d\n",
                evt->data.rx.len,
                evt->data.rx.offset);


            ring_buf_put(&rx_ring,
                         evt->data.rx.buf + evt->data.rx.offset,
                         evt->data.rx.len);
            break;

        case UART_RX_BUF_REQUEST:
            uart_rx_buf_rsp(dev, rx_buf[rx_buf_idx], RX_BUF_SIZE);
            rx_buf_idx ^= 1;
            break;

        case UART_RX_DISABLED:
            uart_rx_enable(dev, rx_buf[0], RX_BUF_SIZE, RX_TIMEOUT_US);
            break;

        default:
            break;
    }
}

int rs485_init(void)
{
    if (!device_is_ready(uart_dev)) {
        printk("UART not ready\n");
        return -ENODEV;
    }
    if (!device_is_ready(de_gpio.port)) {
        printk("DE GPIO not ready\n");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&de_gpio, GPIO_OUTPUT_INACTIVE);
    k_sem_init(&tx_done_sem, 0, 1);

    uart_callback_set(uart_dev, uart_cb, NULL);

    uart_rx_enable(uart_dev, rx_buf[0], RX_BUF_SIZE, RX_TIMEOUT_US);

    printk("RS485 DMA TX + DMA RX 初始化成功\n");
    return 0;
}

/* -------------------------- DMA 发送 --------------------------- */
int rs485_write(const uint8_t *data, size_t len)
{
    int ret;

    if (len > TX_BUF_SIZE) {
        return -EINVAL;
    }

    // 复制到 nocache 缓冲区
    memcpy(tx_buf, data, len);

    gpio_pin_set_dt(&de_gpio, 1);
#if 1
    // 👈 DMA 发送
    ret = uart_tx(uart_dev, tx_buf, len, SYS_FOREVER_MS);
    if (ret != 0) {
        gpio_pin_set_dt(&de_gpio, 0);
        return ret;
    }
#else
    // 手动发送（阻塞）
    for (int i = 0; i < len; i++) {
        uart_poll_out(uart_dev, tx_buf[i]);
    }

    gpio_pin_set_dt(&de_gpio, 0);
#endif 

    k_sem_take(&tx_done_sem, K_FOREVER);
    return 0;
}

int rs485_read(uint8_t *buf, size_t max_len, k_timeout_t timeout)
{
    size_t total = 0;
    int64_t end = k_uptime_get() +  (timeout.ticks);

    while (total < max_len) {
        uint32_t len = ring_buf_get(&rx_ring, buf + total, max_len - total);
        if (len > 0) {
            total += len;
            continue;
        }
        if (timeout.ticks == K_NO_WAIT.ticks) break;
        if (k_uptime_get() > end) break;
        k_sleep(K_MSEC(1));
    }
    return total;
}
