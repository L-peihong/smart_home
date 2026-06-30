#include "e1.h"


int led_idx=-1;
int tube_idx=-1;
/*********************************************************************************************************
函数名:     I2c_e1_led_find
入口参数:   list  设备表地址
出口参数:   无
返回值：         无
作者:       ljy
日期:       2025/8/10
调用描述:   检测led设备是否存在，存在则显示led设备地址个数与标号
**********************************************************************************************************/
void I2c_e1_led_find(i2c_probe_target *list)
{
	led_idx = i2c_device_idx("E1_LIGHT");
	if((led_idx<0)||(list[led_idx].no<0)||(!list[led_idx].detected_count)){
		perror("未探测到led设备");
		exit(EXIT_FAILURE);
	}
	else{
		printf("检测到E1_led设备个数为:%d\n",list[led_idx].detected_count);
		for(int i=0;i<list[led_idx].detected_count;i++){
			printf("检测到的一个E1_led地址为0x%02x,标号为%d\n",list[led_idx].detected_addrs[i],i);
		}
	}
	
}

/*********************************************************************************************************
函数名:	    e1_led_off
入口参数:   fd  i2c文件描述符		slave_address 接收信息的pac9685地址  
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/22
调用描述:   熄灭led
**********************************************************************************************************/
void e1_led_off(int fd, unsigned char slave_address)
{
	I2c_write_reg(fd, slave_address, ALLLED_ON_L, 0);
	I2c_write_reg(fd, slave_address, ALLLED_ON_H, 0);
	I2c_write_reg(fd, slave_address, ALLLED_OFF_L, 0);
	I2c_write_reg(fd, slave_address, ALLLED_OFF_H, 0);//有一个版本写的是0x10，个人认为是0
}

/*********************************************************************************************************
函数名:	    e1_led_off_all
入口参数:   fd  i2c文件描述符		list  设备表地址 
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/8/10
调用描述:   熄灭所有E1的led
**********************************************************************************************************/
void e1_led_off_all(int fd, i2c_probe_target *list)
{
	for(int i=0;i<list[led_idx].detected_count;i++){
		e1_led_off(fd,list[led_idx].detected_addrs[i]);
	}
}

/*********************************************************************************************************
函数名:	    setPWM
入口参数:   fd  i2c文件描述符		slave_address 接收信息的pac9685地址
	    num PCA9685对应的输出口	on输出高电平使能计数值		off输出高电平关闭计数值 
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/22
调用描述:   通过on与off的值调节pwm占空比
**********************************************************************************************************/
void setPWM(int fd, unsigned char slave_address, unsigned char num, unsigned short on, unsigned short off)
{
	I2c_write_reg(fd, slave_address, LED0_ON_L + 4 * num, on);
	I2c_write_reg(fd, slave_address, LED0_ON_H + 4 * num, on >> 8);
	I2c_write_reg(fd, slave_address, LED0_OFF_L + 4 * num, off);
	I2c_write_reg(fd, slave_address, LED0_OFF_H + 4 * num, off >> 8);
}

/*********************************************************************************************************
函数名:	    I2c_e1_led_init
入口参数:   fd  i2c文件描述符		addr led的i2c地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/22
调用描述:   初始化led并熄灭
**********************************************************************************************************/
void I2c_e1_led_init(int fd, unsigned char addr)
{
	I2c_write_reg(fd, addr, E1_PCA9685_MODE1, 0x00);
	e1_led_off(fd, addr);
}

/*********************************************************************************************************
函数名:	    I2c_e1_led_init_all
入口参数:   fd  i2c文件描述符		list  设备表地址		
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/8/10
调用描述:   初始化所有E1的led并熄灭
**********************************************************************************************************/
void I2c_e1_led_init_all(int fd,i2c_probe_target *list)
{
	
	for(int i=0;i<list[led_idx].detected_count;i++){
		I2c_e1_led_init(fd,list[led_idx].detected_addrs[i]);
	}
}


/*********************************************************************************************************
函数名:	    e1_rgb_color_control
入口参数:   fd   i2c文件描述符		addr led的i2c地址
	    red  红色占比	        green 绿色占比	       blue 蓝色占比
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/22
调用描述:   通过调节led的红色、绿色和蓝色的占比使led显示不同的颜色
**********************************************************************************************************/
void e1_rgb_color_control(int fd,unsigned char addr,uint8_t red,uint8_t green,uint8_t blue)
{
	unsigned short on, off;
    	on = 0x0f;

	off = on + red * 0x10;
	setPWM(fd, addr, LED_R, on, off);    //设置红色

	off = on + green * 0x10;
	setPWM(fd, addr, LED_G, on, off);    //设置绿色

	off = on + blue * 0x10;
	setPWM(fd, addr, LED_B, on, off);    //设置蓝色
}

/*********************************************************************************************************
函数名:	    e1_rgb_color_control_all
入口参数:   fd  i2c文件描述符		list  设备表地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/8/10
调用描述:   使所有E1的led呈现同一颜色
**********************************************************************************************************/
void e1_rgb_color_control_all(int fd,i2c_probe_target *list,uint8_t red,uint8_t green,uint8_t blue)
{
	for(int i=0;i<list[led_idx].detected_count;i++){
		e1_rgb_color_control(fd,list[led_idx].detected_addrs[i],red,green,blue);
	}
}

/*********************************************************************************************************
函数名:	    e1_led_ctrl
入口参数:   fd  i2c文件描述符		addr led的i2c地址
	    color  颜色种类		brightness  光照强度
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/22
调用描述:   使led显示不同的颜色与光照强度
**********************************************************************************************************/
void e1_led_ctrl(int fd, unsigned char addr, e1_led_color_t color, uint8_t brightness)
{
	uint8_t red = 0, green = 0, blue = 0;

	switch (color)
	{
		case E1_COLOR_BLACK:
		{
			red = 0;
			green = 0;
			blue = 0;
			break;
		}
		case E1_COLOR_RED:
		{
			red = 255;
			green = 0;
			blue = 0;
			break;
		}
		case E1_COLOR_ORANGE:
		{
			red = 255;
			green = 128;
			blue = 0;
			break;
		}
		case E1_COLOR_YELLOW:
		{
			red = 255;
			green = 255;
			blue = 0;
			break;
		}
		case E1_COLOR_GREEN:
		{
			red = 0;
			green = 255;
			blue = 0;
			break;
		}
		case E1_COLOR_CYAN:
		{
			red = 0;
			green = 255;
			blue = 255;
			break;
		}
		case E1_COLOR_BLUE:
		{
			red = 0;
			green = 0;
			blue = 255;
			break;
		}
		case E1_COLOR_PURPLE:
		{
			red = 128;
			green = 0;
			blue = 128;
			break;
		}
		case E1_COLOR_WHITE:
		{
			red = 255;
			green = 255;
			blue = 255;
			break;
		}
		
		default:
			break;
	}

	if (brightness > 100)
		brightness = 100;

	if (brightness == 0)
	{
		red = 0;
		green = 0;
		blue = 0;
	}
	else
	{
		red = (red * brightness) / 100;
		green = (green * brightness) / 100;
		blue = (blue * brightness) / 100;
	}

	e1_rgb_color_control(fd, addr, red, green, blue);
}

/*********************************************************************************************************
函数名:	    e1_led_ctrl_all
入口参数:   fd  i2c文件描述符		list  设备表地址
	    color  颜色种类		brightness  光照强度
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/8/10
调用描述:   设定所有E1的led的颜色与光照强度
**********************************************************************************************************/
void e1_led_ctrl_all(int fd, i2c_probe_target *list, e1_led_color_t color, uint8_t brightness)
{
	for(int i=0;i<list[led_idx].detected_count;i++){
		e1_led_ctrl(fd,list[led_idx].detected_addrs[i],color,brightness);
	}
}


/*********************************************************************************************************
函数名:     I2c_e1_tube_find
入口参数:   设备表地址
出口参数:   无
返回值：         无
作者:       ljy
日期:       2025/8/10
调用描述:   检测数码管设备是否存在，存在则显示数码管设备地址个数与标号
**********************************************************************************************************/
void I2c_e1_tube_find(i2c_probe_target *list)
{
	tube_idx = i2c_device_idx("E1_DISPLAY");
	if((tube_idx<0)||(list[tube_idx].no<0)||(!list[tube_idx].detected_count)){
		perror("未探测到数码管设备");
		exit(EXIT_FAILURE);
	}
	else{
		printf("检测到E1_tube设备个数为:%d\n",list[tube_idx].detected_count);
		for(int i=0;i<list[tube_idx].detected_count;i++){
			printf("检测到的一个E1_tube地址为0x%02x,标号为%d\n",list[tube_idx].detected_addrs[i],i);
		}
	}
}

/*********************************************************************************************************
函数名:	    ht16k33_e1_display_off
入口参数:   fd  i2c文件描述符		addr 数码管的i2c地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/22
调用描述:   熄灭数码管
**********************************************************************************************************/
void ht16k33_e1_display_off(int fd, unsigned char addr)
{
	I2c_write_reg(fd,addr,0x02,0x00);
	I2c_write_reg(fd,addr,0x03,0x00);
	I2c_write_reg(fd,addr,0x04,0x00);
	I2c_write_reg(fd,addr,0x05,0x00);
	I2c_write_reg(fd,addr,0x06,0x00);
	I2c_write_reg(fd,addr,0x07,0x00);
	I2c_write_reg(fd,addr,0x08,0x00);
	I2c_write_reg(fd,addr,0x09,0x00);
	I2c_write_cmd(fd,addr,DISPLAY_ON);
}

/*********************************************************************************************************
函数名:	    I2c_e1_tube_init
入口参数:   fd  i2c文件描述符		addr 数码管的i2c地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/22
调用描述:   初始化数码管并熄灭
**********************************************************************************************************/
void I2c_e1_tube_init(int fd, unsigned char addr)
{
	I2c_write_cmd(fd,addr,SYSTEM_ON);
	ht16k33_e1_display_off(fd,addr);
}

/*********************************************************************************************************
函数名:	    I2c_e1_tube_init_all
入口参数:   fd  i2c文件描述符		addr 数码管的i2c地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/8/10
调用描述:   初始化所有E1的数码管并熄灭
**********************************************************************************************************/
void I2c_e1_tube_init_all(int fd, i2c_probe_target *list)
{
	for(int i=0;i<list[tube_idx].detected_count;i++){
		I2c_e1_tube_init(fd,list[tube_idx].detected_addrs[i]);
	}
}

/*********************************************************************************************************
函数名:	    ht16k33_e1_display_data
入口参数:   fd  i2c文件描述符		addr 数码管的i2c地址
	    bit 数码管位置		data 数码管显示的数据
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/22
调用描述:   数码管对应位置显示数据
**********************************************************************************************************/
void ht16k33_e1_display_data(int fd,unsigned char addr,uint8_t bit,uint8_t data)
{
	switch(bit)
	{
		case  1:
		I2c_write_reg(fd,addr,0x02,disdata[data][0]);
		I2c_write_reg(fd,addr,0x03,disdata[data][1]);

		break;

		case  2:
		I2c_write_reg(fd,addr,0x04,disdata[data][0]);
		I2c_write_reg(fd,addr,0x05,disdata[data][1]);
		break;

		case 3:
		I2c_write_reg(fd,addr,0x06,disdata[data][0]);
		I2c_write_reg(fd,addr,0x07,disdata[data][1]);
		break;

		case 4:
		I2c_write_reg(fd,addr,0x08,disdata[data][0]);
		I2c_write_reg(fd,addr,0x09,disdata[data][1]);
		break;

	}
	I2c_write_cmd(fd,addr,DISPLAY_ON);
}

/*********************************************************************************************************
函数名:	    e1_digital_display
入口参数:   fd  i2c文件描述符		addr 数码管的i2c地址
	    dis1 数码管第一位数据  	dis2 数码管第二位数据	dis3 数码管第三位数据	dis4 数码管第四位数据
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/22
调用描述:   四位数码管显示不同数据
**********************************************************************************************************/
void e1_digital_display(int fd,unsigned char addr,uint8_t dis1,uint8_t dis2,uint8_t dis3,uint8_t dis4)
{
	ht16k33_e1_display_data(fd,addr,1,dis1);
	ht16k33_e1_display_data(fd,addr,2,dis2);
	ht16k33_e1_display_data(fd,addr,3,dis3);
	ht16k33_e1_display_data(fd,addr,4,dis4);
}

/*********************************************************************************************************
函数名:	    e1_digital_display_off
入口参数:   fd  i2c文件描述符		addr 数码管的i2c地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/22
调用描述:   熄灭数码管
**********************************************************************************************************/
void e1_digital_display_off(int fd, unsigned char addr)
{
	ht16k33_e1_display_off(fd,addr);
}

/*********************************************************************************************************
函数名:	    e1_digital_display_off_all
入口参数:   fd  i2c文件描述符		addr 数码管的i2c地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/22
调用描述:   熄灭所有E1的数码管
**********************************************************************************************************/
void e1_digital_display_off_all(int fd, i2c_probe_target *list)
{
	for(int i=0;i<list[tube_idx].detected_count;i++){
		e1_digital_display_off(fd,list[tube_idx].detected_addrs[i]);
	}
}

/*********************************************************************************************************
函数名:	    e1_digital_bit_display_char
入口参数:   fd  i2c文件描述符		addr 数码管的i2c地址
	    bit 数码管位置		ch 数码管显示的数据
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/7/22
调用描述:   数码管对应位置显示数据
**********************************************************************************************************/
void e1_digital_bit_display_char(int fd,unsigned char addr, uint8_t bit, uint8_t ch)
{
	if (ch >= '0' && ch <= '9')
	{
		ht16k33_e1_display_data(fd, addr, (bit + 1), ch - '0');
	}
	else if (ch >= 'A' && ch <= 'F')
	{
		ht16k33_e1_display_data(fd, addr, (bit + 1), ch - 'A' + 10);
	}
	else
	{
		switch (ch)
		{
			case '-':
			{
				I2c_write_reg(fd, addr, ((bit * 2) + 2),disdata[20][0]);
				I2c_write_reg(fd, addr, ((bit * 2) + 3),disdata[20][1]);        
				I2c_write_cmd(fd, addr,DISPLAY_ON);
				break;
			}
			case '_':
			{
				I2c_write_reg(fd, addr, ((bit * 2) + 2), disdata[19][0]);
				I2c_write_reg(fd, addr, ((bit * 2) + 3), disdata[19][1]);
				I2c_write_cmd(fd, addr,DISPLAY_ON);
			}
			case '.':
			{
				I2c_write_reg(fd, addr,((bit * 2) + 2), disdata[21][0]);
				I2c_write_reg(fd, addr,((bit * 2) + 3), disdata[21][1]);
				I2c_write_cmd(fd, addr,DISPLAY_ON);
			}
			case ' ':
			{
				I2c_write_reg(fd, addr, ((bit * 2) + 2), disdata[16][0]);
				I2c_write_reg(fd, addr, ((bit * 2) + 3), disdata[16][1]);
				I2c_write_cmd(fd, addr,DISPLAY_ON);
				break;
			}
			default:
				break;
		}
	}
}





