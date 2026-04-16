#ifndef QSPI_SERVICE_H
#define QSPI_SERVICE_H

#include <stddef.h>
#include <stdint.h>

/* 初始化 QSPI Flash 设备与基础参数 */
int qspi_service_init(void);

/* 擦除测试区域起始扇区 */
int qspi_service_erase(void);

/* 以顺序追加方式写入测试区域 */
int qspi_service_write(const uint8_t *data, size_t len);

/* 按十六进制打印指定区域内容 */
void qspi_service_dump(uint32_t off, size_t len);

#endif /* QSPI_SERVICE_H */
