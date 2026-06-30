#include "s7.h"

int human_idx=-1;
/*********************************************************************************************************
函数名:     I2c_s7_find
入口参数:   list  设备表地址
出口参数:   无
返回值：         无
作者:       ljy
日期:       2025/8/13
调用描述:   检测S7设备是否存在，存在则显示S7设备地址个数与标号
**********************************************************************************************************/
void I2c_s7_find(i2c_probe_target *list)
{
	human_idx = i2c_device_idx("S7_HUMAN");
	if((human_idx<0)||(list[human_idx].no<0)||(!list[human_idx].detected_count)){
		perror("未探测到S7设备");
		exit(EXIT_FAILURE);
	}
	else{
		printf("检测到S7设备个数为:%d\n",list[human_idx].detected_count);
		for(int i=0;i<list[human_idx].detected_count;i++){
			printf("检测到的一个S7地址为%04x,标号为%d\n",list[human_idx].detected_addrs[i],i);
		}
	}
}

/*********************************************************************************************************
函数名:     pca9557_ir_init
入口参数:   fd  i2c文件描述符		addr S7的i2c地址
出口参数:   无
返回值：         无
作者:       ljy
日期:       2025/8/13
调用描述:   初始化pca9557芯片
**********************************************************************************************************/
void pca9557_ir_init(int fd, unsigned char addr )
{
    I2c_write_reg(fd, addr , PCA9557_POLARITY_INVERSION_REG, 0x00);
    I2c_write_reg(fd, addr , PCA9557_CONFIG_REG, 0xFF);
}

/*********************************************************************************************************
函数名:     I2c_s7_init
入口参数:   fd  i2c文件描述符		addr S7的i2c地址
出口参数:   无
返回值：         无
作者:       ljy
日期:       2025/8/13
调用描述:   初始化S7子板
**********************************************************************************************************/
void I2c_s7_init(int fd, unsigned char addr)
{
    pca9557_ir_init(fd, addr);
}

/*********************************************************************************************************
函数名:	    I2c_s7_init_all
入口参数:   fd  i2c文件描述符		list 设备表地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/8/13
调用描述:   初始化所有S7子板
**********************************************************************************************************/
void I2c_s7_init_all(int fd, i2c_probe_target *list)
{
	for(int i=0;i<list[human_idx].detected_count;i++){
		I2c_s7_init(fd,list[human_idx].detected_addrs[i]);
	}
}

/*********************************************************************************************************
函数名:	    I2c_human_detected
入口参数:   fd  i2c文件描述符		addr S7的i2c地址
出口参数:   无 
返回值：         有人状态返回1,无人状态返回0
作者:       ljy
日期:       2025/8/13
调用描述:   用S7的人体传感器检测是否有人存在
**********************************************************************************************************/
unsigned char I2c_human_detected(int fd, unsigned char addr)
{
    int ret;
    unsigned char s_status;

    ret = I2c_read_regs(fd, addr, 0x00, &s_status, 1);

    if (s_status & 0x01)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/*********************************************************************************************************
函数名:	    I2c_human_detected_all
入口参数:   fd  i2c文件描述符		list 设备表地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/8/13
调用描述:   用S7的人体传感器检测是否有人存在
**********************************************************************************************************/
void I2c_human_detected_all(int fd, i2c_probe_target *list)
{
	for(int i=0;i<list[human_idx].detected_count;i++){
		if(I2c_human_detected(fd,list[human_idx].detected_addrs[i])){
			printf("S7_addr:0x%02x 检测到有人\n",list[human_idx].detected_addrs[i]);
		}
		else{
			printf("S7_addr:0x%02x 检测到无人\n",list[human_idx].detected_addrs[i]);
		}
		
	}
}
