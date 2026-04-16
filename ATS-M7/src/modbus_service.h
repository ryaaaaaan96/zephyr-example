#ifndef MODBUS_SERVICE_H
#define MODBUS_SERVICE_H

#include <stdint.h>

/* 初始化 Modbus RTU Server（串口、回调、点位持久化） */
int modbus_service_init(void);

/* 周期任务：用于维护少量运行时寄存器（如心跳） */
void modbus_service_tick(void);

#endif /* MODBUS_SERVICE_H */
