#ifndef MAIN_H
#define MAIN_H

#include "esp32_rf_protocol.c"
#include "esp32_rf_receiver.c"
#include "esp32_rf_transmitter.c"
#include "button.c"
#include "ble_spp_server_demo.c"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_timer.h"

#define RF_TX_PINO 12 // Pino de transmissão RF (D12)
#define RF_RX_PINO 13 // Pino de recepção RF (D13)
#define BUTTON_PINO 14 // Pino do botão (D14)

void app_main();

#endif /* MAIN_H */