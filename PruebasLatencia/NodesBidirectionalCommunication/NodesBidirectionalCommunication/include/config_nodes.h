#pragma once
#include <stdint.h>

#define NODE_ID 3

// IDs de todos los nodos de la red (ajusta según tu topología)

#define NUM_NODOS 8
// IDs de todos los nodos de la red (excepto el propio)
void get_todos_los_ids(int *out_array);

// Solo declaraciones extern
extern uint8_t vecinos[][6];
extern int numVecinos;
extern int vecino_ids[];