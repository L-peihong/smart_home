#ifndef I2C
#define I2C

#include <stdio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#define I2C_DEV_NAME_COMMON		"/dev/i2c-"
#define I2C1_DEV_NAME			"/dev/i2c-1"
#define I2C2_DEV_NAME			"/dev/i2c-2"
#define I2C3_DEV_NAME			"/dev/i2c-3"
#define I2C4_DEV_NAME			"/dev/i2c-4"
#define I2C5_DEV_NAME			"/dev/i2c-5"
#define I2C6_DEV_NAME			"/dev/i2c-6"

#define I2C_DEV_NO_START		4
#define I2C_DEV_NUM				6

typedef struct{
    int fd;
    int no;
    unsigned char addr;
    unsigned char flag;
} i2c_addr_def;

int I2c_open(char *devname);
void I2c_close(int fd);
int I2c_detect(int fd, unsigned char *addrArray, int num, unsigned char *addr);
int I2c_detect_all(int fd, unsigned char *addrArray, int num, unsigned char *detectedAddrs, int *detectedCount);
int I2c_write_cmd(int fd, unsigned char addr, unsigned char reg);
int I2c_write_reg(int fd, unsigned char addr, unsigned char reg, unsigned char value);
int I2c_write_reg_datas(int fd, unsigned char addr, unsigned char reg, unsigned char *value, int num);
int I2c_read_regs(int fd, unsigned char addr, unsigned char reg, unsigned char *val, int num);
int I2c_read_regs_addr16(int fd, unsigned char addr, unsigned short reg, unsigned char *val, int num);

#endif //I2C
