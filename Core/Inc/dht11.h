#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>

enum {
  DHT11_OK                = 0,
  DHT11_ERR_NO_RESPONSE   = 1,  /* DHT11 didn't pull bus low after start */
  DHT11_ERR_LOW_TIMEOUT   = 2,  /* response low phase too long */
  DHT11_ERR_HIGH_TIMEOUT  = 3,  /* response high phase too long */
  DHT11_ERR_BIT_START     = 4,  /* missing start-of-bit low pulse */
  DHT11_ERR_BIT_END       = 5,  /* bit-1 high pulse didn't end */
  DHT11_ERR_CHECKSUM      = 6,  /* checksum mismatch */
};

void dht11_init(void);
int  dht11_read(uint8_t *humidity_int, uint8_t *temperature_int);

#endif
