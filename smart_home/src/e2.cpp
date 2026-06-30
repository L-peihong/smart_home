#include "e2.h"


int fan_idx=-1;
/*********************************************************************************************************
函数名:     I2c_e2_find
入口参数:   list  设备表地址
出口参数:   无
返回值：         无
作者:       ljy
日期:       2025/8/11
调用描述:   检测风扇设备是否存在，存在则显示风扇设备地址个数与标号
**********************************************************************************************************/
void I2c_e2_find(i2c_probe_target *list)
{
	fan_idx = i2c_device_idx("E2_FAN");
	if((fan_idx<0)||(list[fan_idx].no<0)||(!list[fan_idx].detected_count)){
		perror("未探测到E2设备");
		exit(EXIT_FAILURE);
	}
	else{
		printf("检测到E2设备个数为:%d\n",list[fan_idx].detected_count);
		for(int i=0;i<list[fan_idx].detected_count;i++){
			printf("检测到的一个E2地址为0x%02x,标号为%d\n",list[fan_idx].detected_addrs[i],i);
		}
	}
	
}

/*********************************************************************************************************
函数名:	    I2c_e2_init_all
入口参数:   fd  i2c文件描述符		list  设备表地址		
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/8/11
调用描述:   初始化所有E2
**********************************************************************************************************/
void I2c_e2_init_all(int fd,i2c_probe_target *list)
{
	for(int i=0;i<list[fan_idx].detected_count;i++){
		I2c_e2_init(fd,list[fan_idx].detected_addrs[i]);
	}
}


/*********************************************************************************************************
函数名:	    I2c_e2_init
入口参数:   fd  i2c文件描述符		addr 风扇的i2c地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/27
调用描述:   初始化风扇并设置转速为0
**********************************************************************************************************/
void I2c_e2_init(int fd, unsigned char addr)
{
	I2c_write_reg(fd, addr, E2_PCA9685_MODE1, 0x00);
	e2_off(fd, addr);
}

/*********************************************************************************************************
函数名:	    e2_off_all
入口参数:   fd  i2c文件描述符		list  设备表地址 
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/8/11
调用描述:   关闭所有E2风扇
**********************************************************************************************************/
void e2_off_all(int fd, i2c_probe_target *list)
{
	for(int i=0;i<list[fan_idx].detected_count;i++){
		e2_off(fd,list[fan_idx].detected_addrs[i]);
	}
}


/*********************************************************************************************************
函数名:	    e2_off
入口参数:   fd  i2c文件描述符		slave_address 接收信息的pac9685地址  
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/27
调用描述:   关闭风扇
**********************************************************************************************************/
void e2_off(int fd, unsigned char slave_address)
{
	I2c_write_reg(fd, slave_address, ALLCN_ON_L, 0);
	I2c_write_reg(fd, slave_address, ALLCN_ON_H, 0);
	I2c_write_reg(fd, slave_address, ALLCN_OFF_L, 0);
	I2c_write_reg(fd, slave_address, ALLCN_OFF_H, 0);//有一个版本写的是0x10，个人认为是0
}

/*********************************************************************************************************
函数名:	    setPWM
入口参数:   fd  i2c文件描述符		slave_address 接收信息的pac9685地址
	    num PCA9685对应的输出口	on输出高电平使能计数值		off输出高电平关闭计数值	 
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/27
调用描述:   通过on与off的值调节pwm占空比
**********************************************************************************************************/
void e2_setPWM(int fd, unsigned char slave_address, unsigned char num, unsigned short on, unsigned short off)
{
	I2c_write_reg(fd, slave_address, CN0_ON_L + 4 * num, on);
	I2c_write_reg(fd, slave_address, CN0_ON_H + 4 * num, on >> 8);
	I2c_write_reg(fd, slave_address, CN0_OFF_L + 4 * num, off);
	I2c_write_reg(fd, slave_address, CN0_OFF_H + 4 * num, off >> 8);
}

/*********************************************************************************************************
函数名:	    e2_speed_control
入口参数:   fd  i2c文件描述符		addr 风扇的i2c地址
	    speed_level 风扇转速，最大为100
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/27
调用描述:   设置风扇的转速，最大为100，超过100则为100
**********************************************************************************************************/
void e2_speed_control(int fd,unsigned char addr,unsigned char speed_level)
{
	uint16_t off,on;
	if(speed_level > 100)
	{
		speed_level = 100;
	}
	on = 0x00;
	off = on + 0xfff*speed_level/100;
	e2_setPWM(fd, addr, 0, on, off);
}

/*********************************************************************************************************
函数名:	    e2_speed_control_all
入口参数:   fd  i2c文件描述符		list  设备表地址
			speed_level 风扇转速，最大为100
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/27
调用描述:   将所有风扇设置为同一转速，最大为100，超过100则为100
**********************************************************************************************************/
void e2_speed_control_all(int fd,i2c_probe_target *list,unsigned char speed_level)
{
	for(int i=0;i<list[fan_idx].detected_count;i++){
		e2_speed_control(fd,list[fan_idx].detected_addrs[i],speed_level);
	}
}

