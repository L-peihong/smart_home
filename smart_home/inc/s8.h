#ifndef S8_H
#define S8_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "i2c.h"
#include "i2c_probe.h"

typedef struct
{
	  float temperature;
	  float humidity;
}s8_para;	

void I2c_s8_find(i2c_probe_target *list);
s8_para read_sht3x(int fd,unsigned char addr);
void read_sht3x_all(int fd, i2c_probe_target *list);

#endif
