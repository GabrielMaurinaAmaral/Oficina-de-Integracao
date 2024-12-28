#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define RF_TX_PIN 4 // Defina o pino de transmissão RF
#define TX_CODE 0xAABBCCDD // Código a ser transmitido
#define BIT_DURATION 500 // Duração de cada bit em microssegundos

void send_bit(bool bit) {
    gpio_set_level(RF_TX_PIN, bit);
    ets_delay_us(BIT_DURATION);
}

void send_pulse(bool level, int duration) {
    gpio_set_level(RF_TX_PIN, level);
    ets_delay_us(duration);
}

void send_code(uint32_t code) {
    for (int i = 31; i >= 0; i--) {
        send_bit((code >> i) & 1);  // Extrai cada bit do código e envia
    }
    // Pulso de sincronização (pulso baixo longo)
    send_pulse(false, 10000);
}

// Tarefa do transmissor RF
void rf_transmitter_task(void *arg) {
    while (1) {
        printf("Transmitindo código RF: 0x%08X\n", TX_CODE); // Imprime o código que está sendo transmitido
        send_code(TX_CODE); // Envia o código
        vTaskDelay(pdMS_TO_TICKS(1000));  // Aguarda 1 segundo antes de enviar novamente
    }
}

void app_main(void) {
    // Configura o pino de transmissão RF
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RF_TX_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Cria a tarefa do transmissor RF
    xTaskCreate(rf_transmitter_task, "rf_transmitter_task", 2048, NULL, 10, NULL);
}