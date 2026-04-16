#ifndef FLASHDB_PORT_ZEPHYR_H
#define FLASHDB_PORT_ZEPHYR_H

#include <stddef.h>
#include <sys/types.h>

int flashdb_port_init(void);
size_t flashdb_port_size(void);
size_t flashdb_port_align(void);
size_t flashdb_port_erase_size(void);
int flashdb_port_read(off_t off, void *buf, size_t len);
int flashdb_port_write(off_t off, const void *buf, size_t len);
int flashdb_port_erase(off_t off, size_t len);
int flashdb_port_self_test(void);

#endif /* FLASHDB_PORT_ZEPHYR_H */
