#include "button.h"
#include "esp_log.h"

// Declaração da fila para comunicação entre a interrupção e a tarefa
QueueHandle_t BOTAO_queue = NULL;

// Dados a serem transmitidos
static unsigned long data = 143629925;
static int bitLength = 28;
static const Protocol *protocol = &proto[5]; // Protocolo 6

void IRAM_ATTR button_isr_handler(void* arg) {
    uint8_t dummy = 0;
    xQueueSendFromISR(BOTAO_queue, &dummy, NULL);
}

void button_task(void* arg) {
    uint8_t dummy;
    while (1) {
        if (xQueueReceive(BOTAO_queue, &dummy, portMAX_DELAY)) {
            sendRFSignal(data, bitLength, protocol);
        }
    }
}

void init_button(uint8_t button_pin) {
    if (BOTAO_queue == NULL) {
        BOTAO_queue = xQueueCreate(1, sizeof(uint8_t));
        if (BOTAO_queue != NULL) {
            gpio_config_t data_pin_BOTAO_config = {
                .intr_type = GPIO_INTR_ANYEDGE,
                .mode = GPIO_MODE_INPUT,
                .pin_bit_mask = (1ULL << button_pin),
                .pull_up_en = GPIO_PULLUP_ENABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE
            };

            printf("GPIO configurado: BOTAO_PINO = %d.\n", button_pin);
            printf("Aguardando inicialização...\n");

            vTaskDelay(pdMS_TO_TICKS(10));

            gpio_config(&data_pin_BOTAO_config);

            gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
            gpio_isr_handler_add(button_pin, button_isr_handler, NULL);
            xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
        }
    }
}