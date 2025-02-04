#include "main.h"
// 1,5ms cada bit
// cada alto ou baixo é 500 us
void app_main() 
{
    // Inicializa o transmissor RF no pino 12
    init_rf_transmitter(RF_TX_PINO);
    // Inicializa o receptor RF no pino 13
    init_rf_receiver(RF_RX_PINO);
    
    // Inicializa o botão no pino 14
    init_button(BUTTON_PINO);

    // Loop principal para verificar se há dados recebidos
    while (1) {

        vTaskDelay(pdMS_TO_TICKS(100)); // Aguarda 10 ms antes de verificar novamente
    }
}