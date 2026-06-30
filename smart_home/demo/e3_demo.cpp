#include <iostream>
#include "e3.h"

extern int motor_idx;

int main(int argc, char **argv)
{
	i2c_probe_target list[I2C_PROBE_LIST_LEN] = {};
    	memcpy(list, I2C_PROBE_LIST, sizeof(list));
    	i2c_probe(list, I2C_PROBE_LIST_LEN);
    	printf("detect finish\n");
    	
    	I2c_e3_find(list);
    	
    	int fd = I2c_open(list[motor_idx].path);
	I2c_e3_init_all(fd,list);

    	while(1){
       	sleep(5);
       	e3_set_position(fd, list[motor_idx].detected_addrs[0], 100);
       	if(list[motor_idx].detected_count>1){
       		e3_set_position(fd, list[motor_idx].detected_addrs[1], 60);
       	}
        	sleep(5);
        	e3_set_position(fd, list[motor_idx].detected_addrs[0], 0);
        	if(list[motor_idx].detected_count>1){
       		e3_set_position(fd, list[motor_idx].detected_addrs[1], 0);
       	}
  	}
    	return 0;
}
