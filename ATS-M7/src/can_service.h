#ifndef CAN_SERVICE_H
#define CAN_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>

/* 初始化 CAN 控制器（模式、波特率、过滤器、启动） */
int can_service_init(void);

/* 设置接收过滤器（id/mask/标准帧或扩展帧） */
int can_service_set_rx_filter(uint32_t id, uint32_t mask, uint8_t flags);

/* 发送标准帧 */
int can_service_send(uint32_t id, const uint8_t *data, size_t len);

/* 发送扩展帧 */
int can_service_send_ext(uint32_t id, const uint8_t *data, size_t len);

/* 从接收消息队列读取一帧 */
int can_service_read(struct can_frame *frame, k_timeout_t timeout);

#endif /* CAN_SERVICE_H */
