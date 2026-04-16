#ifndef MODBUS_SERVICE_H
#define MODBUS_SERVICE_H

#include <stdint.h>

/* 初始化 Modbus RAW Server（协议处理 + 应用层 RS485 收发） */
int modbus_service_init(void);

/* 周期任务：轮询 RS485 输入并维护少量运行时寄存器（如心跳） */
void modbus_service_tick(void);

#endif /* MODBUS_SERVICE_H */
