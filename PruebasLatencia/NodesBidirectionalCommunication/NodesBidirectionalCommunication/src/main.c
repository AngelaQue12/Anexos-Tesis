#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "comms.h"
#include "config_nodes.h"
#include "esp_system.h" 
#include "esp_random.h"
#include "test_latencia.h"
#include "broadcast_demo.h"
#include "types.h"
#define TARGET_NODE_ID 1  
//COMENTARIO DE UN DINOSAURIO ROSADO VOLADOR MACHAPE
#define TAG "MAIN"

extern paquete_t mensajeOut;

void task_envio(void *pvParameter) {
    while (1) {
        // Solo envío manual con botón
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_wifi_init(&(wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT()));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_recv_cb(on_receive);

    registrar_vecinos();
    inicializar_gpio_debug();

    xTaskCreate(task_envio, "task_envio", 4096, NULL, 5, NULL);
    // xTaskCreate(task_test_latencia, "test_latencia", 4096, NULL, 6, NULL); // Deshabilitado
    xTaskCreate(task_broadcast, "task_broadcast", 4096, NULL, 7, NULL);

    ESP_LOGI(TAG, "════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "MODO BROADCAST ACTIVADO: Presiona botón para enviar a todos los nodos (destino 255)");
    ESP_LOGI(TAG, "════════════════════════════════════════════════════════");

    // Mostrar configuración del sistema
    ESP_LOGI(TAG, "════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "CONFIGURACIÓN DE LA RED MESH:");
    ESP_LOGI(TAG, "   • NODO ACTUAL: %d", NODE_ID);
    ESP_LOGI(TAG, "   • VECINOS: %d nodos", numVecinos);
    ESP_LOGI(TAG, "════════════════════════════════════════════════════════");
    
    if (NODE_ID == TARGET_NODE_ID) {
        ESP_LOGI(TAG, "NODO %d iniciado como RECEPTOR. Esperando mensajes...", NODE_ID);
        ESP_LOGI(TAG, "LED se encenderá cuando lleguen mensajes");
    } else {
        ESP_LOGI(TAG, "NODO %d iniciado como EMISOR.", NODE_ID);
        ESP_LOGI(TAG, "Presiona botón (GPIO_0) para enviar mensaje al NODO %d", TARGET_NODE_ID);
    }
    
    if (NODE_ID == TARGET_NODE_ID) {
        ESP_LOGI(TAG, "NODO %d iniciado como RECEPTOR. Esperando mensajes...", NODE_ID);
        ESP_LOGI(TAG, "LED se encenderá cuando lleguen mensajes");
    } else {
        ESP_LOGI(TAG, "NODO %d iniciado como EMISOR.", NODE_ID);
        ESP_LOGI(TAG, "Presiona botón (GPIO_0) para enviar mensaje al NODO %d", TARGET_NODE_ID);
    }
    
    ESP_LOGI(TAG, "Logic Analyzer: GPIO_4 (digital) + GPIO_17 (UART)");
}