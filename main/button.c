#include "button.h"
#include "esp_log.h"

// Declaração da fila para comunicação entre a interrupção e a tarefa
QueueHandle_t BOTAO_queue = NULL;

// Dados a serem transmitidos
volatile unsigned long receivedData = 0;
volatile int receivedBitLength = 0;
volatile int receivedProtocolIndex = 0;

// Variável para armazenar o estado do botão
volatile bool button_pressed = false;

void IRAM_ATTR button_isr_handler(void* arg) {
    uint8_t dummy = 0;
    xQueueSendFromISR(BOTAO_queue, &dummy, NULL);
}

void button_task(void* arg) {
    uint8_t dummy;
    while (1) {
        if (xQueueReceive(BOTAO_queue, &dummy, portMAX_DELAY)) {
            // Verifica o estado atual do botão
            bool current_state = (gpio_get_level(14) == 0);

            if (current_state && !button_pressed) {
                // Botão foi pressionado
                button_pressed = true;

                // Receber os dados do receptor
                receivedData = getReceivedValue(); 
                receivedBitLength = getReceivedBitlength();
                receivedProtocolIndex = getReceivedProtocol() - 1;

                ESP_LOGI("BUTTON", "Dados recebidos: %lu, BitLength: %d, ProtocolIndex: %d", receivedData, receivedBitLength, receivedProtocolIndex);

                // Verificar se os dados recebidos são válidos
                if (receivedProtocolIndex >= 0 && receivedProtocolIndex < sizeof(proto) / sizeof(proto[0])) {
                    const Protocol *receivedProtocol = &proto[receivedProtocolIndex];

                    // Enviar o sinal RF continuamente enquanto o botão estiver pressionado
                    while (button_pressed) {
                        sendRFSignal(receivedData, receivedBitLength, receivedProtocol);
                        button_pressed = (gpio_get_level(14) == 0); // Atualiza o estado do botão
                    }
                } else {
                    ESP_LOGE("BUTTON", "Protocolo recebido inválido: %d", receivedProtocolIndex + 1);
                }
            } else if (!current_state && button_pressed) {
                // Botão foi solto
                button_pressed = false;
            }
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

            gpio_config(&data_pin_BOTAO_config);
            
            gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
            gpio_isr_handler_add(button_pin, button_isr_handler, NULL);
            xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);

            printf("GPIO configurado: BOTAO_PINO = %d.\n", button_pin);
            printf("Aguardando inicialização...\n");

            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}