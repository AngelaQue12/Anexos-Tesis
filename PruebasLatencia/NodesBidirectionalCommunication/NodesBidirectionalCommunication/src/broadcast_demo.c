#ifndef MAX_IDS
#define MAX_IDS 50
#endif
#include "comms.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

uint32_t esp_random(void);
#include "esp_timer.h"
#include "types.h"
#include "config_nodes.h"

#define BUTTON_GPIO GPIO_NUM_0
#define LED_GPIO GPIO_NUM_2


#define BROADCAST_ID 255
#define BROADCAST_MSG 777
#define BROADCAST_ACK 778
#include "config_nodes.h"

static int ack_bitmap[MAX_IDS] = {0};
static int ack_count = 0;
static int waiting_broadcast_id = 0;
static int waiting_for_acks = 0;
static int64_t ack_wait_start = 0;

static const char *TAG = "BROADCAST";

extern paquete_t mensajeOut;

void inicializar_gpio_broadcast() {
    gpio_reset_pin(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "NODO %d listo para broadcast", NODE_ID);
}

void task_broadcast(void *pvParameter) {
    inicializar_gpio_broadcast();
    int last_state = 1;
    int todos_los_ids[NUM_NODOS-1];
    get_todos_los_ids(todos_los_ids);
    while (1) {
        int current_state = gpio_get_level(BUTTON_GPIO);
        int64_t now = esp_timer_get_time();
        // Cualquier nodo puede ser emisor
        if (current_state == 0 && last_state == 1 && !waiting_for_acks) {
            mensajeOut.origen = NODE_ID;
            mensajeOut.destino = BROADCAST_ID;
            mensajeOut.mensaje = BROADCAST_MSG;
            mensajeOut.ttl = 8;
            mensajeOut.id_mensaje = esp_random();
            for (int i = 0; i < MAX_IDS; i++) ack_bitmap[i] = 0;
            ack_count = 0;
            waiting_broadcast_id = mensajeOut.id_mensaje;
            waiting_for_acks = 1;
            ack_wait_start = now;
            gpio_set_level(LED_GPIO, 1);
            send_to_all_neighbors();
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_GPIO, 0);
        }
        // Todos los nodos controlan ACKs y timeout
        if (waiting_for_acks && ack_count >= (NUM_NODOS-1)) { // Espera ACKs de todos los otros nodos
            ESP_LOGI(TAG, "NODO %d: ¡Todos los ACKs recibidos!", NODE_ID);
            // Mostrar estado de cada nodo
            int todos_los_ids[NUM_NODOS-1];
            get_todos_los_ids(todos_los_ids);
            for (int i = 0; i < NUM_NODOS-1; i++) {
                if (ack_bitmap[i]) {
                    ESP_LOGI(TAG, "Nodo %d: CONECTADO", todos_los_ids[i]);
                } else {
                    ESP_LOGW(TAG, "Nodo %d: DESCONECTADO", todos_los_ids[i]);
                }
            }
            gpio_set_level(LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(3000));
            gpio_set_level(LED_GPIO, 0);
            waiting_for_acks = 0;
        }
        if (waiting_for_acks && (now - ack_wait_start) > 10000000) {
            ESP_LOGW(TAG, "NODO %d: Timeout esperando ACKs (%d/%d recibidos)", NODE_ID, ack_count, NUM_NODOS-1);
            // Mostrar estado de cada nodo
            int todos_los_ids[NUM_NODOS-1];
            get_todos_los_ids(todos_los_ids);
            for (int i = 0; i < NUM_NODOS-1; i++) {
                if (ack_bitmap[i]) {
                    ESP_LOGI(TAG, "Nodo %d: CONECTADO", todos_los_ids[i]);
                } else {
                    ESP_LOGW(TAG, "Nodo %d: DESCONECTADO", todos_los_ids[i]);
                }
            }
            for (int i = 0; i < 10; i++) {
                gpio_set_level(LED_GPIO, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED_GPIO, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            waiting_for_acks = 0;
        }
        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void procesar_broadcast(const paquete_t *msg, const uint8_t *mac) {
    if (msg->destino == BROADCAST_ID && msg->mensaje == BROADCAST_MSG) {
        ESP_LOGI(TAG, "NODO %d: Recibido BROADCAST de NODO %d (ID:%d)", NODE_ID, msg->origen, msg->id_mensaje);
        // Responder con ACK al emisor
        paquete_t ack;
        ack.origen = NODE_ID;
        ack.destino = msg->origen;
        ack.mensaje = BROADCAST_ACK;
        ack.ttl = 8;
        ack.id_mensaje = msg->id_mensaje;
        mensajeOut = ack;
        send_to_all_neighbors();
        // Reenviar el broadcast si TTL > 1
        if (msg->ttl > 1) {
            paquete_t fwd = *msg;
            fwd.ttl--;
            mensajeOut = fwd;
            send_to_all_neighbors();
        }
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(LED_GPIO, 0);
    } else if (msg->mensaje == BROADCAST_ACK) {
        ESP_LOGI(TAG, "NODO %d: Recibido ACK de %d para %d (ID:%d, TTL:%d)", NODE_ID, msg->origen, msg->destino, msg->id_mensaje, msg->ttl);
        // Reenviar el ACK si TTL > 1
        if (msg->ttl > 1) {
            paquete_t fwd = *msg;
            fwd.ttl--;
            mensajeOut = fwd;
            send_to_all_neighbors();
        }
        // Cualquier nodo emisor debe marcar los ACKs recibidos
        if (msg->destino == NODE_ID && waiting_for_acks && msg->id_mensaje == waiting_broadcast_id) {
            // Solo cuenta un ACK por nodo origen, y solo si es un nodo esperado
            int todos_los_ids[NUM_NODOS-1];
            get_todos_los_ids(todos_los_ids);
            int es_nodo = 0;
            int ya_conteo = 0;
            for (int i = 0; i < NUM_NODOS-1; i++) {
                if (todos_los_ids[i] == msg->origen) {
                    es_nodo = 1;
                    if (ack_bitmap[i] == 1) {
                        ya_conteo = 1;
                    }
                    break;
                }
            }
            if (es_nodo && !ya_conteo) {
                // Marca el ACK recibido de ese nodo
                for (int i = 0; i < NUM_NODOS-1; i++) {
                    if (todos_los_ids[i] == msg->origen) {
                        ack_bitmap[i] = 1;
                        break;
                    }
                }
                ack_count++;
                ESP_LOGI(TAG, "NODO %d: ACK recibido de nodo %d (%d/%d)", NODE_ID, msg->origen, ack_count, NUM_NODOS-1);
            }
        }
    }
}
