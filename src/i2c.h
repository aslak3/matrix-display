#include <stdint.h>
#include <stdlib.h>

void setup_i2c(void);
bool i2c_read(uint8_t address, uint8_t *buffer, size_t len);
bool i2c_write(uint8_t address, const uint8_t *buffer, size_t len);
bool i2c_write_read(uint8_t address, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len);
