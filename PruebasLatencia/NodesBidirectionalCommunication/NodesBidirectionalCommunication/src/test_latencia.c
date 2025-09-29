#include "comms.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_log.h"
#include "types.h"
#include "esp_random.h"
#include "config_nodes.h"

#define BUTTON_GPIO GPIO_NUM_0
#define LED_GPIO GPIO_NUM_2
#define TARGET_NODE_ID 1  // cambiar según el nodo receptor
#define TEST_MENSAJE 555
#define ACK_MENSAJE 999

static int64_t tiempo_inicio = 0;
static bool esperando_respuesta = false;

void inicializar_gpio_test() {
    // Solo configurar botón en nodos emisores (2-8)
    // Nodo 1 es el receptor y no tiene botón pero se puede cambiar
    if (NODE_ID != TARGET_NODE_ID) {
        gpio_reset_pin(BUTTON_GPIO);
        gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
        gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);
        gpio_reset_pin(LED_GPIO);
        gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

        ESP_LOGI("TEST", "NODO %d: Emisor", NODE_ID);
    } else {
        ESP_LOGI("TEST", "NODO %d: Receptor", NODE_ID);
    }
}

void task_test_latencia(void *pvParameter) {
    inicializar_gpio_test();
    int last_state = 1;

    while (1) {
        // Solo nodos emisores (2-8) pueden enviar
        // Nodo 1 es el receptor y no tiene botón pero se puede cambiar
        if (NODE_ID != TARGET_NODE_ID) {
            int current_state = gpio_get_level(BUTTON_GPIO);

            if (current_state == 0 && last_state == 1 && !esperando_respuesta) {
                mensajeOut.origen = NODE_ID;
                mensajeOut.destino = TARGET_NODE_ID;
                mensajeOut.mensaje = TEST_MENSAJE;
                mensajeOut.ttl = 8;
                mensajeOut.id_mensaje = esp_random();
                
                tiempo_inicio = esp_timer_get_time();
                esperando_respuesta = true;

                ESP_LOGI("TEST", "NODO %d: ¡BOTÓN PRESIONADO! Enviando → NODO %d (ID:%d)", 
                         NODE_ID, TARGET_NODE_ID, mensajeOut.id_mensaje);

                // Flanco para logic analyzer
                gpio_set_level(GPIO_NUM_4, 1);
                esp_rom_delay_us(10);
                gpio_set_level(GPIO_NUM_4, 0);

                send_to_all_neighbors();
            }
            last_state = current_state;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void procesar_test_latencia(const paquete_t *msg, const uint8_t *mac) {
    // CASO 1: Soy el nodo receptor y recibí mensaje original
    if (NODE_ID == TARGET_NODE_ID && msg->mensaje == TEST_MENSAJE) {
        ESP_LOGI("TEST", "NODO %d (RECEPTOR): ¡Mensaje recibido de NODO %d! (ID:%d)", 
                 NODE_ID, msg->origen, msg->id_mensaje);
        
        // Enviar ACK de vuelta automáticamente
        paquete_t ack;
        ack.origen = NODE_ID;
        ack.destino = msg->origen;
        ack.mensaje = ACK_MENSAJE;
        ack.ttl = 8;
        ack.id_mensaje = esp_random();

        ESP_LOGI("TEST", "NODO %d (RECEPTOR): Enviando ACK → NODO %d (ID:%d)", 
                 NODE_ID, msg->origen, ack.id_mensaje);
        
        vTaskDelay(pdMS_TO_TICKS(100));
        mensajeOut = ack;
        send_to_all_neighbors();
        
        // Mantener LED encendido
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(GPIO_NUM_2, 0);
    }
    
    // CASO 2: Soy el nodo emisor original y recibí el ACK de vuelta
    else if (msg->mensaje == ACK_MENSAJE && msg->destino == NODE_ID && esperando_respuesta) {
        int64_t tiempo_fin = esp_timer_get_time();
        int64_t latencia = tiempo_fin - tiempo_inicio;
        
        ESP_LOGI("TEST", "NODO %d (EMISOR): ¡ACK recibido del NODO %d! Latencia: %lld μs (%.2f ms)", 
                 NODE_ID, msg->origen, latencia, latencia / 1000.0);
        
        esperando_respuesta = false;

        ESP_LOGI("TEST", "NODO %d: Comunicación completada exitosamente", NODE_ID);
        
        // Parpadear LED para indicar éxito 
        for(int i = 0; i < 3; i++) {
            gpio_set_level(LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level(LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}