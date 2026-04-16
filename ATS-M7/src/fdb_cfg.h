#ifndef FDB_CFG_H
#define FDB_CFG_H

#include <zephyr/sys/printk.h>

/* 仅启用 KVDB，满足 Modbus 点位持久化需求 */
#define FDB_USING_KVDB

/* 使用 FAL 模式，将 FlashDB 底层映射到 Zephyr QSPI 分区 */
#define FDB_USING_FAL_MODE

/*
 * 写粒度单位是 bit。
 * QSPI NOR Flash 对应 1 bit（可从 1 写成 0）。
 */
#define FDB_WRITE_GRAN 1

/* FlashDB 日志重定向到 Zephyr 控制台 */
#define FDB_PRINT(...) printk(__VA_ARGS__)

#endif /* FDB_CFG_H */
