#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

// Declaração da fila para comunicação entre a interrupção e a tarefa
extern QueueHandle_t BOTAO_queue;

// Função de inicialização do botão
void init_button(uint8_t button_pin);

// Manipulador de interrupção do botão
void IRAM_ATTR button_isr_handler(void* arg);

// Tarefa do botão
void button_task(void* arg);

#endif // BUTTON_H