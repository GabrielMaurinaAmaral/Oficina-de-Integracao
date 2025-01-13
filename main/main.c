#include "main.h"

void app_main() 
{
    // Inicializa o transmissor RF no pino 12
    init_rf_transmitter(RF_TX_PINO);
    // Inicializa o receptor RF no pino 13
    init_rf_receiver(RF_RX_PINO);
    
    // Inicializa o botão no pino 14
    init_button(14);

    // Loop principal para verificar se há dados recebidos
    while (1) {
        if (available()) {
            ESP_LOGI(TAG, "Received %lu / %dbit Protocol: %d.\n", getReceivedValue(), getReceivedBitlength(), getReceivedProtocol());
            resetAvailable();
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Aguarda 10 ms antes de verificar novamente
    }
}