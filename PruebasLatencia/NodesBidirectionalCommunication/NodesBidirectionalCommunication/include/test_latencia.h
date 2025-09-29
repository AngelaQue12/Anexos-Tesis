#pragma once
#include "types.h"

void task_test_latencia(void *pvParameter);
void procesar_test_latencia(const paquete_t *msg, const uint8_t *mac);
void inicializar_gpio_test(void);

extern const int TARGET_NODE_ID;