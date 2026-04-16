#ifndef MODBUS_POINTS_DB_H
#define MODBUS_POINTS_DB_H

#include <stdbool.h>
#include <stdint.h>

/* Holding Register 地址定义（FC03/FC06/FC16） */
#define MODBUS_POINT_HR_DEVICE_ID    0U
#define MODBUS_POINT_HR_THRESHOLD    1U
#define MODBUS_POINT_HR_RETRY_MS     2U
#define MODBUS_POINT_HR_USER_WORD    3U
#define MODBUS_POINT_HR_COUNT        4U

/* Coil 地址定义（FC01/FC05/FC15） */
#define MODBUS_POINT_COIL_ENABLE     0U
#define MODBUS_POINT_COIL_SAVE_EN    1U
#define MODBUS_POINT_COIL_COUNT      2U

/* 初始化 FlashDB KV 数据库 */
int modbus_points_db_init(void);

/* 读取/写入 Holding Register（底层映射为 FlashDB KV） */
int modbus_points_db_get_holding(uint16_t addr, uint16_t *value);
int modbus_points_db_set_holding(uint16_t addr, uint16_t value);

/* 读取/写入 Coil（底层映射为 FlashDB KV） */
int modbus_points_db_get_coil(uint16_t addr, bool *state);
int modbus_points_db_set_coil(uint16_t addr, bool state);

#endif /* MODBUS_POINTS_DB_H */
