/**
 * Inspired by RC-Switch library (https://github.com/sui77/rc-switch)
 * Mac Wyznawca make some changes. Non-blocking loop with Queue and ESP-SDK native function esp_timer_get_time() for millisecond.
 */

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

#define ADVANCED_OUTPUT 1
#define TAG "RF433"

static xQueueHandle s_esp_RF433_queue = NULL; // Declaração da fila para comunicação entre a interrupção e a tarefa

// Definição dos protocolos de comunicação RF
static const Protocol proto[] = {
  { 350, {  1, 31 }, {  1,  3 }, {  3,  1 }, false },    // protocol 1
  { 650, {  1, 10 }, {  1,  2 }, {  2,  1 }, false },    // protocol 2
  { 100, { 30, 71 }, {  4, 11 }, {  9,  6 }, false },    // protocol 3
  { 380, {  1,  6 }, {  1,  3 }, {  3,  1 }, false },    // protocol 4
  { 500, {  6, 14 }, {  1,  2 }, {  2,  1 }, false },    // protocol 5
  { 450, { 23,  1 }, {  1,  2 }, {  2,  1 }, true }      // protocol 6 (HT6P20B)
};

enum {
   numProto = sizeof(proto) / sizeof(proto[0]) // Número de protocolos definidos
};

volatile unsigned long nReceivedValue = 0; // Valor recebido
volatile unsigned int nReceivedBitlength = 0; // Comprimento do bit recebido
volatile unsigned int nReceivedDelay = 0; // Atraso recebido
volatile unsigned int nReceivedProtocol = 0; // Protocolo recebido
int nReceiveTolerance = 60; // Tolerância de recepção
const unsigned nSeparationLimit = 4300; // Limite de separação entre códigos recebidos

unsigned int timings[RCSWITCH_MAX_CHANGES]; // Armazena os tempos de mudança de sinal

/* Função auxiliar para calcular a diferença entre dois valores */
static inline unsigned int diff(int A, int B) {
  return abs(A - B);
}

/* Função para receber o protocolo */
bool receiveProtocol(const int p, unsigned int changeCount) {
    const Protocol pro = proto[p-1]; // Seleciona o protocolo

    unsigned long code = 0;
    // Assume que o comprimento do pulso mais longo é o capturado em timings[0]
    const unsigned int syncLengthInPulses =  ((pro.syncFactor.low) > (pro.syncFactor.high)) ? (pro.syncFactor.low) : (pro.syncFactor.high);
    const unsigned int delay = timings[0] / syncLengthInPulses;
    const unsigned int delayTolerance = delay * nReceiveTolerance / 100;

    /* Para protocolos que começam com sinal baixo */
    const unsigned int firstDataTiming = (pro.invertedSignal) ? (2) : (1);

    for (unsigned int i = firstDataTiming; i < changeCount - 1; i += 2) {
        code <<= 1;
        if (diff(timings[i], delay * pro.zero.high) < delayTolerance &&
            diff(timings[i + 1], delay * pro.zero.low) < delayTolerance) {
            // zero
        } else if (diff(timings[i], delay * pro.one.high) < delayTolerance &&
                   diff(timings[i + 1], delay * pro.one.low) < delayTolerance) {
            // one
            code |= 1;
        } else {
            // Falhou
            return false;
        }
    }

    if (changeCount > 7) {    // Ignora transmissões muito curtas
        nReceivedValue = code;
        nReceivedBitlength = (changeCount - 1) / 2;
        nReceivedDelay = delay;
        nReceivedProtocol = p;
        return true;
    }

    return false;
}

// -- Wrappers para variáveis que devem ser modificadas apenas internamente

bool available() {
    return nReceivedValue != 0; // Verifica se há um valor recebido disponível
}

void resetAvailable() {
    nReceivedValue = 0; // Reseta o valor recebido
}

unsigned long getReceivedValue() {
    return nReceivedValue; // Retorna o valor recebido
}

unsigned int getReceivedBitlength() {
    return nReceivedBitlength; // Retorna o comprimento do bit recebido
}

unsigned int getReceivedDelay() {
    return nReceivedDelay; // Retorna o atraso recebido
}

unsigned int getReceivedProtocol() {
    return nReceivedProtocol; // Retorna o protocolo recebido
}

unsigned int* getReceivedRawdata() {
  return timings; // Retorna os dados brutos recebidos
}

// ---

/* Manipulador de interrupção de dados */
void data_interrupt_handler(void* arg)
{
    static unsigned int changeCount = 0;
    static unsigned long lastTime = 0;
    static unsigned int repeatCount = 0;

    const long time = esp_timer_get_time(); // Obtém o tempo atual
    const unsigned int duration = time - lastTime; // Calcula a duração desde a última mudança de sinal

    if (duration > nSeparationLimit) {
        // Um longo período sem mudança de sinal ocorreu. Isso pode ser o intervalo entre duas transmissões.
        if (diff(duration, timings[0]) < 200) {
          // Este longo sinal é próximo em comprimento ao sinal longo que iniciou os timings registrados anteriormente
          repeatCount++;
          if (repeatCount == 2) {
            for(uint8_t i = 1; i <= numProto; i++) {
              if (receiveProtocol(i, changeCount)) {
                // Recepção bem-sucedida para o protocolo i
              uint8_t protocol_num = (uint8_t)i;
              xQueueSendFromISR(s_esp_RF433_queue, &protocol_num, NULL); // Envia o número do protocolo para a fila
                break;
              }
            }
            repeatCount = 0;
          }
        }
        changeCount = 0;
      }
      // Detecta overflow
      if (changeCount >= RCSWITCH_MAX_CHANGES) {
        changeCount = 0;
        repeatCount = 0;
      }

      timings[changeCount++] = duration; // Armazena a duração no array de timings
      lastTime = time; // Atualiza o último tempo
}

/* Tarefa para processar os dados recebidos */
void receiver_rf433(void* pvParameter)
{
  uint8_t prot_num = 0;
  while(1)
  {
    if (xQueueReceive(s_esp_RF433_queue, &prot_num, portMAX_DELAY) == pdFALSE) {
      ESP_LOGE(TAG, "RF433 interrurpt fail"); // Log de erro se a recepção da fila falhar
    }
    else {
          ESP_LOGW(TAG, "Received %lu / %dbit Protocol: %d.\n", getReceivedValue(), getReceivedBitlength(), prot_num); // Log dos dados recebidos
            resetAvailable(); // Reseta a disponibilidade dos dados
        }
  }
}

/* Função principal do aplicativo */
void app_main()
{
  if (s_esp_RF433_queue == NULL) {
      s_esp_RF433_queue = xQueueCreate(1, sizeof(uint8_t)); // Cria a fila para comunicação entre a interrupção e a tarefa
      if (s_esp_RF433_queue != NULL) {
        // Configura o pino de entrada de dados
        gpio_config_t data_pin_config = {
          .intr_type = GPIO_INTR_ANYEDGE, // Configura a interrupção para qualquer borda (subida ou descida)
          .mode = GPIO_MODE_INPUT, // Define o modo do pino como entrada
          .pin_bit_mask = GPIO_SEL_22, // Seleciona o pino GPIO 22
          .pull_up_en = GPIO_PULLUP_DISABLE, // Desabilita o resistor de pull-up
          .pull_down_en = GPIO_PULLDOWN_DISABLE // Desabilita o resistor de pull-down
        };

        gpio_config(&data_pin_config); // Aplica a configuração do pino

        // Anexa o manipulador de interrupção
        gpio_install_isr_service(ESP_INTR_FLAG_EDGE); // Instala o serviço de interrupção
        gpio_isr_handler_add(GPIO_NUM_22, data_interrupt_handler, NULL); // Adiciona o manipulador de interrupção para o pino GPIO 22
        xTaskCreate(&receiver_rf433, "receiver_rf433", 2048, NULL, 3, NULL); // Cria a tarefa para processar os dados recebidos
    }
  }
}