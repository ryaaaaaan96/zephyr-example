#ifndef FLASHDB_PORT_ZEPHYR_H
#define FLASHDB_PORT_ZEPHYR_H

#include <stddef.h>
#include <sys/types.h>

/* 初始化 flashdb_partition 访问句柄与页参数 */
int flashdb_port_init(void);

/* 获取分区总大小 */
size_t flashdb_port_size(void);

/* 获取写对齐要求（字节） */
size_t flashdb_port_align(void);

/* 获取擦除粒度（页/扇区大小） */
size_t flashdb_port_erase_size(void);

/* 从分区内偏移读取数据 */
int flashdb_port_read(off_t off, void *buf, size_t len);

/* 向分区内偏移写入数据 */
int flashdb_port_write(off_t off, const void *buf, size_t len);

/* 擦除分区内偏移范围 */
int flashdb_port_erase(off_t off, size_t len);

/* 冒烟测试：erase->write->read->verify（会改写分区起始区域） */
int flashdb_port_self_test(void);

/* 打印原始 RDID（QSPI 控制器 1-1-1 模式下的 0x9F/0x9E），失败时回退 3 字节 JEDEC API */
int flashdb_port_print_jedec_id(void);

/* 打印 SFDP 头、参数头与 BFP DW15（包含 QER 位提取） */
int flashdb_port_print_sfdp_dw15(void);

#endif /* FLASHDB_PORT_ZEPHYR_H */
