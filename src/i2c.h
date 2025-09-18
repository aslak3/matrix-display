#include <stdint.h>
#include <stdlib.h>

void setup_i2c(void);
int i2c_read(uint8_t address, uint8_t *buffer, size_t len, bool nostop);
int i2c_write(uint8_t address, const uint8_t *buffer, size_t len, bool nostop);
