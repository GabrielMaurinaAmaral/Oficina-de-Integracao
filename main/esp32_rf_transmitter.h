#ifndef ESP32_RF_TRANSMITTER_H
#define ESP32_RF_TRANSMITTER_H

#include "esp32_rf_protocol.h"
#include <stdint.h>
#include <stdbool.h>

void init_rf_transmitter(uint8_t tx_pin);
void sendRFSignal(unsigned long data, int bitLength, const Protocol *protocol);

#endif /* ESP32_RF_TRANSMITTER_H */