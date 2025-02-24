#include "esp32_rf_transmitter.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

static uint8_t RF_TX_PINO;
// Função auxiliar para enviar um bit
static void sendBit(int level, int highTime, int lowTime) {
    gpio_set_level(RF_TX_PINO, level);
    esp_rom_delay_us(highTime);
    gpio_set_level(RF_TX_PINO, !level);
    esp_rom_delay_us(lowTime);
}

void sendRFSignal(unsigned long data, int bitLength, const Protocol *protocol)
{
    // vereficar se não vai mandar o sinal invertido
    int level = protocol->invertedSignal ? 1 : 0;
    // Enviando o fator de sincronização
    sendBit(level, protocol->pulseLength * protocol->syncFactor.high, protocol->pulseLength * protocol->syncFactor.low);
    
    for (int i = 0; i < bitLength; i++)
    {
        if (data & (1 << (bitLength - 1 - i)))
        {
            // Enviando '1' bit no sinal
            sendBit(level, protocol->pulseLength * protocol->one.high, protocol->pulseLength * protocol->one.low);
        }
        else
        {
            // Enviando '0' bit no sinal
            sendBit(level, protocol->pulseLength * protocol->zero.high, protocol->pulseLength * protocol->zero.low);
        }
    }
}


void init_rf_transmitter(uint8_t tx_pin)
{
    RF_TX_PINO = tx_pin;

    // Configura o pino de saída de dados
    gpio_config_t data_pin_config = {
        .intr_type = GPIO_INTR_DISABLE,       // Desabilita a interrupção
        .mode = GPIO_MODE_OUTPUT,             // Define o modo do pino como saída
        .pin_bit_mask = (1ULL << RF_TX_PINO), // Define o pino a ser configurado
        .pull_up_en = GPIO_PULLUP_DISABLE,    // Desabilita o resistor de pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE // Desabilita o resistor de pull-down
    };

    gpio_config(&data_pin_config); // Aplica a configuração do pino
    printf("GPIO configurado: RF_TX_PINO=%d.\n", RF_TX_PINO);
    printf("Aguardando inicialização...\n");

    vTaskDelay(pdMS_TO_TICKS(100));
}