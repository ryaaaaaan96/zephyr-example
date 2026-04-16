#ifndef RS485_SERVICE_H
#define RS485_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

/* 初始化 RS485 串口（DMA TX/RX + 回调 + 环形缓冲） */
int rs485_init(void);

/* 发送一帧数据（会自动控制 DE） */
int rs485_write(const uint8_t *data, size_t len);

/* 从接收环形缓冲读取数据 */
int rs485_read(uint8_t *buf, size_t max_len, k_timeout_t timeout);

#endif /* RS485_SERVICE_H */
