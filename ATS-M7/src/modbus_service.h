#ifndef MODBUS_SERVICE_H
#define MODBUS_SERVICE_H

#include <stdint.h>

int modbus_service_init(void);
void modbus_service_tick(void);

#endif /* MODBUS_SERVICE_H */
