#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "e2.h"
#include <assert.h>
#include <iostream>

extern int fan_idx;
uint8_t speed[4] = {0, 20 ,50 ,90};

uint8_t next_key(uint8_t origin){
    for(int i=0;i<4;++i){
        if(speed[i]==origin)
        return speed[(i+1)%4];
    }
    return -1;
}

int main(int argc, char **argv)
{
	i2c_probe_target list[I2C_PROBE_LIST_LEN] = {};
    	memcpy(list, I2C_PROBE_LIST, sizeof(list));
    	i2c_probe(list, I2C_PROBE_LIST_LEN);
    	printf("detect finish\n");
    	
    	I2c_e2_find(list);
    	
    	int fd = I2c_open(list[fan_idx].path);
	I2c_e2_init_all(fd,list);
    	
    	uint8_t key;
    	key = 0;
    	while (1){
        	 key = next_key(key);
       	 e2_speed_control_all(fd, list, key);
       	 printf("key is %d\n", key);
       	 sleep(3);
    	}

    	return 0;
}
