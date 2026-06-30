#ifndef S7_H
#define S7_H

#include "i2c_probe.h"
#include "i2c.h"

#define PCA9557_POLARITY_INVERSION_REG  0x02
#define PCA9557_CONFIG_REG              0x03
 
#define POLARITY_INVERSION_DEFAULT      0xF0
#define CONFIG_DEFAULT                  0xFF

void I2c_s7_find(i2c_probe_target *list);
void I2c_s7_init(int fd, unsigned char addr);
void I2c_s7_init_all(int fd, i2c_probe_target *list);
unsigned char I2c_human_detected(int fd, unsigned char addr);
void I2c_human_detected_all(int fd, i2c_probe_target *list);
#endif
