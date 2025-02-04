#include "esp32_rf_receiver.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"

static uint8_t RF_RX_PINO; // Pino de recepção RF
static QueueHandle_t s_esp_RF433_queue = NULL; // Declaração da fila para comunicação entre a interrupção e a tarefa

enum {
   tamanho_Proto = sizeof(proto) / sizeof(proto[0]) // Número de protocolos definidos
};

volatile unsigned long valor_Recebido = 0; // Valor recebido
volatile unsigned int comprimento_BIT_Recebido = 0; // Comprimento do bit recebido
volatile unsigned int delay_Recebido = 0; // Atraso recebido
volatile unsigned int protocolo_Recebido = 0; // Protocolo recebido
int tolerancia_Recebida = 60; // Tolerância de recepção
const unsigned limite_separacao = 4300; // Limite de separação entre códigos recebidos

unsigned int tempos[RCSWITCH_MAX_CHANGES]; // Armazena os tempos de mudança de sinal

/* Função auxiliar para calcular a diferença entre dois valores */
static inline unsigned int diferenca(int A, int B) {
  return abs(A - B);
}

/* Função para receber o protocolo */
bool Receber_protocolo(const int p, unsigned int changeCount) {
    const Protocol pro = proto[p-1]; // Seleciona o protocolo

    unsigned long codigo = 0;
    // Assume que o comprimento do pulso mais longo é o capturado em tempos[0]
    const unsigned int syncLengthInPulses =  ((pro.syncFactor.low) > (pro.syncFactor.high)) ? (pro.syncFactor.low) : (pro.syncFactor.high);
    const unsigned int delay = tempos[0] / syncLengthInPulses;
    const unsigned int tolerancia_Delay = delay * tolerancia_Recebida / 100;

    /* Para protocolos que começam com sinal baixo */
    const unsigned int firstDataTiming = (pro.invertedSignal) ? (2) : (1);

    for (unsigned int i = firstDataTiming; i < changeCount - 1; i += 2) {
        codigo <<= 1;
        if (diferenca(tempos[i], delay * pro.zero.high) < tolerancia_Delay &&
            diferenca(tempos[i + 1], delay * pro.zero.low) < tolerancia_Delay) {
            // zero
        } else if (diferenca(tempos[i], delay * pro.one.high) < tolerancia_Delay &&
                   diferenca(tempos[i + 1], delay * pro.one.low) < tolerancia_Delay) {
            // one
            codigo |= 1;
        } else {
            // Falhou
            return false;
        }
    }

    if (changeCount > 7) {    // Ignora transmissões muito curtas
        valor_Recebido = codigo;
        comprimento_BIT_Recebido = (changeCount - 1) / 2;
        delay_Recebido = delay;
        protocolo_Recebido = p;
        return true;
    }

    return false;
}

bool available() {
    return valor_Recebido != 0; // Verifica se há um valor recebido disponível
}

void resetAvailable() {
    valor_Recebido = 0; // Reseta o valor recebido
}

unsigned long getReceivedValue() {
    return valor_Recebido; // Retorna o valor recebido
}

unsigned int getReceivedBitlength() {
    return comprimento_BIT_Recebido; // Retorna o comprimento do bit recebido
}

unsigned int getReceivedDelay() {
    return delay_Recebido; // Retorna o atraso recebido
}

unsigned int getReceivedProtocol() {
    return protocolo_Recebido; // Retorna o protocolo recebido
}

unsigned int* getReceivedRawdata() {
  return tempos; // Retorna os dados brutos recebidos
}

/* Manipulador de interrupção de dados */
void data_interrupt_handler(void* arg)
{
    static unsigned int changeCount = 0;
    static unsigned long ultimo_tempo = 0;
    static unsigned int repeatCount = 0;

    const long tempo = esp_timer_get_time(); // Obtém o tempo atual
    const unsigned int duracao = tempo - ultimo_tempo; // Calcula a duração desde a última mudança de sinal

    if (duracao > limite_separacao) {
        // Um longo período sem mudança de sinal ocorreu. Isso pode ser o intervalo entre duas transmissões.
        if (diferenca(duracao, tempos[0]) < 200) {
          // Este longo sinal é próximo em comprimento ao sinal longo que iniciou os tempos registrados anteriormente
          repeatCount++;
          if (repeatCount == 2) {
            for(uint8_t i = 1; i <= tamanho_Proto; i++) {
              if (Receber_protocolo(i, changeCount)) {
                // Recepção bem-sucedida para o protocolo i
              uint8_t id_Protocolo = (uint8_t)i;
              xQueueSendFromISR(s_esp_RF433_queue, &id_Protocolo, NULL); // Envia o número do protocolo para a fila
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

      tempos[changeCount++] = duracao; // Armazena a duração no array de tempos
      ultimo_tempo = tempo; // Atualiza o último tempo
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
      ESP_LOGW(TAG, "Received %lu / %d bit / Protocol: %d.\n", getReceivedValue(), getReceivedBitlength(), prot_num); // Log dos dados recebidos        
    }
  }
}

/* Função principal do aplicativo */

void init_rf_receiver(uint8_t rx_pin) {
  RF_RX_PINO = rx_pin;
  if (s_esp_RF433_queue == NULL) {
      s_esp_RF433_queue = xQueueCreate(1, sizeof(uint8_t));
      if (s_esp_RF433_queue != NULL) {
        gpio_config_t data_pin_config = {
          .intr_type = GPIO_INTR_ANYEDGE,
          .mode = GPIO_MODE_INPUT,
          .pin_bit_mask = (1ULL << RF_RX_PINO),
          .pull_up_en = GPIO_PULLUP_DISABLE,
          .pull_down_en = GPIO_PULLDOWN_DISABLE
        };

        printf("GPIO configurado: RF_RX_PINO=%d.\n", RF_RX_PINO);
        printf("Aguardando inicialização...\n");
        vTaskDelay(pdMS_TO_TICKS(10)); 


        gpio_config(&data_pin_config);

        gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
        gpio_isr_handler_add(RF_RX_PINO, data_interrupt_handler, NULL);
        xTaskCreate(&receiver_rf433, "receiver_rf433", 2048, NULL, 5, NULL);
    }
  }
}