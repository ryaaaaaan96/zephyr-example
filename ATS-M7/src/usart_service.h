#ifndef RS485_SERVICE_H
#define RS485_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

int rs485_init(void);
int rs485_write(const uint8_t *data, size_t len);
int rs485_read(uint8_t *buf, size_t max_len, k_timeout_t timeout);

#endif
