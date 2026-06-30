#ifndef  E1_H
#define  E1_H

#include "i2c.h"
#include "i2c_probe.h"
#include <stdint.h>



static const uint8_t disdata[22][2] ={
		{0xF8,0x01},	// 0
		{0x30,0x00},	// 1
		{0xD8,0x02},	// 2
		{0x78,0x02},	// 3
		{0x30,0x03},	// 4
		{0x68,0x03},	// 5
		{0xE8,0x03},	// 6
		{0x38,0x00},	// 7
		{0xF8,0x03},	// 8
		{0x78,0x03},	// 9
		{0xB8,0x03},	// A
		{0xE0,0x03},	// B
		{0xC8,0x01},	// C
		{0xF0,0x02},	// D
		{0xC8,0x03},	// E
		{0x88,0x03},	// F
		{0x00,0x00},	// 不显示
		{0xC1,0x03},	// 't'
		{0xB3,0x03},	// 'H'
		{0x40,0x00},	// 下划线 '_'
		{0x00,0x02},	// 中划线 '-'
		{0x00,0x04},	// 小数点 '.'
};




//#define PCA9685_ADDRESS_E1    0xC0			//0xC0 0xC2 0xC4 0xC6
//#define HT16K33_ADDRESS_E1    0xE0			//0xE0 0xE2 0xE4 0xE6


#define E1_PCA9685_SUBADR1       0x2
#define E1_PCA9685_SUBADR2       0x3
#define E1_PCA9685_SUBADR3       0x4
#define E1_PCA9685_MODE1         0x0
#define E1_PCA9685_PRESCALE      0xFE

#define LED_R 1
#define LED_G 0
#define LED_B 2

#define LED0_ON_L             0x6
#define LED0_ON_H             0x7
#define LED0_OFF_L            0x8
#define LED0_OFF_H            0x9
#define ALLLED_ON_L           0xFA
#define ALLLED_ON_H           0xFB
#define ALLLED_OFF_L          0xFC
#define ALLLED_OFF_H          0xFD


/* HT16K33  CMD */
#define	SYSTEM_OFF				  0x20
#define	SYSTEM_ON				  0x21
#define	SET_INT_NONE		    	  0xA0
#define	DISPLAY_ON			     	  0x81
#define	DISPLAY_OFF			      	  0x80
#define	DIMMING_SET_DEFAULT	  0xEF


#define NODIS                 0x10



/*设置LED灯的颜色*/
typedef enum {
	E1_COLOR_BLACK = 0,	// 黑色
	E1_COLOR_RED,		// 红色
	E1_COLOR_ORANGE,	// 橘色
	E1_COLOR_YELLOW,	// 黄色
	E1_COLOR_GREEN,	// 绿色
	E1_COLOR_CYAN,		// 青色
	E1_COLOR_BLUE,		// 蓝色
	E1_COLOR_PURPLE,	// 紫色
	E1_COLOR_WHITE		// 白色
} e1_led_color_t;






void I2c_e1_led_find(i2c_probe_target *list);
void I2c_e1_led_init(int fd, unsigned char addr);

void e1_led_off(int fd, unsigned char slave_address);
void setPWM(int fd, unsigned char slave_address, unsigned char num, unsigned short on,unsigned short off);
void e1_rgb_color_control(int fd,unsigned char addr,uint8_t red,uint8_t green,uint8_t blue);
void e1_led_ctrl(int fd, unsigned char addr ,e1_led_color_t color, uint8_t brightness);

void I2c_e1_led_init_all(int fd,i2c_probe_target *list);
void e1_rgb_color_control_all(int fd,i2c_probe_target *list,uint8_t red,uint8_t green,uint8_t blue);
void e1_led_ctrl_all(int fd, i2c_probe_target *list, e1_led_color_t color, uint8_t brightness);
void e1_led_off_all(int fd, i2c_probe_target *list);



void I2c_e1_tube_find(i2c_probe_target *list);
void I2c_e1_tube_init(int fd, unsigned char addr);
void ht16k33_e1_display_off(int fd, unsigned char addr);
void ht16k33_e1_display_data(int fd, unsigned char addr,uint8_t bit,uint8_t data);

void e1_digital_display(int fd, unsigned char addr,uint8_t dis1,uint8_t dis2,uint8_t dis3,uint8_t dis4);
void e1_digital_display_off(int fd, unsigned char addr);
void e1_digital_bit_display_char(int fd, unsigned char addr, uint8_t bit, uint8_t ch);

void e1_digital_display_off_all(int fd,i2c_probe_target *list);
void I2c_e1_tube_init_all(int fd,i2c_probe_target *list);
#endif
