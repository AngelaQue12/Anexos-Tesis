#ifndef BROADCAST_DEMO_H
#define BROADCAST_DEMO_H

#include <stdint.h>
#include "types.h"

void inicializar_gpio_broadcast();
void task_broadcast(void *pvParameter);
void procesar_broadcast(const paquete_t *msg, const uint8_t *mac);

#endif // BROADCAST_DEMO_H
