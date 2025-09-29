#pragma once
#include "esp_now.h"
#include "types.h"

void on_receive(const esp_now_recv_info_t *info, const uint8_t *data, int len);
void registrar_vecinos(void);
void send_to_all_neighbors(void);
void inicializar_gpio_debug(void);
bool mensaje_repetido(int id);

extern paquete_t mensajeOut;
extern paquete_t mensajeIn;