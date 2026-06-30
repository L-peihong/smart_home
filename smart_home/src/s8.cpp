#include "s8.h"



int sht_idx=-1;
/*********************************************************************************************************
函数名:     I2c_s8_find
入口参数:   list  设备表地址
出口参数:   无
返回值：         无
作者:       ljy
日期:       2025/8/13
调用描述:   检测S8设备是否存在，存在则显示S8设备地址个数与标号
**********************************************************************************************************/
void I2c_s8_find(i2c_probe_target *list)
{
	sht_idx = i2c_device_idx("S8_TEMP&HUMIDITY");
	if((sht_idx<0)||(list[sht_idx].no<0)||(!list[sht_idx].detected_count)){
		perror("未探测到S8设备");
		exit(EXIT_FAILURE);
	}
	else{
		printf("检测到S8设备个数为:%d\n",list[sht_idx].detected_count);
		for(int i=0;i<list[sht_idx].detected_count;i++){
			printf("检测到的一个S8地址为0x%02x,标号为%d\n",list[sht_idx].detected_addrs[i],i);
		}
	}
}

/*********************************************************************************************************
函数名:     计算CRC_BYTE
入口参数:   list  设备表地址
出口参数:   无
返回值：         CRC_BYTE 计算结果
作者:       ljy
日期:       2025/8/13
调用描述:   计算CRC_BYTE
**********************************************************************************************************/
unsigned char s8_sht3x_crc_cal(uint16_t DAT)
{
		unsigned char i,t,temp;
		unsigned char CRC_BYTE;

		CRC_BYTE = 0xFF;
		temp = (DAT>>8) & 0xFF;

		for(t = 0; t < 2; t++)
		{
				CRC_BYTE ^= temp;
				for(i = 0;i < 8;i ++)
				{
						if(CRC_BYTE & 0x80)
						{
							  CRC_BYTE <<= 1;
							  CRC_BYTE ^= 0x31;
						}
						else
						{
							  CRC_BYTE <<= 1;
						}
				}

				if(t == 0)
				{
					  temp = DAT & 0xFF;
				}
		}

	  return CRC_BYTE;
}

/*********************************************************************************************************
函数名:     read_sht3x
入口参数:   fd  i2c文件描述符		addr S8的i2c地址
出口参数:   无
返回值：         sht_para	温湿度数据
作者:       ljy
日期:       2025/8/13
调用描述:   获取温湿度数据
**********************************************************************************************************/
s8_para read_sht3x(int fd,unsigned char addr)
{
	uint8_t  th_value[6];
	s8_para  sht_para;
	uint16_t tmp;
	
    	I2c_write_reg(fd,addr,0x2C,0x0D);
	usleep(100);
    	I2c_read_regs(fd,addr,0x00,th_value,6);
	
	tmp = (th_value[0]<<8) + th_value[1];
	if(s8_sht3x_crc_cal(tmp) == th_value[2])
	{
		sht_para.temperature = (float)tmp*175/(65536-1)-45;
	}
		 
	tmp = (th_value[3]<<8) + th_value[4];
	if(s8_sht3x_crc_cal(tmp) == th_value[5])
	{
		sht_para.humidity = (float)tmp*100/(65536-1);
	}
		 
	return sht_para;
}

/*********************************************************************************************************
函数名:	    read_sht3x_all
入口参数:   fd  i2c文件描述符		list 设备表地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/8/13
调用描述:   获取所有S8温湿度数据
**********************************************************************************************************/
void read_sht3x_all(int fd, i2c_probe_target *list)
{
	s8_para s8_value;
	for(int i=0;i<list[sht_idx].detected_count;i++){
		s8_value=read_sht3x(fd,list[sht_idx].detected_addrs[i]);
		printf("s8_addr:0x%02x	   ",list[sht_idx].detected_addrs[i]);
		printf("temperature = %f   ",s8_value.temperature); 
	 	printf("humidity = %f\n",s8_value.humidity); 
	}
}
