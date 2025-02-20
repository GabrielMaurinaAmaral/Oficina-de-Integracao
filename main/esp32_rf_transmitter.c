#include "esp32_rf_transmitter.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

static uint8_t RF_TX_PINO;

void sendRFSignal(unsigned long data, int bitLength, const Protocol *protocol)
{
    for (int i = 0; i < bitLength; i++)
    {
        if (data & (1 << (bitLength - 1 - i)))
        {
            // enviando '1' bit no sinal
            gpio_set_level(RF_TX_PINO, 1); // BAIXO
            esp_rom_delay_us(protocol->pulseLength * protocol->one.high);
            gpio_set_level(RF_TX_PINO, 0); // ALTO
            esp_rom_delay_us(protocol->pulseLength * protocol->one.low);
        }
        else
        {
            // enviando'0' bit no sinal
            gpio_set_level(RF_TX_PINO, 1); // BAIXO
            esp_rom_delay_us(protocol->pulseLength * protocol->zero.high);
            gpio_set_level(RF_TX_PINO, 0); // ALTO
            esp_rom_delay_us(protocol->pulseLength * protocol->zero.low);
        }
    }
    gpio_set_level(RF_TX_PINO, 1); // BAIXO
    esp_rom_delay_us(protocol->pulseLength * protocol->syncFactor.high);
    gpio_set_level(RF_TX_PINO, 0); // ALTO
    esp_rom_delay_us(protocol->pulseLength * protocol->syncFactor.low);

}

void init_rf_transmitter(uint8_t tx_pin)
{
    RF_TX_PINO = tx_pin;

    // Configura o pino de saída de dados
    gpio_config_t data_pin_config = {
        .intr_type = GPIO_INTR_DISABLE, // Desabilita a interrupção
        .mode = GPIO_MODE_OUTPUT,       // Define o modo do pino como saída
        .pin_bit_mask = (1ULL << RF_TX_PINO), // Define o pino a ser configurado
        .pull_up_en = GPIO_PULLUP_DISABLE,    // Desabilita o resistor de pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE // Desabilita o resistor de pull-down
    };

    printf("GPIO configurado: RF_TX_PINO=%d.\n", RF_TX_PINO);
    printf("Aguardando inicialização...\n");
    vTaskDelay(pdMS_TO_TICKS(100));

    gpio_config(&data_pin_config); // Aplica a configuração do pino
}