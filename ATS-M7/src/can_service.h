#ifndef CAN_SERVICE_H
#define CAN_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>

int can_service_init(void);
int can_service_set_rx_filter(uint32_t id, uint32_t mask, uint8_t flags);
int can_service_send(uint32_t id, const uint8_t *data, size_t len);
int can_service_send_ext(uint32_t id, const uint8_t *data, size_t len);
int can_service_read(struct can_frame *frame, k_timeout_t timeout);

#endif
