#define MAX_ACKS 100
#include "broadcast_demo.h"
#define BROADCAST_ACK 778
#include "comms.h"
#include "esp_now.h"
#include "esp_log.h"
#include <stdbool.h>
#include <string.h>
#include "test_latencia.h"
#include "types.h"
#include <stdbool.h>
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config_nodes.h"
#define TARGET_NODE_ID 1  
#define TAG "COMMS"
#define MAX_IDS 50

#define DEBUG_SIGNAL GPIO_NUM_4    // Todos los nodos usan GPIO_4

static int ids_recibidos[MAX_IDS];
static int num_ids = 0;

paquete_t mensajeOut;
paquete_t mensajeIn;

typedef struct {
    int id_mensaje;
    int origen;
} ack_id_t;
static ack_id_t ids_acks_recibidos[MAX_ACKS];
static int num_acks = 0;

bool ack_repetido(int id_mensaje, int origen) {
    for (int i = 0; i < num_acks; i++) {
        if (ids_acks_recibidos[i].id_mensaje == id_mensaje && ids_acks_recibidos[i].origen == origen) {
            return true;
        }
    }
    if (num_acks < MAX_ACKS) {
        ids_acks_recibidos[num_acks].id_mensaje = id_mensaje;
        ids_acks_recibidos[num_acks].origen = origen;
        num_acks++;
    } else {
        // Desplazar y agregar al final
        for (int i = 1; i < MAX_ACKS; i++) ids_acks_recibidos[i-1] = ids_acks_recibidos[i];
        ids_acks_recibidos[MAX_ACKS-1].id_mensaje = id_mensaje;
        ids_acks_recibidos[MAX_ACKS-1].origen = origen;
    }
    return false;
}

bool mensaje_repetido(int id) {
    static int cleanup_counter = 0;
    cleanup_counter++;
    if (cleanup_counter > 100) {
        num_ids = 0;
        cleanup_counter = 0;
        ESP_LOGD(TAG, "Limpieza de IDs duplicados");
    }

    for (int i = 0; i < num_ids; i++) {
        if (ids_recibidos[i] == id) {
            return true;
        }
    }
    
    if (num_ids < MAX_IDS) {
        ids_recibidos[num_ids++] = id;
    } else {
        for (int i = 1; i < MAX_IDS; i++) {
            ids_recibidos[i-1] = ids_recibidos[i];
        }
        ids_recibidos[MAX_IDS-1] = id;
    }
    return false;
}

void on_receive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len != sizeof(paquete_t)) {
        ESP_LOGW(TAG, "Tamaño incorrecto: %d bytes", len);
        return;
    }

    paquete_t paquete;
    memcpy(&paquete, data, sizeof(paquete_t));

    // SEÑAL: ESTE NODO PROCESÓ UN MENSAJE
    gpio_set_level(DEBUG_SIGNAL, 1);

    ESP_LOGI(TAG, "NODO %d: Procesando %d→%d (TTL:%d, ID:%d)", 
             NODE_ID, paquete.origen, paquete.destino, paquete.ttl, paquete.id_mensaje);
   
    // Filtro de duplicados: si ya lo recibí, no proceso ni reenvío
    if (paquete.mensaje == BROADCAST_ACK) {
        if (ack_repetido(paquete.id_mensaje, paquete.origen)) {
            ESP_LOGD(TAG, "ACK duplicado ignorado (ID: %d, origen: %d)", paquete.id_mensaje, paquete.origen);
            gpio_set_level(DEBUG_SIGNAL, 0);
            return;
        }
    } else if (mensaje_repetido(paquete.id_mensaje)) {
        ESP_LOGD(TAG, "Duplicado ignorado (ID: %d)", paquete.id_mensaje);
        gpio_set_level(DEBUG_SIGNAL, 0);
        return;
    }

    // Reenvío multihop para cualquier mensaje con TTL > 1 (excepto si ya fue procesado)
    if (paquete.ttl > 1 && paquete.destino != NODE_ID) {
        paquete.ttl--;
        ESP_LOGI(TAG, "NODO %d: Reenviando %d→%d (TTL:%d)", NODE_ID, paquete.origen, paquete.destino, paquete.ttl);
        vTaskDelay(pdMS_TO_TICKS(30 + (NODE_ID * 10)));
        for (int i = 0; i < numVecinos; i++) {
            if (memcmp(vecinos[i], info->src_addr, 6) != 0) {
                esp_err_t result = esp_now_send(vecinos[i], (uint8_t*)&paquete, sizeof(paquete_t));
                if (result != ESP_OK) {
                    ESP_LOGW(TAG, "Error reenviando a vecino %d: %s", vecino_ids[i], esp_err_to_name(result));
                }
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Procesar mensaje dirigido a este nodo
    if (paquete.destino == 255 || paquete.mensaje == BROADCAST_ACK) {
        procesar_broadcast(&paquete, info->src_addr);
        vTaskDelay(pdMS_TO_TICKS(200));
    } else if (paquete.destino == NODE_ID) {
         ESP_LOGI(TAG, "NODO %d: Mensaje recibido de nodo %d", NODE_ID, paquete.origen);
         vTaskDelay(pdMS_TO_TICKS(100));
    } else {
        ESP_LOGW(TAG, "NODO %d: TTL agotado (%d)", NODE_ID, paquete.ttl);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Finalizar señal
    gpio_set_level(DEBUG_SIGNAL, 0);
}

void registrar_vecinos() {
    esp_now_peer_info_t peer = {0};
    peer.channel = 0;
    peer.encrypt = false;
    peer.ifidx = WIFI_IF_STA;

    ESP_LOGI(TAG, "NODO %d: Registrando %d vecinos...", NODE_ID, numVecinos);
    
    for (int i = 0; i < numVecinos; i++) {
        memcpy(peer.peer_addr, vecinos[i], 6);
        if (!esp_now_is_peer_exist(vecinos[i])) {
            if (esp_now_add_peer(&peer) == ESP_OK) {
                ESP_LOGI(TAG, "Vecino %d agregado: %02x:%02x:%02x:%02x:%02x:%02x", 
                         vecino_ids[i], vecinos[i][0], vecinos[i][1], vecinos[i][2], 
                         vecinos[i][3], vecinos[i][4], vecinos[i][5]);
            } else {
                ESP_LOGE(TAG, "Error al agregar vecino %d", vecino_ids[i]);
            }
        }
    }
    
    ESP_LOGI(TAG, "NODO %d: Registro completado", NODE_ID);
}

void send_to_all_neighbors() {
    for (int i = 0; i < numVecinos; i++) {
        esp_err_t result = esp_now_send(vecinos[i], (uint8_t *)&mensajeOut, sizeof(paquete_t));
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Error enviando a vecino %d: %s", vecino_ids[i], esp_err_to_name(result));
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void inicializar_gpio_debug() {
    // Inicializar señal de debug específica del nodo
    gpio_reset_pin(DEBUG_SIGNAL);
    gpio_set_direction(DEBUG_SIGNAL, GPIO_MODE_OUTPUT);
    gpio_set_level(DEBUG_SIGNAL, 0);
    
    // LED para nodo receptor (configurable)
    if (NODE_ID == TARGET_NODE_ID) {
        gpio_reset_pin(GPIO_NUM_2);
        gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_2, 0);
        ESP_LOGI(TAG, "NODO %d: LED receptor configurado", NODE_ID);
    }
    
    ESP_LOGI(TAG, "NODO %d: Señal debug GPIO_%d inicializada", NODE_ID, DEBUG_SIGNAL);
}