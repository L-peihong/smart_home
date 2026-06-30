#ifndef S2_H
#define S2_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "i2c.h"
#include "i2c_probe.h"


       
void I2c_s2_find(i2c_probe_target *list);

float s2_read_bh1750_value(int fd,unsigned char addr);
void I2c_s2_init(int fd, unsigned char addr);
void I2c_s2_init_all(int fd, i2c_probe_target *list);
#endif
