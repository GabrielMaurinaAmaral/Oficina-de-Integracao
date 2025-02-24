#include "main.h"

void app_main()
{
    // Inicializa o transmissor RF no pino 12
    init_rf_transmitter(RF_TX_PINO);
    // Inicializa o receptor RF no pino 13
    init_rf_receiver(RF_RX_PINO);
    // Inicializa o botão no pino 14
    init_button(BUTTON_PINO);
    // Inicializa o módulo BLE SPP
    init_ble_spp();
}