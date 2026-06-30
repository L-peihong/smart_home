#ifndef S1_H
#define S1_H

#include "i2c_probe.h"
#include "i2c.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>      // 时间函数头文件

#define	HT16K33_KEY_KS0		0x40
#define	HT16K33_KEY_REG_NUM		6

// 时间常量定义(单位:毫秒)
#define DEBOUNCE_TIME   20    // 消抖时间
#define LONG_PRESS_TIME 2000  // 长按判定时间(2秒)
#define SHORT_PRESS_TIME 600  // 短按判定时间(0.6秒)


#define SWN  0   //无按键按下
#define SW1  1
#define SW2  2
#define SW3  3
#define SW4  4
#define SW5  5
#define SW6  6
#define SW7  7
#define SW8  8
#define SW9  9
#define SWA  10  // *
#define SW0  11  // 0
#define SWC  12  // #

// 按键状态机
typedef enum {
    KEY_NULL,       // 无按键状态
    KEY_PRESSED,    // 按键按下待确认
    KEY_HOLDING     // 按键已确认按下
} KeyState;

typedef struct {
    unsigned char addr;
    bool SW_long[13]={false};  // 按键长按标志位
    bool SW_short[13]={false}; // 按键长按标志位
    KeyState key_state = KEY_NULL;
    uint8_t current_key = SWN;
    uint64_t key_down_time = 0;           // 现在存储微秒时间戳
    bool key_processed = false;
}s1_key_process;

extern s1_key_process *s1_key;

void I2c_s1_find(i2c_probe_target *list);
void I2c_s1_init(int fd, unsigned char addr);
void I2c_s1_init_all(int fd, i2c_probe_target *list);
unsigned char s1_key_scan(int fd,unsigned char addr);
void process_keys(int fd,unsigned char addr,i2c_probe_target *list);

#endif
