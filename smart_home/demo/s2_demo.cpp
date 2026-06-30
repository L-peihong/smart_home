#include "i2c_probe.h"
#include "s2.h"

float ss_value;

extern int bright_idx;
int main(int argc, char **argv)
{
    	i2c_probe_target list[I2C_PROBE_LIST_LEN] = {};
    	memcpy(list, I2C_PROBE_LIST, sizeof(list));
    	i2c_probe(list, I2C_PROBE_LIST_LEN);
    	printf("detect finish\n");
    	
    	I2c_s2_find(list);
    	
    	int fd = I2c_open(list[bright_idx].path);
	I2c_s2_init_all(fd,list);

    	while(1){
		ss_value = s2_read_bh1750_value(fd, list[bright_idx].detected_addrs[0]);
		printf("sunshine = %f\n",ss_value); 
		if(list[bright_idx].detected_count>1){
       		ss_value = s2_read_bh1750_value(fd, list[bright_idx].detected_addrs[1]);
       		printf("sunshine = %f\n",ss_value);
       	}
   		sleep(3);
    	}
    	return 0;
}
