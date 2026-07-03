#ifndef LORA_H
#define LORA_H

#include <stdint.h>

typedef struct {
  uint8_t role;
  uint32_t sequence;
  uint8_t temperature;
  uint8_t humidity;
  int error;
} lora_sample_t;

void lora_init(void);
void lora_send_sample(uint8_t role, uint32_t sequence, uint8_t temperature,
                      uint8_t humidity, int error);
int lora_poll_sample(lora_sample_t *sample);

#endif
