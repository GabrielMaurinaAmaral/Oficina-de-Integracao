#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define RF_RX_PIN 16
#define MAX_SAMPLES 1000 // Número máximo de amostras

int64_t timestamps[MAX_SAMPLES]; // Array para armazenar timestamps
int signal_states[MAX_SAMPLES];  // Array para armazenar estados do sinal
int sample_index = 0;            // Índice da amostra atual

void rf_receiver_task(void *arg)
{
    while (1)
    {
        // Espera até o pino mudar de estado
        int current_state = gpio_get_level(RF_RX_PIN);
        int64_t current_time = esp_timer_get_time();

        // Armazena os dados no buffer
        if (sample_index < MAX_SAMPLES)
        {
            timestamps[sample_index] = current_time;
            signal_states[sample_index] = current_state;
            sample_index++;
        }
        else
        {
            printf("Buffer cheio. Finalize a coleta.\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // Aguarda um tempo pequeno para evitar sobrecarga
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void export_data()
{
    printf("Exportando dados...\n");
    for (int i = 0; i < sample_index; i++)
    {
        printf("%lld, %d\n", timestamps[i], signal_states[i]);
    }
}

void app_main(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RF_RX_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    xTaskCreate(rf_receiver_task, "rf_receiver_task", 2048, NULL, 10, NULL);

    // Após um tempo de coleta, exporta os dados
    vTaskDelay(pdMS_TO_TICKS(5000)); // Aguarde 5 segundos para coleta de dados
    export_data();
}
