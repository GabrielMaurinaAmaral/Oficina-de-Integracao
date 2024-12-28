#include <stdio.h> // Inclui a biblioteca padrão de entrada e saída
#include "freertos/FreeRTOS.h" // Inclui a biblioteca FreeRTOS
#include "freertos/task.h" // Inclui a biblioteca de tarefas do FreeRTOS
#include "driver/gpio.h" // Inclui a biblioteca de controle de GPIO
#include "esp_timer.h" // Inclui a biblioteca de temporizadores do ESP

#define RF_RX_PIN 16      // Define o pino de recepção RF
#define SYNC_PULSE 10000 // Define a duração do pulso de sincronização em microssegundos
#define BIT_THRESHOLD 500 // Define o limiar para distinguir entre 0 e 1

void rf_receiver_task(void *arg); // Declaração da função da tarefa de recepção RF

void rf_receiver_task(void *arg)
{
    uint32_t received_code = 0; // Variável para armazenar o código recebido
    int bit_count = 0;          // Contador de bits recebidos

    while (1) // Loop infinito para a tarefa
    {
        // Espera até o pino mudar de estado (de baixo para alto)
        while (gpio_get_level(RF_RX_PIN) == 0)
            ;

        // Medir o tempo do pulso
        int64_t start_time = esp_timer_get_time(); // Captura o tempo de início do pulso
        while (gpio_get_level(RF_RX_PIN) == 1)
            ;                                                 // Espera até o pino mudar de estado (de alto para baixo)
        int64_t duration = esp_timer_get_time() - start_time; // Calcula a duração do pulso

        if (duration > SYNC_PULSE) // Verifica se a duração do pulso é maior que o pulso de sincronização
        {
            // Pulso de sincronização detectado
            if (bit_count == 32) // Verifica se 32 bits foram recebidos
            {
                printf("Código recebido: 0x%08lX\n", received_code); // Imprime o código recebido
                // Envia o código recebido via serial
                printf("DATA: %08lX\n", received_code);
            }

            else if (bit_count > 0) // Verifica se algum bit foi recebido, mas menos de 32 bits
            {
                printf("Erro: Código incompleto (%d bits recebidos).\n", bit_count); // Imprime erro se o código estiver incompleto
            }
            
            received_code = 0; // Reseta o código recebido
            bit_count = 0;     // Reseta o contador de bits
        }
        
        else // Se a duração do pulso não for de sincronização
        {
            // Adiciona o bit recebido ao código
            received_code = (received_code << 1) | (duration > BIT_THRESHOLD); // Desloca o código recebido e adiciona o novo bit
            bit_count++; // Incrementa o contador de bits
        }
    }
}

void app_main(void)
{
    // Configura o pino de recepção RF
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RF_RX_PIN), // Define o pino a ser configurado
        .mode = GPIO_MODE_INPUT, // Define o modo do pino como entrada
        .pull_up_en = GPIO_PULLUP_DISABLE, // Desabilita o resistor de pull-up
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // Habilita o resistor de pull-down
        .intr_type = GPIO_INTR_DISABLE}; // Desabilita interrupções no pino
    gpio_config(&io_conf); // Aplica a configuração do pino

    // Cria a tarefa do receptor RF
    xTaskCreate(rf_receiver_task, "rf_receiver_task", 2048, NULL, 10, NULL); // Cria a tarefa de recepção RF com prioridade 10 e stack de 2048 bytes
}