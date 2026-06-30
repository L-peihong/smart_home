#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <iostream>

#include "s1.h"
#include "i2c.h"

extern int key_idx;
extern s1_key_process *s1_key;
int main(int argc, char **argv)
{	
	i2c_probe_target list[I2C_PROBE_LIST_LEN] = {};
    	memcpy(list, I2C_PROBE_LIST, sizeof(list));
    	i2c_probe(list, I2C_PROBE_LIST_LEN);
    	printf("detect finish\n");
    	
    	I2c_s1_find(list);
    	
    	int fd = I2c_open(list[key_idx].path);
	I2c_s1_init_all(fd,list);
	while(1){
		for(int i=0;i<list[key_idx].detected_count;i++){
			process_keys(fd,list[key_idx].detected_addrs[i],list);
			for(int j=0;j<13;j++){
				if(s1_key[i].SW_long[j]){
					s1_key[i].SW_long[j]=false;
					printf("S1_addr = 0x%02x, 长按按键%d\n",s1_key[i].addr,j);
				}
				if(s1_key[i].SW_short[j]){
					s1_key[i].SW_short[j]=false;
					printf("S1_addr = 0x%02x, 短按按键%d\n",s1_key[i].addr,j);
				}
			}
		}
		usleep(20000);
		
	}

    	return 0;
}
