#ifndef QSPI_SERVICE_H
#define QSPI_SERVICE_H

#include <stddef.h>
#include <stdint.h>

int qspi_service_init(void);
int qspi_service_erase(void);
int qspi_service_write(const uint8_t *data, size_t len);
void qspi_service_dump(uint32_t off, size_t len);

#endif