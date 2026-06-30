#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "s8.h"
#include <assert.h>
#include <iostream>


s8_para s8_value;
extern int sht_idx;
int main(int argc, char **argv)
{
	i2c_probe_target list[I2C_PROBE_LIST_LEN] = {};
    	memcpy(list, I2C_PROBE_LIST, sizeof(list));
    	i2c_probe(list, I2C_PROBE_LIST_LEN);
    	printf("detect finish\n");
    	
    	I2c_s8_find(list);
    	
    	int fd = I2c_open(list[sht_idx].path);
    	while(1){
    		read_sht3x_all(fd, list);
   		sleep(2);
	}
    	return 0;
}
