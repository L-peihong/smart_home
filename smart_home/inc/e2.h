#ifndef E2_H
#define E2_H

#include "i2c.h"
#include "i2c_probe.h"
#include <stdint.h>

#define  E2_PCA9685_SUBADR1         0x2
#define  E2_PCA9685_SUBADR2         0x3
#define  E2_PCA9685_SUBADR3         0x4
#define  E2_PCA9685_MODE1           0x0
#define  E2_PCA9685_PRESCALE        0xFE

#define  CN0_ON_L               0x6
#define  CN0_ON_H               0x7
#define  CN0_OFF_L              0x8
#define  CN0_OFF_H              0x9

#define  ALLCN_ON_L             0xFA
#define  ALLCN_ON_H             0xFB
#define  ALLCN_OFF_L            0xFC
#define  ALLCN_OFF_H            0xFD

void I2c_e2_find(i2c_probe_target *list);
void I2c_e2_init(int fd, unsigned char addr);
void e2_off(int fd, unsigned char slave_address);
void e2_setPWM(int fd, unsigned char slave_address, unsigned char num, unsigned short on, unsigned short off);
void e2_speed_control(int fd, unsigned char addr, unsigned char speed_level);  // 修正：添加空格

void I2c_e2_init_all(int fd, i2c_probe_target *list);
void e2_off_all(int fd, i2c_probe_target *list);
void e2_speed_control_all(int fd, i2c_probe_target *list, unsigned char speed_level);

#endif