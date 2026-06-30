#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <assert.h>
#include "e1.h"
#include "i2c_probe.h"


extern int led_idx;
extern int tube_idx;
int main(int argc, char **argv)
{
    	i2c_probe_target list[I2C_PROBE_LIST_LEN] = {};
    	memcpy(list, I2C_PROBE_LIST, sizeof(list));
    	i2c_probe(list, I2C_PROBE_LIST_LEN);
    	printf("detect finish\n");
    	
    	I2c_e1_led_find(list);
    	I2c_e1_tube_find(list);
    	
    	int fd = I2c_open(list[led_idx].path);
	I2c_e1_led_init_all(fd,list);
	I2c_e1_tube_init_all(fd,list);
    	int index = 0; 
    while (1)
    {
        index++;
        
        // 循环设置灯的颜色
        if(index % 3 == 0){
            e1_rgb_color_control_all(fd, list, 60, 0, 0);
        } 
        else if(index % 3 == 1){
            e1_rgb_color_control_all(fd, list, 0, 60, 0);
        } 
        else if(index % 3 == 2){
            e1_rgb_color_control_all(fd, list, 0, 0, 60);
        }
        
        // 循环设置数码管显示数字
        e1_digital_display_off(fd, list[tube_idx].detected_addrs[0]);
        e1_digital_bit_display_char(fd, list[tube_idx].detected_addrs[0], index % 4, '0'+ (index % 10));
        sleep(1);
    }

    return 0;
}
