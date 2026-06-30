#include "s2.h"
#include "i2c.h"
#include <unistd.h>


int bright_idx=-1;
/*********************************************************************************************************
函数名:     I2c_s2_find
入口参数:   list  设备表地址
出口参数:   无
返回值：         无
作者:       ljy
日期:       2025/8/12
调用描述:   检测S2设备是否存在，存在则显示S5设备地址个数与标号
**********************************************************************************************************/
void I2c_s2_find(i2c_probe_target *list)
{
	bright_idx = i2c_device_idx("S2_BRIGHTNESS");
	if((bright_idx<0)||(list[bright_idx].no<0)||(!list[bright_idx].detected_count)){
		perror("未探测到S2设备");
		exit(EXIT_FAILURE);
	}
	else{
		printf("检测到S2设备个数为:%d\n",list[bright_idx].detected_count);
		for(int i=0;i<list[bright_idx].detected_count;i++){
			printf("检测到的一个S2地址为0x%02x,标号为%d\n",list[bright_idx].detected_addrs[i],i);
		}
	}
}

/*********************************************************************************************************
函数名:	    I2c_s2_init
入口参数:   fd  i2c文件描述符		addr 光照传感器的i2c地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/8/12
调用描述:   初始化S2
**********************************************************************************************************/
void I2c_s2_init(int fd, unsigned char addr)
{
	I2c_write_cmd(fd, addr, 0x01);
}

/*********************************************************************************************************
函数名:	    I2c_s2_init_all
入口参数:   fd  i2c文件描述符		list  设备表地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/8/12
调用描述:   初始化所有S2
**********************************************************************************************************/
void I2c_s2_init_all(int fd, i2c_probe_target *list)
{
	for(int i=0;i<list[bright_idx].detected_count;i++){
		I2c_s2_init(fd,list[bright_idx].detected_addrs[i]);
	}
}



/*********************************************************************************************************
函数名:	    s2_read_bh1750_value
入口参数:   fd  i2c文件描述符		addr 光照传感器的i2c地址
出口参数:   无 
返回值：         ss  光照传感器返回值
作者:       ljy
日期:       2025/8/12
调用描述:   获取光照传感器的光照数值
**********************************************************************************************************/
float s2_read_bh1750_value(int fd,unsigned char addr)
{	
	uint16_t tmp;
	uint8_t ss_value[2];
	float ss;
	
     	I2c_write_cmd(fd,addr,0x01);
	I2c_write_cmd(fd,addr,0x10);
	usleep(120 * 1000);  // 120毫秒 = 120000微秒
	I2c_read_regs(fd,addr,0x10,ss_value,sizeof(ss_value));
	tmp = (ss_value[0]<<8) + ss_value[1];
	ss = (float)tmp*10/12;
	
	return ss;
}
