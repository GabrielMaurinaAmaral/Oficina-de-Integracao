#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

// GPIOs para Transmissor e Receptor
#define RF_TX_PIN GPIO_NUM_5 // Pino para o transmissor RF
#define RF_RX_PIN GPIO_NUM_4 // Pino para o receptor RF

// Configurações dos tempos de pulso
#define PULSE_MIN 300    // Duração mínima de um pulso em microsegundos
#define PULSE_MAX 1200   // Duração máxima de um pulso em microsegundos
#define SYNC_PULSE 9000  // Duração mínima do pulso de sincronização

// Código a ser enviado pelo transmissor
#define TX_CODE 0xA1B2C3D4

// Função para enviar pulsos (bit alto ou baixo)
void send_pulse(bool high, int duration_us) {
    gpio_set_level(RF_TX_PIN, high ? 1 : 0); // Define o nível do pino (alto ou baixo)
    ets_delay_us(duration_us);  // Aguarda o tempo especificado em microsegundos
}

// Função para enviar um bit
void send_bit(bool bit) {
    if (bit) {
        send_pulse(true, 350);  // Pulso alto para bit 1
        send_pulse(false, 1050); // Pulso baixo para bit 1
    } else {
        send_pulse(true, 350);  // Pulso alto para bit 0
        send_pulse(false, 350);  // Pulso baixo para bit 0
    }
}

// Função para enviar um código (32 bits neste exemplo)
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

// Tarefa para capturar e decodificar o sinal recebido
void rf_receiver_task(void *arg) {
    uint32_t received_code = 0; // Variável para armazenar o código recebido
    int bit_count = 0; // Contador de bits recebidos

    while (1) {
        // Espera até o pino mudar de estado (de baixo para alto)
        while (gpio_get_level(RF_RX_PIN) == 0);

        // Medir o tempo do pulso
        int64_t start_time = esp_timer_get_time(); // Captura o tempo de início do pulso
        while (gpio_get_level(RF_RX_PIN) == 1); // Espera até o pino mudar de estado (de alto para baixo)
        int64_t duration = esp_timer_get_time() - start_time; // Calcula a duração do pulso

        if (duration > SYNC_PULSE) {
            // Pulso de sincronização detectado
            if (bit_count == 32) {
                printf("Código recebido: 0x%08X\n", received_code); // Imprime o código recebido
                // Envia o código recebido via serial
                printf("DATA:%08X\n", received_code);
            } else if (bit_count > 0) {
                printf("Erro: Código incompleto (%d bits recebidos).\n", bit_count); // Imprime erro se o código estiver incompleto
            }
            received_code = 0; // Reseta o código recebido
            bit_count = 0;     // Reseta o contador de bits
        } else if (duration >= PULSE_MIN && duration <= PULSE_MAX) {
            // Determinar se é um bit 1 ou 0 baseado no tempo do pulso
            received_code <<= 1; // Desloca o código para a esquerda para adicionar o novo bit
            if (duration > (PULSE_MIN + PULSE_MAX) / 2) {
                received_code |= 1; // Adiciona o bit 1
            }
            bit_count++;
        }

        // Tempo limite de recepção para evitar travamentos
        if (bit_count > 32) {
            printf("Erro: Número de bits excedido.\n"); // Imprime erro se o número de bits exceder 32
            received_code = 0; // Reseta o código recebido
            bit_count = 0; // Reseta o contador de bits
        }
    }
}

void app_main() {
    // Configuração do GPIO para o transmissor RF
    gpio_config_t tx_io_conf = {
        .pin_bit_mask = (1ULL << RF_TX_PIN), // Define o pino do transmissor
        .mode = GPIO_MODE_OUTPUT, // Define o modo do pino como saída
        .pull_up_en = GPIO_PULLUP_DISABLE, // Desabilita o pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // Desabilita o pull-down
        .intr_type = GPIO_INTR_DISABLE, // Desabilita interrupções
    };
    gpio_config(&tx_io_conf); // Configura o GPIO

    // Configuração do GPIO para o receptor RF
    gpio_config_t rx_io_conf = {
        .pin_bit_mask = (1ULL << RF_RX_PIN), // Define o pino do receptor
        .mode = GPIO_MODE_INPUT, // Define o modo do pino como entrada
        .pull_up_en = GPIO_PULLUP_DISABLE, // Desabilita o pull-up
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // Habilita o pull-down
        .intr_type = GPIO_INTR_DISABLE, // Desabilita interrupções
    };
    gpio_config(&rx_io_conf); // Configura o GPIO

    // Criar tarefas para transmissor e receptor
    xTaskCreate(rf_transmitter_task, "rf_transmitter_task", 4096, NULL, 10, NULL); // Cria a tarefa do transmissor
    xTaskCreate(rf_receiver_task, "rf_receiver_task", 4096, NULL, 10, NULL); // Cria a tarefa do receptor
}