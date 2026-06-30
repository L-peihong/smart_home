#ifndef i2c_probe_h
#define i2c_probe_h

#define I2C_PROBE_STR_LEN 24
#define MAX_DETECTED_ADDRS 4

/**
 * @brief I2C设备探测目标结构体
 * 
 * 该结构体用于定义I2C设备探测的输入参数和存储探测结果。
 * 支持单设备和多设备探测，能够处理同一类型的多个设备。
 */
struct i2c_probe_target {
    // ========== 输入字段 ==========
    char name[I2C_PROBE_STR_LEN];                    // 设备的可读名称，如"S2_BRIGHTNESS"、"S7_HUMAN"等
    const unsigned char *candidate_addrs;            // 指向候选I2C地址数组的指针，包含该设备类型的所有可能地址
    const unsigned int candidate_addrs_len;          // 候选地址数组的长度，告诉探测函数需要检查多少个地址

    // ========== 输出字段 ==========
    char path[I2C_PROBE_STR_LEN];                    // 设备文件路径，如"/dev/i2c-1"，用于后续设备通信
    int no;                                          // I2C总线编号，标识设备连接在哪条I2C总线上
    unsigned char addr;                              // 第一个检测到的设备地址，用于向后兼容原有代码
    
    // ========== 多设备支持字段 ==========
    unsigned char detected_addrs[MAX_DETECTED_ADDRS];// 存储所有检测到的设备地址数组，支持多个同类型设备
    int detected_count;                              // 实际检测到的设备数量，范围：0到MAX_DETECTED_ADDRS
};

void i2c_probe(i2c_probe_target *list, const int len);
int i2c_device_idx(const char *name);

extern const i2c_probe_target I2C_PROBE_LIST[];
extern const int I2C_PROBE_LIST_LEN;

#endif