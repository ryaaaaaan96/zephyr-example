#ifndef USART_SERVICE_H
#define USART_SERVICE_H

#include <stddef.h>
#include <stdint.h>

int usart_service_init(void);
int usart_service_poll(uint8_t *c);

#endif