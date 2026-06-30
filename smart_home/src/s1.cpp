#include "s1.h"

int key_idx=-1;
s1_key_process *s1_key=NULL;
/*********************************************************************************************************
函数名:     I2c_s1_find
入口参数:   list  设备表地址
出口参数:   无
返回值：         无
作者:       ljy
日期:       2025/8/14
调用描述:   检测S1设备是否存在，存在则显示S1设备地址个数与标号
**********************************************************************************************************/
void I2c_s1_find(i2c_probe_target *list)
{
	key_idx = i2c_device_idx("S1_KEYS");
	if((key_idx<0)||(list[key_idx].no<0)||(!list[key_idx].detected_count)){
		perror("未探测到S1设备");
		exit(EXIT_FAILURE);
	}
	else{
		printf("检测到S1设备个数为:%d\n",list[key_idx].detected_count);
		s1_key=(s1_key_process *)malloc(list[key_idx].detected_count*sizeof(s1_key_process ));
		for(int i=0;i<list[key_idx].detected_count;i++){
			printf("检测到的一个S1地址为0x%02x,标号为%d\n",list[key_idx].detected_addrs[i],i);
			s1_key[i].addr=list[key_idx].detected_addrs[i];
		}
	}
	
}
/*********************************************************************************************************
函数名:     I2c_s1_init
入口参数:   fd  i2c文件描述符		addr S1的i2c地址
出口参数:   无
返回值：         无
作者:       ljy
日期:       2025/8/14
调用描述:   初始化S1子板
**********************************************************************************************************/
void I2c_s1_init(int fd, unsigned char addr)
{
    	I2c_write_cmd(fd, addr, 0xA0);
	I2c_write_reg(fd, addr, 0x02, 0x00);
	I2c_write_reg(fd, addr, 0x03, 0x00);
	I2c_write_reg(fd, addr, 0x04, 0x00);
	I2c_write_reg(fd, addr, 0x05, 0x00);
	I2c_write_reg(fd, addr, 0x06, 0x00);
	I2c_write_reg(fd, addr, 0x07, 0x00);
	I2c_write_reg(fd, addr, 0x08, 0x00);
	I2c_write_reg(fd, addr, 0x09, 0x00);
}

/*********************************************************************************************************
函数名:	    I2c_s1_init_all
入口参数:   fd  i2c文件描述符		list 设备表地址
出口参数:   无 
返回值：         无
作者:       ljy
日期:       2025/8/13
调用描述:   初始化所有S1子板
**********************************************************************************************************/
void I2c_s1_init_all(int fd, i2c_probe_target *list)
{
	for(int i=0;i<list[key_idx].detected_count;i++){
		I2c_s1_init(fd,list[key_idx].detected_addrs[i]);
	}
}

/*********************************************************************************************
函数名:      s1_key_scan
入口参数:    fd  i2c文件描述符		addr S1的i2c地址
出口参数:    无
返回值：           返回按键扫描键值
作者：                LJY
日期:        2025/8/14
调用描述:    调用此函数,得到键值
**********************************************************************************************/
unsigned char s1_key_scan(int fd,unsigned char addr)
{
	unsigned char key_value;
	unsigned char keyvalue[6];
	I2c_read_regs(fd,addr,HT16K33_KEY_KS0,keyvalue,HT16K33_KEY_REG_NUM);
	if(keyvalue[0]&0x01)
	{
		key_value = SW1;
	}	
	else if(keyvalue[2]&0x01)
	{	
		key_value = SW2;	 
	}
	else if(keyvalue[4]&0x01)
	{
		key_value = SW3;
	}	
	else if(keyvalue[0]&0x02)
	{
		key_value = SW4;
	}	
	else if(keyvalue[2]&0x02)
	{
		key_value = SW5; 
	}	
	else if(keyvalue[4]&0x02)
	{
		key_value = SW6; 
	}
	else if(keyvalue[0]&0x04)
	{
		key_value = SW7;
	}
	else if(keyvalue[2]&0x04)
	{
		key_value = SW8;
	}
	else if(keyvalue[4]&0x04)
	{
		key_value = SW9;
	}
	else if(keyvalue[0]&0x08)
	{
		key_value = SWA;
	}
	else if(keyvalue[2]&0x08)
	{
		key_value = SW0;
	}
	else if(keyvalue[4]&0x08)
	{
		key_value = SWC;
	}	
	else 
	{
		key_value = SWN;
	}	

	return  key_value;		
}

/*********************************************************************************************
函数名:      get_timestamp
入口参数:    无
出口参数:    无
返回值：           时间戳毫秒
作者：                LJY
日期:        2025/8/14
调用描述:    获取时间戳
**********************************************************************************************/
uint64_t get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/*********************************************************************************************
函数名:      process_keys
入口参数:    fd  i2c文件描述符		addr S1的i2c地址
	     list 设备表地址
出口参数:    无
返回值：           无
作者：                LJY
日期:        2025/8/14
调用描述:    单个S1确认是按键事件是长按还是短按
**********************************************************************************************/
void process_keys(int fd,unsigned char addr,i2c_probe_target *list) {
	int i;
	for(i=0;i<list[key_idx].detected_count;i++){
		if(s1_key[i].addr==addr)break;
	}
	unsigned char key_value = s1_key_scan(fd, addr);
    	uint64_t current_time = get_timestamp();
    
    	switch(s1_key[i].key_state) {
        	case KEY_NULL:
           		if(key_value != SWN) {
               		s1_key[i].key_down_time = current_time;
                		s1_key[i].key_state = KEY_PRESSED;  // 进入消抖状态
                		s1_key[i].current_key = key_value;
                		s1_key[i].key_processed = false;    // 重置处理标志
            		}
            	break;
            
        	case KEY_PRESSED:
            	// 消抖检查：持续按下超过消抖时间才算有效按下
            		if((current_time - s1_key[i].key_down_time) > DEBOUNCE_TIME) {
                		if(key_value == s1_key[i].current_key) {
                    			s1_key[i].key_state = KEY_HOLDING;  // 确认有效按下
                		} 
                		else {
                    			s1_key[i].key_state = KEY_NULL;     // 抖动，返回空闲状态
                		}
            		}
            	break;    
        	case KEY_HOLDING:
            		if(key_value == SWN) {  // 按键释放
                		uint64_t press_duration = current_time - s1_key[i].key_down_time;
                		if(!s1_key[i].key_processed) {
                    			if(press_duration >= LONG_PRESS_TIME) {
                        		// 长按处理
                        			s1_key[i].SW_long[s1_key[i].current_key]=true;
                    			} 
                    			if(press_duration<=SHORT_PRESS_TIME){
                        		// 短按处理
                        			s1_key[i].SW_short[s1_key[i].current_key]=true;
                    			}
                    		s1_key[i].key_processed = true;
                		}
                
                	// 重置状态
               	s1_key[i].key_state = KEY_NULL;
                	s1_key[i].current_key = SWN;
            		}
            	break;
    }
}
