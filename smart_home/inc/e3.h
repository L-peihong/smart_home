#ifndef E3_H
#define E3_H
#include "i2c.h"
#include "i2c_probe.h"
#include <stdint.h>

void I2c_e3_find(i2c_probe_target *list);
void I2c_e3_init(int fd, unsigned char addr);
void e3_set_position(int fd, unsigned char addr, char pos);

unsigned char e3_get_status(int fd, unsigned char addr);
unsigned char e3_get_position(int fd, unsigned char addr);

void I2c_e3_init_all(int fd, i2c_probe_target *list);
#endif 
