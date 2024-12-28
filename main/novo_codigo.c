#include "esp32_rf_receiver.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/queue.h>
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "include/output.h"
#include "esp_timer.h"

#define RF_RX_PINO 13 // Pino de recepção RF (D13)
#define ADVANCED_OUTPUT 1
#define TAG "RF433"

static QueueHandle_t s_esp_RF433_queue = NULL; // Declaração da fila para comunicação entre a interrupção e a tarefa

const unsigned limite_separacao = 4300;                    // Limite de separação entre códigos recebidos
unsigned int tempo_mudanca_sinal[RCSWITCH_MAX_CHANGES]; // Armazena os tempos de mudança de sinal

int tolerancia_recebendo = 60; // Tolerância de recepção

volatile unsigned long valor_Recebido = 0; // Valor recebido
volatile unsigned int comprimento_Bit_Recebido = 0; // Comprimento do bit recebido
volatile unsigned int atraso_Recebido = 0; // Atraso recebido
volatile unsigned int protocolo_Recebido = 0; // Protocolo recebido

static const Protocol proto[] = {
    {450, {23, 1}, {1, 2}, {2, 1}, true},     // protocol 6 (HT6P20B)
    {350, {1, 31}, {1, 3}, {3, 1}, false},   // protocol 1
    {650, {1, 10}, {1, 2}, {2, 1}, false},   // protocol 2
    {100, {30, 71}, {4, 11}, {9, 6}, false}, // protocol 3
    {380, {1, 6}, {1, 3}, {3, 1}, false},    // protocol 4
    {500, {6, 14}, {1, 2}, {2, 1}, false}   // protocol 5
};

enum
{
    tamanho_Protocolo = sizeof(proto) / sizeof(proto[0]) // Número de protocolos definidos
};

void resetAvailable() {
    valor_Recebido = 0; // Reseta o valor recebido
}

unsigned long getReceivedValue() {
    return valor_Recebido; // Retorna o valor recebido
}

unsigned int getReceivedBitlength() {
    return comprimento_Bit_Recebido; // Retorna o comprimento do bit recebido
}

static inline unsigned int diferensa(int A, int B) {
  return abs(A - B);
}

bool receber_protocolo(const int protocolo, unsigned int contagem_estados)
{
    const Protocol pro = proto[protocolo - 1];

    unsigned long codigo = 0;
    // Assume que o comprimento do pulso mais longo é o capturado em tempo_mudanca_sinal[0]
    const unsigned int sincronizar_Comprimento_Pulsos =  ((pro.syncFactor.low) > (pro.syncFactor.high)) ? (pro.syncFactor.low) : (pro.syncFactor.high);
    const unsigned int delay = tempo_mudanca_sinal[0] / sincronizar_Comprimento_Pulsos;
    const unsigned int tolerancia_delay = delay * tolerancia_recebendo / 100;

    /* Para protocolos que começam com sinal baixo */
    const unsigned int primerio_dado_tempo = (pro.invertedSignal) ? (2) : (1);

    for(unsigned int i = primerio_dado_tempo; i < contagem_estados; i+=2)
    {
        codigo <<= 1;
        if (diferensa(tempo_mudanca_sinal[i], delay * pro.zero.high) < tolerancia_delay &&
            diferensa(tempo_mudanca_sinal[i+1], delay * pro.zero.low) < tolerancia_delay) 
        {
            // zero
            codigo |= 0;
        }
        else if (diferensa(tempo_mudanca_sinal[i], delay * pro.one.high) < tolerancia_delay &&
                diferensa(tempo_mudanca_sinal[i + 1], delay * pro.one.low) < tolerancia_delay) 
        {
            // um
            codigo |= 1;
        }
        else 
        {
            return false;
        }
    }
    // Ignora transmissões muito curtas
    if(contagem_estados > 10)
    {
        valor_Recebido = codigo;
        comprimento_Bit_Recebido = (contagem_estados - 1) / 2;
        atraso_Recebido = delay;
        protocolo_Recebido = protocolo;
        return true;   
    }
    return false;
}

void manipula_interrupcao(void *arg)
{
    static unsigned int contagem_estados = 0;
    static unsigned long ultimo_tempo = 0;
    static unsigned int repetir_contagem = 0;

    const long tempo = esp_timer_get_time();           // Obtém o tempo atual
    const unsigned int duracao = tempo - ultimo_tempo; // Calcula a duração desde a última mudança de sinal

    if (duracao > limite_separacao)
    {
        // Um longo período sem mudança de sinal ocorreu. Isso pode ser o intervalo entre duas transmissões.
        if (diferensa(duracao, tempo_mudanca_sinal[0]) < 200)
        {
            // Este longo sinal é próximo em comprimento ao sinal longo que iniciou os timings registrados anteriormente
            repetir_contagem++;
            if (repetir_contagem == 2)
            {
                for (uint8_t i = 0; i < tamanho_Protocolo; i++)
                {
                    if (receber_protocolo(i, contagem_estados))
                    {
                        // Recepção bem-sucedida para o protocolo i
                        uint8_t protocolo_numero = (uint8_t)i;
                        xQueueSendFromISR(s_esp_RF433_queue, &protocolo_numero, NULL); // Envia o número do protocolo para a fila
                        break;
                    }
                }
                repetir_contagem = 0;
            }
        }
        contagem_estados = 0;
    }
    // Detecta overflow
    if (contagem_estados >= RCSWITCH_MAX_CHANGES)
    {
        contagem_estados = 0;
        repetir_contagem = 0;
    }

    tempo_mudanca_sinal[contagem_estados++] = duracao; // Armazena a duração no array de timings
    ultimo_tempo = tempo;                              // Armazena o tempo atual para a próxima mudança de sinal
}

void rf_receptor_task(void *pvParameters)
{
    uint8_t numero_Protocolo = 0;
    while (1)
    {
        if (xQueueReceive(s_esp_RF433_queue, &numero_Protocolo, portMAX_DELAY) == pdFALSE)
        {
            ESP_LOGE(TAG, "RF433 interrurpt fail"); // Log de erro se a recepção da fila falhar
        }
        else
        {          
            ESP_LOGW(TAG, "Received %lu / %dbit Protocol: %d.\n", getReceivedValue(), getReceivedBitlength(), numero_Protocolo); // Log dos dados recebidos
            resetAvailable(); // Reseta a disponibilidade dos dados
        }
    }
}


void app_main(void)
{

    if (s_esp_RF433_queue == NULL)
    {
        s_esp_RF433_queue = xQueueCreate(1, sizeof(uint8_t)); // Cria a fila para comunicação entre a interrupção e a tarefa
        if (s_esp_RF433_queue != NULL)
        {
            gpio_config_t io_conf_rx = {
                .pin_bit_mask = (1ULL << RF_RX_PINO),
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_ANYEDGE
            };
            gpio_config(&io_conf_rx);

            printf("GPIO configurado: RF_RX_PINO=%d.\n", RF_RX_PINO);
            printf("Aguardando inicialização...\n");

            vTaskDelay(pdMS_TO_TICKS(1000)); // Tempo para inicialização

            // Anexa o manipulador de interrupção
            gpio_install_isr_service(ESP_INTR_FLAG_EDGE);                 // Instala o serviço de interrupção
            gpio_isr_handler_add(RF_RX_PINO, manipula_interrupcao, NULL); // Adiciona o manipulador de interrupção para o pino GPIO 22

            // Cria a tarefa de recepção de RF
            xTaskCreate(&rf_receptor_task, "rf_receptor_task", 2048, NULL, 3, NULL);
        }
    }
}