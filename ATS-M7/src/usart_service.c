#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/ring_buffer.h>
#include <errno.h>

/* 缓冲参数 */
#define RX_BUF_SIZE 256
#define RX_RING_SIZE 512
#define TX_BUF_SIZE 256
#define RX_TIMEOUT_US 304

/* RS485 设备树节点：包含 uart 与 de-gpios */
#define RS485_NODE DT_NODELABEL(rs485_0)

/* USART 寄存器偏移（仅用于调试打印） */
#define UART_REG_BASE DT_REG_ADDR(DT_PHANDLE(RS485_NODE, uart))
#define USART_CR1_OFFSET  0x00
#define USART_CR2_OFFSET  0x04
#define USART_CR3_OFFSET  0x08
#define USART_RTOR_OFFSET 0x14
#define USART_ISR_OFFSET  0x1C

/*
 * DMA 缓冲放到 nocache 段：
 * 避免 D-Cache 与 DMA 同步问题导致收发数据异常。
 */
static uint8_t __aligned(32) __attribute__((section(".nocache.data"))) rx_buf[2][RX_BUF_SIZE];
static uint8_t __aligned(32) __attribute__((section(".nocache.data"))) tx_buf[TX_BUF_SIZE];

static int rx_buf_idx = 1;
RING_BUF_DECLARE(rx_ring, RX_RING_SIZE);

static const struct device *uart_dev = DEVICE_DT_GET(DT_PHANDLE(RS485_NODE, uart));
static const struct gpio_dt_spec de_gpio = GPIO_DT_SPEC_GET(RS485_NODE, de_gpios);

/* 等待发送完成信号量 */
static struct k_sem tx_done_sem;

/* 读 USART 寄存器（调试用） */
static uint32_t uart_reg_read(uint32_t offset)
{
	return *(volatile uint32_t *)(UART_REG_BASE + offset);
}

/* 打印串口关键寄存器状态（调试超时/中断问题时使用） */
static void uart_dump_regs(const char *tag)
{
	uint32_t cr1 = uart_reg_read(USART_CR1_OFFSET);
	uint32_t cr2 = uart_reg_read(USART_CR2_OFFSET);
	uint32_t cr3 = uart_reg_read(USART_CR3_OFFSET);
	uint32_t rtor = uart_reg_read(USART_RTOR_OFFSET);
	uint32_t isr = uart_reg_read(USART_ISR_OFFSET);

	printk("[%s] USART@0x%08x CR1=0x%08x CR2=0x%08x CR3=0x%08x RTOR=0x%08x ISR=0x%08x\n",
	       tag, UART_REG_BASE, cr1, cr2, cr3, rtor, isr);
	printk("[%s] IDLEIE=%d RTOIE=%d RTOEN=%d RTOR_RTO=%u IDLE=%d RTOF=%d\n",
	       tag,
	       !!(cr1 & USART_CR1_IDLEIE),
	       !!(cr1 & USART_CR1_RTOIE),
	       !!(cr2 & USART_CR2_RTOEN),
	       (unsigned int)(rtor & USART_RTOR_RTO),
	       !!(isr & USART_ISR_IDLE),
	       !!(isr & USART_ISR_RTOF));
}

/* UART 异步回调：处理 DMA 收发事件 */
static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	switch (evt->type) {
	case UART_TX_DONE:
	case UART_TX_ABORTED:
		/* 发送完成后拉低 DE，回到接收态 */
		gpio_pin_set_dt(&de_gpio, 0);
		k_sem_give(&tx_done_sem);
		break;

	case UART_RX_RDY:
		/* 把驱动缓冲中的新数据搬进环形缓冲 */
		printk("RX_RDY len=%d offset=%d\n",
		    evt->data.rx.len,
		    evt->data.rx.offset);

		ring_buf_put(&rx_ring,
			     evt->data.rx.buf + evt->data.rx.offset,
			     evt->data.rx.len);
		break;

	case UART_RX_BUF_REQUEST:
		/* 双缓冲切换：提供下一块 RX DMA 缓冲 */
		uart_rx_buf_rsp(dev, rx_buf[rx_buf_idx], RX_BUF_SIZE);
		rx_buf_idx ^= 1;
		break;

	case UART_RX_DISABLED:
		/* 若 RX 被关闭，立刻重启，确保持续接收 */
		uart_dump_regs("UART_RX_DISABLED before re-enable");
		uart_rx_enable(dev, rx_buf[0], RX_BUF_SIZE, RX_TIMEOUT_US);
		uart_dump_regs("UART_RX_DISABLED after re-enable");
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

	/* 启动异步接收（DMA） */
	uart_rx_enable(uart_dev, rx_buf[0], RX_BUF_SIZE, RX_TIMEOUT_US);
	uart_dump_regs("rs485_init after uart_rx_enable");

	printk("RS485 DMA TX + DMA RX 初始化成功\n");
	return 0;
}

/* DMA 发送 */
int rs485_write(const uint8_t *data, size_t len)
{
	int ret;

	if (len > TX_BUF_SIZE) {
		return -EINVAL;
	}

	/* 复制到 nocache 发送缓冲 */
	memcpy(tx_buf, data, len);

	/* 发送前拉高 DE，切换到驱动发送 */
	gpio_pin_set_dt(&de_gpio, 1);
#if 1
	ret = uart_tx(uart_dev, tx_buf, len, SYS_FOREVER_MS);
	if (ret != 0) {
		gpio_pin_set_dt(&de_gpio, 0);
		return ret;
	}
#else
	/* 备用阻塞发送路径（默认关闭） */
	for (int i = 0; i < len; i++) {
		uart_poll_out(uart_dev, tx_buf[i]);
	}

	gpio_pin_set_dt(&de_gpio, 0);
#endif

	/* 等待 TX_DONE 事件 */
	k_sem_take(&tx_done_sem, K_FOREVER);
	return 0;
}

int rs485_read(uint8_t *buf, size_t max_len, k_timeout_t timeout)
{
	size_t total = 0;
	int64_t end = k_uptime_get() + (timeout.ticks);

	while (total < max_len) {
		uint32_t len = ring_buf_get(&rx_ring, buf + total, max_len - total);
		if (len > 0) {
			total += len;
			continue;
		}
		if (timeout.ticks == K_NO_WAIT.ticks) {
			break;
		}
		if (k_uptime_get() > end) {
			break;
		}
		k_sleep(K_MSEC(1));
	}
	return total;
}
