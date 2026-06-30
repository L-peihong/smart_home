#include "i2c_probe.h"
#include "i2c.h"

#include <stdio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <memory.h>

static const unsigned char ADDR_S1[]           = {0x74,0x75,0x76,0x77};
static const unsigned char ADDR_S2_BH[]        = {0x46>>1, 0xB8>>1};                    // light intensity sensor
static const unsigned char ADDR_S2_ICM[]       = {0xD0>>1, 0xD2>>1};                    // inertial sensor
static const unsigned char ADDR_S3_MIC[]       = {0x34>>1};
static const unsigned char ADDR_S5_NFC[]       = {0x50>>1, 0x52>>1, 0x54>>1, 0x56>>1};
static const unsigned char ADDR_S6_UTRASONIC[] = {0x58>>1, 0x5A>>1, 0x5C>>1, 0x5E>>1};
static const unsigned char ADDR_S7_HUMAN[]     = {0x30>>1, 0x32>>1, 0x34>>1, 0x36>>1};
static const unsigned char ADDR_S8_SHT[]       = {0x88>>1, 0x8A>>1};                    // temperature & humidity sensor
static const unsigned char ADDR_E1_LIGHT[]     = {0xC0>>1, 0xC2>>1, 0xC4>>1, 0xC6>>1};
static const unsigned char ADDR_E1_DISPLAY[]   = {0xE0>>1, 0xE2>>1, 0xE4>>1, 0xE6>>1};
static const unsigned char ADDR_E2_FAN[]       = {0xC8>>1, 0xCA>>1, 0xCC>>1, 0xCE>>1};
static const unsigned char ADDR_E3_MOTOR[]     = {0x38>>1, 0x3A>>1, 0x3C>>1, 0x3E>>1};

#define __FILL(n) n, sizeof(n), {}, -1, 0, {}, 0

const i2c_probe_target I2C_PROBE_LIST[] = {
    { "S1_KEYS",           __FILL(ADDR_S1) },
    { "S2_BRIGHTNESS",     __FILL(ADDR_S2_BH) },
    { "S2_INERTIAL",       __FILL(ADDR_S2_ICM) },
    { "S5_NFC",            __FILL(ADDR_S5_NFC) },
    { "S6_UTRASONIC",      __FILL(ADDR_S6_UTRASONIC) },  
    { "S7_HUMAN",          __FILL(ADDR_S7_HUMAN) },
    { "S8_TEMP&HUMIDITY",  __FILL(ADDR_S8_SHT) },
    { "E1_LIGHT",          __FILL(ADDR_E1_LIGHT) },
    { "E1_DISPLAY",        __FILL(ADDR_E1_DISPLAY) },
    { "E2_FAN",            __FILL(ADDR_E2_FAN) },
    { "E3_MOTOR",          __FILL(ADDR_E3_MOTOR) },      
};

#undef __FILL

const int I2C_PROBE_LIST_LEN = sizeof(I2C_PROBE_LIST) / sizeof(i2c_probe_target);

//#define DEBUG

#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

void check_e1_e2_addr(void){
	int fd = I2c_open("/dev/i2c-5");
	if (ioctl(fd, I2C_SLAVE_FORCE, 0x70) >= 0){
		I2c_write_reg(fd, 0x70, 0x00, 0x00);
        I2c_write_reg(fd, 0x71, 0x00, 0x00);
        I2c_write_reg(fd, 0x72, 0x00, 0x00);
        I2c_write_reg(fd, 0x73, 0x00, 0x00);
	}
	I2c_close(fd);
	
}

void i2c_probe(i2c_probe_target *list, const int len) {
	check_e1_e2_addr();
    char dev_name[I2C_PROBE_STR_LEN];

    debug("i2c_probe: len = %d\n", len);

    for (int i = I2C_DEV_NO_START; i < (I2C_DEV_NO_START + I2C_DEV_NUM); ++i) {
        memset(dev_name, 0, sizeof(dev_name));
        sprintf(dev_name, "%s%d", I2C_DEV_NAME_COMMON, i);

        int fd = I2c_open(dev_name);

        if (fd > 0) {
            debug("open %s\n", dev_name);
            for (int j = 0;j < len;j++) {
                i2c_probe_target *tgt = list + j;

                debug("%s: %s, on", dev_name, tgt->name);
                for (int j = 0;j < tgt->candidate_addrs_len;j++){
                    debug(" %#02x", tgt->candidate_addrs[j]);
                }
                debug(" ");

                unsigned char detectedAddrs[MAX_DETECTED_ADDRS];
                int detectedCount;
                int rtn = I2c_detect_all(
                    fd, 
                    (unsigned char*)tgt->candidate_addrs,
                    tgt->candidate_addrs_len,
                    detectedAddrs,
                    &detectedCount
                );
                if (rtn >= 0 && detectedCount > 0) {
                    // device(s) found
                    memset(tgt->path, 0, I2C_PROBE_STR_LEN);
                    strncpy(tgt->path, dev_name, I2C_PROBE_STR_LEN-1);
                    tgt->addr = detectedAddrs[0];  // keep first address for compatibility
                    tgt->no = i;
                    
                    // store all detected addresses
                    tgt->detected_count = detectedCount;
                    for (int k = 0; k < detectedCount && k < MAX_DETECTED_ADDRS; k++) {
                        tgt->detected_addrs[k] = detectedAddrs[k];
                    }
                    debug("found %d device(s) at addr", detectedCount);
                    for (int k = 0; k < detectedCount; k++) {
                        debug(" %x", detectedAddrs[k]);
                    }
                    debug("\n");
                } else {
                    debug("addr not found \n");
                }
            }
            I2c_close(fd);
        }
    }
}



/**
 * @brief 根据设备名称在I2C_PROBE_LIST中查找设备索引
 * 
 * 该函数在全局的I2C_PROBE_LIST数组中搜索指定名称的设备。
 * 它通过逐字符比较的方式来匹配设备名称。
 * 
 * 工作原理：
 * 1. 遍历I2C_PROBE_LIST数组中的每个设备
 * 2. 对每个设备，逐字符比较设备名称和输入的名称
 * 3. 如果找到完全匹配的设备名称，返回该设备在数组中的索引
 * 4. 如果遍历完所有设备都没有找到匹配的，返回-1表示未找到
 * 
 * @param name 要搜索的设备名称字符串（例如："S2_BRIGHTNESS"表示光强传感器）
 * @return int 如果找到设备返回其在I2C_PROBE_LIST中的索引（0到I2C_PROBE_LIST_LEN-1），
 *             如果未找到返回-1
 * 
 * 使用示例：
 *   int idx = i2c_device_idx("S2_BRIGHTNESS");  // 查找光强传感器
 *   if (idx >= 0) {
 *       // 设备找到，可以通过list[idx]访问设备信息
 *       printf("找到设备，索引为：%d\n", idx);
 *   } else {
 *       // 设备未找到
 *       printf("未找到指定设备\n");
 *   }
 * 
 * 注意事项：
 * - 函数进行的是精确匹配，设备名称必须完全一致
 * - 比较过程遇到输入名称的结尾字符'\0'时会停止
 * - 该函数是线性搜索，时间复杂度为O(n)
 */
int i2c_device_idx(const char *name) {
    // 遍历所有预定义的I2C设备
    for(int i = 0;i < I2C_PROBE_LIST_LEN;i++) {
        const char *devname = I2C_PROBE_LIST[i].name;  // 获取当前设备的名称
        bool match = true;  // 标记是否匹配
        
        // 逐字符比较设备名称
        for(int j = 0;j < I2C_PROBE_STR_LEN; j++) {
            if (name[j] == 0) break;  // 输入名称结束，停止比较
            if (devname[j] != name[j]) {  // 字符不匹配
                match = false;
                break;
            }
        }

        if (match) {
            return i;  // 找到匹配的设备，返回索引
        }
    }
    return -1;  // 未找到匹配的设备
}
