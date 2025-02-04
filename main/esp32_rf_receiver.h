#ifndef ESP32_RF_RECEIVER_H
#define ESP32_RF_RECEIVER_H

#include "esp32_rf_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void init_rf_receiver(uint8_t rx_pin);
bool available();
void resetAvailable();
unsigned long getReceivedValue();
unsigned int getReceivedBitlength();
unsigned int getReceivedDelay();
unsigned int getReceivedProtocol();
unsigned int* getReceivedRawdata();


#endif /* ESP32_RF_RECEIVER_H */