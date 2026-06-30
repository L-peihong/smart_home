#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <iostream>
#include "s7.h"

// 是否检测到人
unsigned char has_person = 0;
extern int human_idx;
int main(int argc, char **argv)
{
        i2c_probe_target list[I2C_PROBE_LIST_LEN] = {};
    	memcpy(list, I2C_PROBE_LIST, sizeof(list));
    	i2c_probe(list, I2C_PROBE_LIST_LEN);
    	printf("detect finish\n");
    	
    	I2c_s7_find(list);
    	
    	int fd = I2c_open(list[human_idx].path);
	I2c_s7_init_all(fd, list);
	
    	while (1){
    		I2c_human_detected_all(fd, list);
        	sleep(1);
    	}

    	return 0;
}
