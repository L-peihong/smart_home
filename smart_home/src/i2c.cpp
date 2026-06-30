#include <errno.h>

#include "i2c.h"

void I2c_delay(int time)
{
    while (time--);
}

int I2c_open(char *devname)
{
   return open(devname, O_RDWR);
}

void I2c_close(int fd)
{
	close(fd);
}

__s32 i2c_smbus_access(int file, char read_write, unsigned char command,
		       int size, union i2c_smbus_data *data)
{
	struct i2c_smbus_ioctl_data args;
	__s32 err;

	args.read_write = read_write;
	args.command = command;
	args.size = size;
	args.data = data;

	err = ioctl(file, I2C_SMBUS, &args);
	if (err == -1)
		err = -errno;
	return err;
}

__s32 i2c_smbus_write_quick(int file, unsigned char value)
{
	return i2c_smbus_access(file, value, 0, I2C_SMBUS_QUICK, NULL);
}

__s32 i2c_smbus_read_byte(int file)
{
	union i2c_smbus_data data;
	int err;

	err = i2c_smbus_access(file, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data);
	if (err < 0)
		return err;

	return 0x0FF & data.byte;
}

int I2c_detect(int fd, unsigned char *addrArray, int num, unsigned char *addr)
{
	int	ret = -1;

	for(int i=0; i<num; i++)
	{
		if (ioctl(fd, I2C_SLAVE_FORCE, addrArray[i]) < 0)
		{
			continue;
		}
#if 1
		ret = i2c_smbus_write_quick(fd, I2C_SMBUS_WRITE);
#else
		ret = i2c_smbus_read_byte(fd);
#endif
		if (ret >= 0)
		{
			printf("I2c addr 0X%02X is detected.\n", addrArray[i]);
			*addr = addrArray[i];
			break;
		}
	}

	return ret;
}

/**
 * @brief 检测I2C总线上指定地址数组中的所有可用设备
 * 
 * 该函数扫描指定的I2C地址数组，检测并返回所有在总线上存在的设备地址。
 * 与I2c_detect函数不同，此函数不会在找到第一个设备后停止，而是继续
 * 扫描所有候选地址，返回所有检测到的设备。
 * 
 * 工作原理：
 * 1. 遍历所有候选地址
 * 2. 对每个地址使用ioctl设置为从设备地址
 * 3. 尝试读取一个字节来测试设备是否响应
 * 4. 如果设备响应，将地址添加到检测结果数组中
 * 5. 继续检测下一个地址，直到检查完所有候选地址
 * 
 * @param fd 已打开的I2C设备文件描述符
 * @param addrArray 候选I2C地址数组指针，包含要检测的所有可能地址
 * @param num 候选地址数组的长度，即要检测多少个地址
 * @param detectedAddrs 输出参数，用于存储检测到的设备地址数组
 * @param detectedCount 输出参数，返回实际检测到的设备数量
 * 
 * @return int 成功检测到至少一个设备返回0，未检测到任何设备返回-1
 * 
 * 使用示例：
 *   unsigned char candidates[] = {0x23, 0x5C};  // 候选地址
 *   unsigned char detected[4];                  // 存储检测结果
 *   int count;                                  // 检测到的设备数量
 *   
 *   int result = I2c_detect_all(fd, candidates, 2, detected, &count);
 *   if (result >= 0) {
 *       printf("检测到 %d 个设备\n", count);
 *       for (int i = 0; i < count; i++) {
 *           printf("设备地址: 0x%02X\n", detected[i]);
 *       }
 *   }
 * 
 * 注意事项：
 * - detectedAddrs数组必须有足够空间存储所有可能检测到的地址
 * - 函数会打印每个检测到的设备地址信息
 * - 如果ioctl设置地址失败，会跳过该地址继续检测下一个
 */
int I2c_detect_all(int fd, unsigned char *addrArray, int num, unsigned char *detectedAddrs, int *detectedCount)
{
	int	ret = -1;
	*detectedCount = 0;  // 初始化检测到的设备数量为0

	// 遍历所有候选地址
	for(int i=0; i<num; i++)
	{
		// 设置I2C从设备地址
		if (ioctl(fd, I2C_SLAVE, addrArray[i]) < 0)
		{
			continue;  // 设置地址失败，跳过此地址
		}
#if 1
		ret = i2c_smbus_write_quick(fd, I2C_SMBUS_WRITE);
#else	
		ret = i2c_smbus_read_byte(fd);  // 尝试读取一个字节测试设备响应
#endif
		if (ret >= 0)
		{
			// 设备响应成功，记录此地址
			printf("I2c addr 0X%02X is detected.\n", addrArray[i]);
			detectedAddrs[*detectedCount] = addrArray[i];  // 存储检测到的地址
			(*detectedCount)++;  // 增加检测到的设备计数
		}
	}

	// 如果检测到至少一个设备返回0，否则返回-1
	return (*detectedCount > 0) ? 0 : -1;
}

int I2c_write_cmd(int fd, unsigned char addr, unsigned char reg)
{

    unsigned char				outbuf[1];
    struct i2c_rdwr_ioctl_data	packets;
    struct i2c_msg				messages[1];

    messages[0].addr  = addr;
    messages[0].flags = 0;
    messages[0].len   = sizeof(outbuf);
    messages[0].buf   = outbuf;

    /* The first byte indicates which register we'll write */
    outbuf[0] = reg;

    /* Transfer the i2c packets to the kernel and verify it worked */
    packets.msgs  = messages;
    packets.nmsgs = 1;
    if(ioctl(fd, I2C_RDWR, &packets) < 0) {
        perror("Unable to send data");
        return -1;
    }

    return 0;
}

int I2c_write_reg(int fd, unsigned char addr, unsigned char reg, unsigned char value)
{
    unsigned char				outbuf[2];
    struct i2c_rdwr_ioctl_data	packets;
    struct i2c_msg				messages[1];
 
    messages[0].addr  = addr;
    messages[0].flags = 0;
    messages[0].len   = sizeof(outbuf);
    messages[0].buf   = outbuf;
 
    /* The first byte indicates which register we'll write */
    outbuf[0] = reg;
 
    /* 
     * The second byte indicates the value to write.  Note that for many
     * devices, we can write multiple, sequential registers at once by
     * simply making outbuf bigger.
     */
    outbuf[1] = value;
 
    /* Transfer the i2c packets to the kernel and verify it worked */
    packets.msgs  = messages;
    packets.nmsgs = 1;
    if(ioctl(fd, I2C_RDWR, &packets) < 0) {
        perror("Unable to send data");
        return -1;
    }
 
    return 0;
}

int I2c_write_reg_datas(int fd, unsigned char addr, unsigned char reg, unsigned char *value, int num)
{
    unsigned char				outbuf[2];
    struct i2c_rdwr_ioctl_data	packets;
    struct i2c_msg				messages[1];

    messages[0].addr  = addr;
    messages[0].flags = 0;
    messages[0].len   = sizeof(outbuf);
    messages[0].buf   = outbuf;

    packets.nmsgs = 1;
    packets.msgs  = messages;

    outbuf[0] = reg;
    for(int i=0; i<num; i++)
    {
        outbuf[1] = value[i];

        if(ioctl(fd, I2C_RDWR, &packets) < 0) {
            perror("Unable to send data");
            return -1;
        }
    }

    return 0;
}

int I2c_read_regs(int fd, unsigned char addr, unsigned char reg, unsigned char *val, int num)
{
    struct i2c_rdwr_ioctl_data	packets;
    struct i2c_msg				messages[2];

    messages[0].addr  = addr;
    messages[0].flags = 0;
    messages[0].len   = 1;
    messages[0].buf   = &reg;

    messages[1].addr  = addr;
    messages[1].flags = I2C_M_RD;
    messages[1].len   = num;
    messages[1].buf   = val;

    packets.nmsgs     = 2;
    packets.msgs      = messages;

    if(ioctl(fd, I2C_RDWR, &packets) < 0) {
        perror("Unable to send data");
        return -1;
    }

    return 0;
}

int I2c_read_regs_addr16(int fd, unsigned char addr, unsigned short reg, unsigned char *val, int num)
{
    struct i2c_rdwr_ioctl_data	packets;
    struct i2c_msg				messages[2];
    unsigned char				regAddr[2];

    regAddr[0] = (reg >> 8) & 0xFF;;
    regAddr[1] = reg & 0xFF;

    messages[0].addr  = addr;
    messages[0].flags = 0;
    messages[0].len   = 2;
    messages[0].buf   = regAddr;

    messages[1].addr  = addr;
    messages[1].flags = I2C_M_RD;
    messages[1].len   = num;
    messages[1].buf   = val;

    packets.nmsgs     = 2;
    packets.msgs      = messages;

    ioctl(fd, I2C_TIMEOUT, 2);
    ioctl(fd, I2C_RETRIES, 1);

    if(ioctl(fd, I2C_RDWR, &packets) < 0) {
        perror("Unable to send data");
        return -1;
    }

    return 0;
}
