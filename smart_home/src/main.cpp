#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <ctime>
#include "s1.h"
#include "s2.h"
#include "s7.h"
#include "s8.h"
#include "e1.h"
#include "e2.h"
#include "i2c.h"
#include "i2c_probe.h"
#include <pthread.h>
#include <sys/reboot.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// 网络状态标志
static bool network_available = true;
static pthread_t network_thread_id;
static pthread_mutex_t network_mutex = PTHREAD_MUTEX_INITIALIZER;

#define VOICE_HISTORY_FILE "./web/voice_history.json"
#define VOICE_RECORD_FILE "/userdata/voice_record.wav"
#define VOICE_TTS_XIAOMI_FILE "/userdata/tts_xiaomi.wav"
#define MAX_VOICE_HISTORY 10
#define ENV_HIST_SIZE 100   // 环境历史记录条数

typedef struct
{
	char role[16];
	char text[256];
	time_t timestamp;
} voice_history_item_t;

static voice_history_item_t voice_history[MAX_VOICE_HISTORY];
static int voice_history_count = 0;
static char last_recognized_text[256] = "";
static char last_nlp_intent[128] = "";

extern int key_idx;
extern int led_idx;
extern int tube_idx;
extern int bright_idx;
extern int human_idx;
extern int sht_idx;
extern s1_key_process *s1_key;

// ========== 全局状态变量 ==========
int fan_off = 0;					   // 风扇开关: 0关闭, 1开启
int lightness = 50;					   // E1亮度 (0-100)
int dang[] = {0, 20, 40, 60, 80, 100}; // 风扇档位
int dang_idx = 0;					   // 当前档位索引
int mode_display = 1;				   // 显示模式: 0关闭, 1温度, 2湿度, 3光照
int mode_fan = 0;					   // 风扇模式: 0手动, 1自动
int mode_background = 0;			   // 场景模式 (0工作,1休闲,2睡眠)
int mode_set = 0;					   // 设置模式: 0普通, 1设置
int temperature_set = 28;			   // 温度阈值(℃)
int humidity_set = 60;				   // 湿度阈值(%)
int light_set = 500;				   // 光照阈值(lx)
int voice_mode = 0;					   // 语音模式开关
int sleep_time = 1;					   // 休眠等待时间(分钟): 1/3/5/10
int light_mode = 0;					   // 0为手动模式 1为自动模式

int fan_was_on_before_idle = 0;	  // 记录进入待机前的风扇开关状态
int fan_dang_idx_before_idle = 0; // 记录进入待机前的风扇档位索引
int prev_mode_background = -1;	  // 上一次场景模式，用于检测切换并播报
int fan_manual_override = 0;	  // 用户是否手动控制过风扇（优先）

// 全局环境数据结构
typedef struct
{
	float temperature;
	float humidity;
	float light_intensity;
	int human_detected;
	time_t timestamp;
} env_data_t;

env_data_t env; // 全局变量

int e5_idle = 0;		  // E5 待机状态
int no_human_seconds = 0; // 无人计时
int e1_cycle_tick = 0;	  // E1 自动切换计数
int record_seconds = 3;	  // 录音时长
float env_hist_temp[ENV_HIST_SIZE] = {0};
float env_hist_hum[ENV_HIST_SIZE] = {0};
float env_hist_lux[ENV_HIST_SIZE] = {0};
int env_hist_count = 0;
int env_hist_pos = 0;
char alert_message[128] = "";
int e5_scene = 0;
int e5_voice_on = 1;
int last_alert_active = 0; // 上一次是否处于告警状态，用于防止重复播报


static pthread_t audio_thread_id;
static pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t audio_cond = PTHREAD_COND_INITIALIZER;
static char audio_path[512] = {0};
static int audio_play_requested = 0;

// ========== 函数声明 ==========
void print_system_status(void);
void update_e1_display(int fd, i2c_probe_target *list);
void update_e1_led(int fd, i2c_probe_target *list);
void smart_control(int fd, i2c_probe_target *list);
void handle_key_event(int fd, i2c_probe_target *list, int dev_idx, int key_num);

void update_e5_display(int fd, i2c_probe_target *list);
void process_e5_command(int fd, i2c_probe_target *list);
void request_play_audio(const char *path);
void request_record_audio(const char *outpath, int seconds);
int record_audio_sync(const char *outpath, int seconds);
int run_python_json(const char *cmd, char *output, size_t output_size);
void escape_shell_arg(const char *src, char *dst, size_t dst_size);
void log_voice_history(const char *role, const char *message);
void write_voice_history_file(void);
int parse_json_string(const char *json, const char *field, char *out, size_t out_size);
int xiaomi_tts_to_file(const char *text, const char *outpath);
void start_voice_interaction(int fd, i2c_probe_target *list);
void execute_voice_intent(int fd, i2c_probe_target *list, const char *intent, const char *slots, const char *original_text);
void tts_speak(const char *text);
void system_reboot(void);
bool is_network_available(void);

// 简单的录音请求（不队列化，直接 spawn）
void request_record_audio(const char *outpath, int seconds)
{
	if (!outpath)
		return;
	char cmd[1024];
	// 使用 tinycap 进行录音
	snprintf(cmd, sizeof(cmd), "tinycap %s -D 0 -d 0 -t %d -b 16 -r 16000 &", outpath, seconds);
	printf("[S3] 开始录音: %s (%ds)\n", outpath, seconds);
	system(cmd);
}

// 请求播放音频（非阻塞，通过后台线程执行）
void request_play_audio(const char *path)
{
	if (!path)
		return;
	pthread_mutex_lock(&audio_mutex);
	strncpy(audio_path, path, sizeof(audio_path) - 1);
	audio_play_requested = 1;
	pthread_cond_signal(&audio_cond);
	pthread_mutex_unlock(&audio_mutex);
}

// 后台音频线程：等待播放请求并调用 tinyplay
static void *audio_thread(void *arg)
{
	(void)arg;
	for (;;)
	{
		pthread_mutex_lock(&audio_mutex);
		while (!audio_play_requested)
			pthread_cond_wait(&audio_cond, &audio_mutex);

		char localpath[512] = {0};
		strncpy(localpath, audio_path, sizeof(localpath) - 1);
		audio_play_requested = 0;
		pthread_mutex_unlock(&audio_mutex);

		if (localpath[0])
		{
			char cmd[1024];
			snprintf(cmd, sizeof(cmd), "aplay -D hw:0,1 %s", localpath);
			printf("[E4] 播放音频: %s\n", localpath);
			system(cmd);
			printf("[E4] 播放完成: %s\n", localpath);
		}
	}
	return NULL;
}

// 文本到语音：优先使用小米 TTS，失败后退回本地 TTS 命令生成 WAV 并播放
void tts_speak(const char *text) {
    if (!text || text[0] == '\0') return;

    // 网络不可用时，直接走本地 TTS
    if (!is_network_available()) {
        const char *out = "/userdata/tts_announce.wav";
        char cmd[1024];
        if (access("/usr/bin/pico2wave", X_OK) == 0) {
            snprintf(cmd, sizeof(cmd), "/usr/bin/pico2wave -l=zh-CN -w=%s \"%s\"", out, text);
            system(cmd);
            request_play_audio(out);
        } else if (access("/usr/bin/espeak-ng", X_OK) == 0) {
            snprintf(cmd, sizeof(cmd), "/usr/bin/espeak-ng -v zh -w %s \"%s\"", out, text);
            system(cmd);
            request_play_audio(out);
        } else if (access("/usr/bin/espeak", X_OK) == 0) {
            snprintf(cmd, sizeof(cmd), "/usr/bin/espeak -v zh -w %s \"%s\"", out, text);
            system(cmd);
            request_play_audio(out);
        } else if (access("/usr/bin/flite", X_OK) == 0) {
            snprintf(cmd, sizeof(cmd), "/usr/bin/flite -t \"%s\" -o %s", text, out);
            system(cmd);
            request_play_audio(out);
        } else {
            printf("[TTS] 离线且无本地 TTS 引擎，仅打印：%s\n", text);
        }
        return;
    }

    // 网络在线时，优先小米云端 TTS，失败后降级本地
    if (xiaomi_tts_to_file(text, VOICE_TTS_XIAOMI_FILE)) {
        request_play_audio(VOICE_TTS_XIAOMI_FILE);
        return;
    }

    // 本地降级
    const char *out = "/userdata/tts_announce.wav";
    char cmd[1024];
    if (access("/usr/bin/pico2wave", X_OK) == 0) {
        snprintf(cmd, sizeof(cmd), "/usr/bin/pico2wave -l=zh-CN -w=%s \"%s\"", out, text);
        system(cmd);
        request_play_audio(out);
    } else if (access("/usr/bin/espeak-ng", X_OK) == 0) {
        snprintf(cmd, sizeof(cmd), "/usr/bin/espeak-ng -v zh -w %s \"%s\"", out, text);
        system(cmd);
        request_play_audio(out);
    } else if (access("/usr/bin/espeak", X_OK) == 0) {
        snprintf(cmd, sizeof(cmd), "/usr/bin/espeak -v zh -w %s \"%s\"", out, text);
        system(cmd);
        request_play_audio(out);
    } else if (access("/usr/bin/flite", X_OK) == 0) {
        snprintf(cmd, sizeof(cmd), "/usr/bin/flite -t \"%s\" -o %s", text, out);
        system(cmd);
        request_play_audio(out);
    } else {
        printf("[TTS] 无可用 TTS 引擎，本地输出文本：%s\n", text);
    }
}

int record_audio_sync(const char *outpath, int seconds)
{
	if (!outpath)
		return -1;
	char cmd[1024];
	snprintf(cmd, sizeof(cmd), "tinycap %s -D 0 -d 0 -t %d -b 16 -r 16000", outpath, seconds);
	printf("[S3] 同步录音: %s\n", cmd);
	return system(cmd);
}

void escape_shell_arg(const char *src, char *dst, size_t dst_size)
{
	if (!src || !dst || dst_size == 0)
		return;

	size_t pos = 0;
	if (pos < dst_size - 1)
		dst[pos++] = '\'';

	while (*src && pos < dst_size - 4)
	{
		if (*src == '\'')
		{
			if (pos + 4 < dst_size)
			{
				dst[pos++] = '\'';
				dst[pos++] = '\\';
				dst[pos++] = '\'';
				dst[pos++] = '\'';
			}
			src++;
		}
		else
		{
			dst[pos++] = *src++;
		}
	}

	if (pos < dst_size - 1)
		dst[pos++] = '\'';
	dst[pos] = '\0';
}

void escape_json_string(const char *input, char *output, size_t size)
{
	if (!input || !output || size == 0)
		return;

	size_t pos = 0;
	while (*input && pos < size - 1)
	{
		char c = *input++;
		if (c == '"' || c == '\\' || c == '/')
		{
			if (pos + 2 >= size)
				break;
			output[pos++] = '\\';
			output[pos++] = c;
		}
		else if (c == '\b')
		{
			if (pos + 2 >= size)
				break;
			output[pos++] = '\\';
			output[pos++] = 'b';
		}
		else if (c == '\f')
		{
			if (pos + 2 >= size)
				break;
			output[pos++] = '\\';
			output[pos++] = 'f';
		}
		else if (c == '\n')
		{
			if (pos + 2 >= size)
				break;
			output[pos++] = '\\';
			output[pos++] = 'n';
		}
		else if (c == '\r')
		{
			if (pos + 2 >= size)
				break;
			output[pos++] = '\\';
			output[pos++] = 'r';
		}
		else if (c == '\t')
		{
			if (pos + 2 >= size)
				break;
			output[pos++] = '\\';
			output[pos++] = 't';
		}
		else
		{
			output[pos++] = c;
		}
	}
	output[pos] = '\0';
}

int run_python_json(const char *cmd, char *output, size_t output_size)
{
	if (!cmd || !output || output_size == 0)
		return -1;

	FILE *fp = popen(cmd, "r");
	if (!fp)
		return -1;

	size_t total = 0;
	char buffer[512];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		size_t len = strnlen(buffer, sizeof(buffer));
		if (total + len < output_size - 1)
		{
			memcpy(output + total, buffer, len);
			total += len;
		}
		else
		{
			break;
		}
	}
	output[total] = '\0';
	int status = pclose(fp);
	return status == 0 ? 0 : -1;
}

int parse_json_string(const char *json, const char *field, char *out, size_t out_size)
{
	if (!json || !field || !out || out_size == 0)
		return 0;

	char key[128];
	snprintf(key, sizeof(key), "\"%s\"", field);
	const char *pos = strstr(json, key);
	if (!pos)
		return 0;

	pos = strchr(pos + strlen(key), ':');
	if (!pos)
		return 0;
	pos++;
	while (*pos && isspace((unsigned char)*pos))
		pos++;
	if (*pos != '\"')
		return 0;
	pos++;

	size_t i = 0;
	while (*pos && *pos != '\"' && i < out_size - 1)
	{
		if (*pos == '\\' && *(pos + 1))
			pos++;
		out[i++] = *pos++;
	}
	out[i] = '\0';
	return 1;
}

void write_voice_history_file(void)
{
	FILE *f = fopen(VOICE_HISTORY_FILE, "w");
	if (!f)
		return;

	fprintf(f, "[\n");
	char role_escaped[256];
	char text_escaped[512];
	for (int i = 0; i < voice_history_count; i++)
	{
		escape_json_string(voice_history[i].role, role_escaped, sizeof(role_escaped));
		escape_json_string(voice_history[i].text, text_escaped, sizeof(text_escaped));
		fprintf(f, "  {\"role\":\"%s\",\"text\":\"%s\",\"timestamp\":%ld}%s\n",
				role_escaped,
				text_escaped,
				(long)voice_history[i].timestamp,
				i == voice_history_count - 1 ? "" : ",");
	}
	fprintf(f, "]\n");
	fclose(f);
}

void log_voice_history(const char *role, const char *message)
{
	if (!role || !message)
		return;

	if (voice_history_count == MAX_VOICE_HISTORY)
	{
		memmove(voice_history, voice_history + 1, sizeof(voice_history_item_t) * (MAX_VOICE_HISTORY - 1));
		voice_history_count--;
	}

	voice_history_item_t *entry = &voice_history[voice_history_count++];
	strncpy(entry->role, role, sizeof(entry->role) - 1);
	entry->role[sizeof(entry->role) - 1] = '\0';
	strncpy(entry->text, message, sizeof(entry->text) - 1);
	entry->text[sizeof(entry->text) - 1] = '\0';
	entry->timestamp = time(NULL);
	write_voice_history_file();
}

int xiaomi_tts_to_file(const char *text, const char *outpath)
{
	if (!text || !outpath)
		return 0;

	char escaped[1024];
	escape_shell_arg(text, escaped, sizeof(escaped));
	char cmd[2048];
	snprintf(cmd, sizeof(cmd), "python3 ./web/xiaomi_ai.py tts %s %s", escaped, outpath);

	char result[4096] = {0};
	if (run_python_json(cmd, result, sizeof(result)) != 0)
		return 0;

	char status[64] = {0};
	if (!parse_json_string(result, "status", status, sizeof(status)))
		return 0;

	return strcmp(status, "ok") == 0;
}

typedef struct {
    const char* keyword;
    const char* intent;
    const char* slots;
} offline_intent_t;

static void offline_nlp(const char* text, char* out_intent, size_t intent_size, char* out_slots, size_t slots_size) {
    out_intent[0] = '\0';
    out_slots[0] = '\0';
    if (!text) return;

    char lower[256];
    strncpy(lower, text, sizeof(lower)-1);
    lower[sizeof(lower)-1] = '\0';
    for (char *p = lower; *p; p++) *p = tolower(*p);

    offline_intent_t table[] = {
        {"打开风扇", "fan_on", ""}, {"开风扇", "fan_on", ""},
        {"关闭风扇", "fan_off", ""}, {"关风扇", "fan_off", ""},
        {"风扇打开", "fan_on", ""}, {"风扇关闭", "fan_off", ""},
        {"温度", "announce_temperature", ""}, {"湿度", "announce_humidity", ""},
        {"光照", "announce_lightness", ""}, {"显示模式", "switch_display", ""},
        {"切换显示", "switch_display", ""}, {"开灯", "light_on", ""},
        {"打开灯", "light_on", ""}, {"关灯", "light_off", ""},
        {"关闭灯", "light_off", ""}, {"场景模式", "background_change", ""},
        {"工作模式", "background_change", ""}, {"休闲模式", "background_change", ""},
        {"睡眠模式", "background_change", ""}
    };

    for (size_t i = 0; i < sizeof(table)/sizeof(offline_intent_t); i++) {
        if (strstr(lower, table[i].keyword)) {
            strncpy(out_intent, table[i].intent, intent_size-1);
            strncpy(out_slots, table[i].slots, slots_size-1);
            return;
        }
    }

    char* speed_pos = strstr(lower, "风扇");
    if (speed_pos) {
        char* num_pos = NULL;
        for (char* p = speed_pos; *p; p++) {
            if (*p >= '0' && *p <= '9') { num_pos = p; break; }
        }
        if (num_pos) {
            int speed = atoi(num_pos);
            if (speed >= 0 && speed <= 100) {
                snprintf(out_intent, intent_size, "fan_speed");
                snprintf(out_slots, slots_size, "%d", speed);
                return;
            }
        }
        if (strstr(speed_pos, "高速") || strstr(speed_pos, "大风")) {
            snprintf(out_intent, intent_size, "fan_speed");
            snprintf(out_slots, slots_size, "80");
            return;
        }
        if (strstr(speed_pos, "中速")) {
            snprintf(out_intent, intent_size, "fan_speed");
            snprintf(out_slots, slots_size, "50");
            return;
        }
        if (strstr(speed_pos, "低速") || strstr(speed_pos, "小风")) {
            snprintf(out_intent, intent_size, "fan_speed");
            snprintf(out_slots, slots_size, "20");
            return;
        }
    }

    if (strstr(lower, "亮") && strstr(lower, "灯")) {
        snprintf(out_intent, intent_size, "light_brightness");
        snprintf(out_slots, slots_size, "80");
        return;
    }
    if (strstr(lower, "暗") && strstr(lower, "灯")) {
        snprintf(out_intent, intent_size, "light_brightness");
        snprintf(out_slots, slots_size, "20");
        return;
    }
}

void execute_voice_intent(int fd, i2c_probe_target *list, const char *intent, const char *slots, const char *original_text)
{
    char feedback[256] = "抱歉，我没有理解您的指令。";
    char fallback_intent[128] = "";
    const char *active_intent = intent;

    // 如果云端没有返回有效 intent，尝试简单关键词匹配
    if (!intent || intent[0] == '\0')
    {
        if (original_text && strstr(original_text, "风扇"))
        {
            if (strstr(original_text, "开") || strstr(original_text, "打开"))
            {
                strncpy(fallback_intent, "fan_on", sizeof(fallback_intent) - 1);
            }
            else if (strstr(original_text, "关") || strstr(original_text, "关闭"))
            {
                strncpy(fallback_intent, "fan_off", sizeof(fallback_intent) - 1);
            }
        }
        else if (original_text && (strstr(original_text, "灯") || strstr(original_text, "灯光")))
        {
            if (strstr(original_text, "开") || strstr(original_text, "打开"))
            {
                strncpy(fallback_intent, "light_on", sizeof(fallback_intent) - 1);
            }
            else if (strstr(original_text, "关") || strstr(original_text, "关闭"))
            {
                strncpy(fallback_intent, "light_off", sizeof(fallback_intent) - 1);
            }
        }
    }

    if ((!active_intent || active_intent[0] == '\0') && fallback_intent[0])
        active_intent = fallback_intent;

    printf("[语音] intent=%s slots=%s 原始=%s\n",
           active_intent ? active_intent : "<none>",
           slots ? slots : "<none>",
           original_text ? original_text : "<none>");

    // ==================== 风扇控制 ====================
    if (active_intent && strcmp(active_intent, "fan_on") == 0)
    {
        if (dang_idx == 0)
            dang_idx = 1;               // 默认20%
        e2_speed_control_all(fd, list, dang[dang_idx]);
        fan_off = 1;
        fan_manual_override = 1;
        snprintf(feedback, sizeof(feedback), "已打开风扇，当前风速 %d%%。", dang[dang_idx]);
    }
    else if (active_intent && strcmp(active_intent, "fan_off") == 0)
    {
        e2_speed_control_all(fd, list, 0);
        fan_off = 0;
        fan_manual_override = 1;
        snprintf(feedback, sizeof(feedback), "已关闭风扇。");
    }
    else if (active_intent && strcmp(active_intent, "fan_speed") == 0)
    {
        int speed = 50;                // 默认中速
        if (slots && strlen(slots) > 0)
        {
            speed = atoi(slots);
            if (speed < 0) speed = 0;
            if (speed > 100) speed = 100;
        }
        // 找到最接近的预设档位 (dang[] = {0,20,40,60,80,100})
        int best_idx = 0;
        int best_diff = abs(dang[0] - speed);
        for (int i = 1; i < 6; i++)
        {
            int diff = abs(dang[i] - speed);
            if (diff < best_diff)
            {
                best_diff = diff;
                best_idx = i;
            }
        }
        dang_idx = best_idx;
        e2_speed_control_all(fd, list, dang[dang_idx]);
        fan_off = (dang[dang_idx] > 0) ? 1 : 0;
        snprintf(feedback, sizeof(feedback), "风扇速度已设为 %d%%。", dang[dang_idx]);
        fan_manual_override = 1;
    }

    // ==================== 灯光控制 ====================
    else if (active_intent && strcmp(active_intent, "light_on") == 0)
    {
        // 如果数码管显示关闭，则开启显示模式
        if (mode_display == 0)
            mode_display = 1;
        // 如果当前亮度为0，恢复默认亮度50
        if (lightness == 0)
            lightness = 50;
        update_e1_led(fd, list);       // 刷新灯光
        snprintf(feedback, sizeof(feedback), "灯已打开。");
        // 清除手动覆盖标志？保持用户设置的颜色/亮度，不清除
    }
    else if (active_intent && strcmp(active_intent, "light_off") == 0)
    {
        e1_led_off_all(fd, list);
        e1_digital_display_off_all(fd, list);
        mode_display = 0;
        lightness = 0;                 // 记录亮度为0，便于语音开灯时恢复默认
        snprintf(feedback, sizeof(feedback), "灯已关闭。");
    }
    else if (active_intent && strcmp(active_intent, "light_brightness") == 0)
    {
        int val = 50;
		if (slots && strlen(slots) > 0)
        {
            val = atoi(slots);
            if (val < 0) val = 0;
            if (val > 100) val = 100;
        }
        lightness = val;
        update_e1_led(fd, list);
        snprintf(feedback, sizeof(feedback), "灯光亮度已设为 %d%%。", lightness);
        // 可选：清除自动灯光模式
        light_mode = 0;
    }
    else if (active_intent && strcmp(active_intent, "light_color") == 0)
    {
        e1_led_color_t color = E1_COLOR_WHITE;
        if (slots)
        {
            if (strcmp(slots, "red") == 0) color = E1_COLOR_RED;
            else if (strcmp(slots, "green") == 0) color = E1_COLOR_GREEN;
            else if (strcmp(slots, "blue") == 0) color = E1_COLOR_BLUE;
            else if (strcmp(slots, "yellow") == 0) color = E1_COLOR_YELLOW;
            else if (strcmp(slots, "orange") == 0) color = E1_COLOR_ORANGE;
            else if (strcmp(slots, "purple") == 0) color = E1_COLOR_PURPLE;
            else if (strcmp(slots, "white") == 0) color = E1_COLOR_WHITE;
        }
        if (lightness == 0) lightness = 50;   // 如果当前灯是关的，开灯并设亮度
		light_mode = 0;                       // 切换为手动模式
        e1_led_ctrl_all(fd, list, color, lightness);
        snprintf(feedback, sizeof(feedback), "灯光颜色已设为%s。", slots ? slots : "白色");
       
    }

    // ==================== 环境信息播报 ====================
    else if (active_intent && strcmp(active_intent, "announce_temperature") == 0)
    {
        snprintf(feedback, sizeof(feedback), "当前温度 %.1f 摄氏度。", env.temperature);
    }
    else if (active_intent && strcmp(active_intent, "announce_humidity") == 0)
    {
        snprintf(feedback, sizeof(feedback), "当前湿度 百分之%.1f。", env.humidity);
    }
	else if (active_intent && strcmp(active_intent, "announce_lightness") == 0)
    {
        snprintf(feedback, sizeof(feedback), "当前光照 %.1f 勒克斯。", env.light_intensity);
    }
    else if (active_intent && strcmp(active_intent, "switch_display") == 0)
    {
        mode_display = (mode_display + 1) % 4;
        const char *mode_names[] = {"关闭", "温度", "湿度", "光照"};
        snprintf(feedback, sizeof(feedback), "已切换显示模式：%s。", mode_names[mode_display]);
    }
	// ==================== 场景模式切换 ====================
	else if (active_intent && strcmp(active_intent, "background_change") == 0)
    {
        mode_background = (mode_background + 1) % 3;
		fan_manual_override = 0; // 场景切换后恢复场景自动控制
		printf("场景模式: %d\n", mode_background);
		// 语音播报切换结果
		if (e5_voice_on)
		{
			switch (mode_background)
			{
			case 0:
				snprintf(feedback, sizeof(feedback), "已切换到工作模式");
				break;
			case 1:
				snprintf(feedback, sizeof(feedback), "已切换到休闲模式");
				break;
			case 2:
				snprintf(feedback, sizeof(feedback), "已切换到睡眠模式");
				break;
			default:
				snprintf(feedback, sizeof(feedback), "已切换到模式 %d", mode_background);
				break;
			}
			
		}
    }

	// ==================== 日期时间查询 ====================
    else if (active_intent && strcmp(active_intent, "query_date") == 0)
    {
        std::time_t t = std::time(nullptr);
        std::tm* now = std::localtime(&t);
        char date_str[64];
        std::strftime(date_str, sizeof(date_str), "今天是%Y年%m月%d日", now);
        snprintf(feedback, sizeof(feedback), "%s", date_str);
    }
    else if (active_intent && strcmp(active_intent, "query_time") == 0)
    {
        std::time_t t = std::time(nullptr);
        std::tm* now = std::localtime(&t);
        char time_str[64];
        std::strftime(time_str, sizeof(time_str), "%H点%M分%S秒", now);
        snprintf(feedback, sizeof(feedback), "现在时间 %s", time_str);
    }
	
	// ==================== 真实天气查询 ====================
    else if (active_intent && strcmp(active_intent, "real_weather") == 0)
    {
        char city[128] = {0};
        if (slots && slots[0] != '\0') {
            strncpy(city, slots, sizeof(city)-1);
        } else {
            // 默认城市，可从配置文件读取，简单写死或从环境变量获取
            strcpy(city, "天津");
        }
        
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "python3 ./web/xiaomi_ai.py real_weather %s", city);
        char result[4096] = {0};
        if (run_python_json(cmd, result, sizeof(result)) == 0) {
            char weather_text[256] = {0};
            if (parse_json_string(result, "text", weather_text, sizeof(weather_text))) {
                strncpy(feedback, weather_text, sizeof(feedback)-1);
            } else {
                // 真实天气获取失败，降级到原有 AI 生成的天气
                goto fallback_weather;
            }
        } else {
            fallback_weather:
            
            if (!is_network_available()) {
                strncpy(feedback, "网络未连接，无法查询天气。", sizeof(feedback)-1);
            } else {
                char result2[4096] = {0};
                if (run_python_json("python3 ./web/xiaomi_ai.py skill weather", result2, sizeof(result2)) == 0) {
                    char skill_text[256] = {0};
                    if (parse_json_string(result2, "text", skill_text, sizeof(skill_text)))
                        strncpy(feedback, skill_text, sizeof(feedback)-1);
                    else
                        strncpy(feedback, "获取天气信息失败，请稍后再试。", sizeof(feedback)-1);
                } else {
                    strncpy(feedback, "天气服务暂不可用。", sizeof(feedback)-1);
                }
            }
        }
    }

    // ==================== 技能（ AI ） ====================
    else if (active_intent && strcmp(active_intent, "weather_query") == 0)
    {
        if (!is_network_available()) {
            strncpy(feedback, "网络未连接，无法查询天气。", sizeof(feedback)-1);
        } else {
            char result[4096] = {0};
            if (run_python_json("python3 ./web/xiaomi_ai.py skill weather", result, sizeof(result)) == 0)
            {
                char skill_text[256] = {0};
                if (parse_json_string(result, "text", skill_text, sizeof(skill_text)))
                    strncpy(feedback, skill_text, sizeof(feedback) - 1);
                else
                    strncpy(feedback, "获取天气信息失败，请稍后再试。", sizeof(feedback) - 1);
            }
            else
            {
                strncpy(feedback, "天气服务暂不可用。", sizeof(feedback) - 1);
            }
        }
    }
    else if (active_intent && strcmp(active_intent, "schedule_query") == 0)
    {
        if (!is_network_available()) {
            strncpy(feedback, "网络未连接，无法获取日程。", sizeof(feedback)-1);
        } else {
            char result[4096] = {0};
            if (run_python_json("python3 ./web/xiaomi_ai.py skill schedule", result, sizeof(result)) == 0)
            {
                char skill_text[256] = {0};
                if (parse_json_string(result, "text", skill_text, sizeof(skill_text)))
                    strncpy(feedback, skill_text, sizeof(feedback) - 1);
                else
                    strncpy(feedback, "获取日程提醒失败，请稍后再试。", sizeof(feedback) - 1);
            }
            else
            {
                strncpy(feedback, "日程服务暂不可用。", sizeof(feedback) - 1);
            }
        }
    }

    // 记录历史并语音播报
    log_voice_history("assistant", feedback);
    tts_speak(feedback);
}

void start_voice_interaction(int fd, i2c_probe_target *list) {
    log_voice_history("system", "开始语音交互，正在唤醒...");
    tts_speak("请说出您的指令。");

    usleep(1500000);
    if (record_audio_sync(VOICE_RECORD_FILE, 3) != 0) {
        log_voice_history("assistant", "录音失败，请检查麦克风。");
        tts_speak("录音失败，请检查麦克风。");
        return;
    }

    log_voice_history("system", "语音采集完成，正在识别...");

    char recognized_text[256] = {0};
    int use_offline = 0;

    if (is_network_available()) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "python3 ./web/xiaomi_ai.py asr %s", VOICE_RECORD_FILE);
        char result[4096] = {0};
        if (run_python_json(cmd, result, sizeof(result)) == 0) {
            parse_json_string(result, "text", recognized_text, sizeof(recognized_text));
        } else {
            log_voice_history("assistant", "语音识别失败，将尝试离线匹配。");
            use_offline = 1;
        }
    } else {
        log_voice_history("system", "网络不可用，跳过云端 ASR，直接使用离线关键词匹配。");
        use_offline = 1;
    }

    if (use_offline || recognized_text[0] == '\0') {
        if (recognized_text[0] == '\0') {
            snprintf(recognized_text, sizeof(recognized_text), "无法识别语音（网络离线）");
        }
        char intent[128] = {0};
        char slots[256] = {0};
        offline_nlp(recognized_text, intent, sizeof(intent), slots, sizeof(slots));
        if (intent[0] != '\0') {
            execute_voice_intent(fd, list, intent, slots, recognized_text);
        } else {
            log_voice_history("assistant", "离线模式下无法理解指令。");
            tts_speak("网络不可用，且无法理解您的指令。");
        }
        return;
    }

    strncpy(last_recognized_text, recognized_text, sizeof(last_recognized_text)-1);
    log_voice_history("user", recognized_text);

    char escaped[1024];
    escape_shell_arg(recognized_text, escaped, sizeof(escaped));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "python3 ./web/xiaomi_ai.py nlp %s", escaped);
    char nlp_result[4096] = {0};

    if (run_python_json(cmd, nlp_result, sizeof(nlp_result)) != 0) {
        char intent[128] = {0};
        char slots[256] = {0};
        offline_nlp(recognized_text, intent, sizeof(intent), slots, sizeof(slots));
        if (intent[0] != '\0') {
            execute_voice_intent(fd, list, intent, slots, recognized_text);
        } else {
            log_voice_history("assistant", "指令解析失败，请重试。");
            tts_speak("指令解析失败，请重试。");
        }
        return;
    }

    char intent[128] = {0};
    char slots[256] = {0};
    parse_json_string(nlp_result, "intent", intent, sizeof(intent));
    parse_json_string(nlp_result, "slots", slots, sizeof(slots));
    if (intent[0] != '\0')
        strncpy(last_nlp_intent, intent, sizeof(last_nlp_intent)-1);

    execute_voice_intent(fd, list, intent, slots, recognized_text);
}

// ========== 函数实现 ==========
void print_system_status(void)
{
	printf("\n========== 系统状态 ==========\n");
	printf("温度: %.1f ℃  湿度: %.1f %%  光照: %.0f lx\n",
		   env.temperature, env.humidity, env.light_intensity);
	printf("人体: %s  风扇: %s (档位: %d%%)  亮度: %d%%  无人计时: %ds\n",
		   env.human_detected ? "有人" : "无人",
		   fan_off ? "开启" : "关闭", dang[dang_idx], lightness, no_human_seconds / 20);
	printf("显示模式: ");
	switch (mode_display)
	{
	case 0:
		printf("关闭");
		break;
	case 1:
		printf("温度");
		break;
	case 2:
		printf("湿度");
		break;
	case 3:
		printf("光照");
		break;
	}
	printf("  场景模式: %d  设置模式: %s\n", mode_background, mode_set ? "开启" : "关闭");
	printf("阈值: 温度%d℃ 湿度%d%% 光照%dlx  休眠时间:%d分钟\n",
		   temperature_set, humidity_set, light_set, sleep_time);
	printf("==============================\n\n");
}

void update_e1_display(int fd, i2c_probe_target *list)
{
	// 睡眠或待机时数码管始终熄灭
	if (tube_idx < 0 || mode_display == 0 || mode_background == 2 || e5_idle)
	{
		mode_display = 0;
		if (tube_idx >= 0)
			e1_digital_display_off_all(fd, list);
		return;
	}
	for (int i = 0; i < list[tube_idx].detected_count; i++)
	{
		unsigned char addr = list[tube_idx].detected_addrs[i];
		int val;
		if (mode_display == 1)
		{ // 温度
			val = (int)(env.temperature);
			if (val < 0)
			{
				e1_digital_bit_display_char(fd, addr, 0, '-');
				val = -val;
			}
			else
			{
				e1_digital_bit_display_char(fd, addr, 0, ' ');
			}
			e1_digital_display(fd, addr,
							   val / 1000, (val % 1000) / 100,
							   (val % 100) / 10, val % 10);
		}
		else if (mode_display == 2)
		{ // 湿度
			val = (int)(env.humidity);
			e1_digital_display(fd, addr,
							   val / 1000, (val % 1000) / 100,
							   (val % 100) / 10, val % 10);
		}
		else
		{ // 光照
			val = (int)env.light_intensity;
			if (val > 9999)
				val = 9999;
			e1_digital_display(fd, addr,
							   val / 1000, (val % 1000) / 100,
							   (val % 100) / 10, val % 10);
		}
	}
}

void update_e1_led(int fd, i2c_probe_target *list)
{
	if (led_idx < 0)
		return;
	if (e5_idle)
	{
		e1_led_off_all(fd, list);
		return;
	}
	// 模式化控制：工作(0)、休闲(1)、睡眠(2)
	if (mode_background == 2)
	{
		// 睡眠模式：关闭指示灯
		e1_led_off_all(fd, list);
		return;
	}
	else if (mode_background == 0)
	{
		// 工作模式：白色、亮度50%
		// lightness=50;
		e1_led_ctrl_all(fd, list, E1_COLOR_WHITE, lightness);
		return;
	}
	else if (mode_background == 1)
	{
		// 休闲模式：暖黄色 (R=255,G=200,B=100)，亮度50%
		// lightness=50;
		e1_led_ctrl_all(fd, list, E1_COLOR_ORANGE, lightness);
		return;
	}
	// 其他模式回退到原先基于环境颜色控制
	e1_led_color_t color = E1_COLOR_GREEN;
	if (!env.human_detected)
		color = E1_COLOR_BLUE;
	else if (env.temperature > temperature_set + 3 || env.humidity > humidity_set + 20)
		color = E1_COLOR_RED;
	else if (env.temperature > temperature_set || env.humidity > humidity_set)
		color = E1_COLOR_YELLOW;
	else
		color = E1_COLOR_GREEN;
	e1_led_ctrl_all(fd, list, color, lightness);
}

// 将 E1 的摘要信息同步到 E5（触控屏）显示
void update_e5_display(int fd, i2c_probe_target *list)
{
	// ========== 历史数据记录：改为每 1 秒记录一次 ==========
    static int record_counter = 0;        // 静态计数器，记录调用次数
    record_counter++;
    if (record_counter >= 20)             // 20 × 50ms = 1000ms = 1秒
    {
        record_counter = 0;               // 重置计数器
        if (env_hist_count < ENV_HIST_SIZE)
            env_hist_count++;
        env_hist_temp[env_hist_pos] = env.temperature;
        env_hist_hum[env_hist_pos] = env.humidity;
        env_hist_lux[env_hist_pos] = env.light_intensity;
        env_hist_pos = (env_hist_pos + 1) % ENV_HIST_SIZE;
    }

	// 人体超时计数（函数被主循环50ms调用一次）
	const int TICKS_PER_SEC = 20;	  // 1000ms / 50ms
	const int THRESH_WORK = 180 * 20; // 3分钟
	const int THRESH_REST = 300 * 20; // 5分钟
	const int THRESH_SLEEP = 60 * 20; // 1分钟

	if (!env.human_detected)
		no_human_seconds += 3;
	else
		no_human_seconds = 0;

	int thresh = THRESH_WORK;
	if (mode_background == 1)
		thresh = THRESH_REST;
	else if (mode_background == 2)
		thresh = THRESH_SLEEP;

	// 进入待机
	if (!e5_idle && no_human_seconds >= thresh)
	{
		prev_mode_background = mode_background;
		fan_was_on_before_idle = fan_off;
		fan_dang_idx_before_idle = dang_idx;

		e5_idle = 1;
		mode_display = 0;

		e1_digital_display_off_all(fd, list);
		e1_led_off_all(fd, list);
		e2_speed_control_all(fd, list, 0);
		fan_off = 0;
		fan_manual_override = 0;
		printf("[无人待机] 进入待机，场景 %d, E1 熄屏, 风扇已关闭\n", prev_mode_background);
	}

	// 唤醒
	if (e5_idle && env.human_detected)
	{
		e5_idle = 0;
		if (prev_mode_background == 2)
		{
			mode_background = 1;	 // 睡眠唤醒进入休闲模式
			fan_manual_override = 0; // 唤醒后恢复休闲模式自动控制
			if (e5_voice_on)
				tts_speak("已切换到休闲模式");
		}
		else
		{
			if (fan_was_on_before_idle)
			{
				dang_idx = fan_dang_idx_before_idle;
				e2_speed_control_all(fd, list, dang[dang_idx]);
				fan_off = 1;
				printf("[有人唤醒] 恢复风扇档位 %d%%\n", dang[dang_idx]);
			}
			else
			{
				printf("[有人唤醒] 风扇保持关闭\n");
			}
		}
		mode_display = 1;
	}

	// E1 自动循环显示（每3秒切换一次温度/湿度/光照）
	if (!mode_set && !e5_idle)
	{
		e1_cycle_tick++;
		if (e1_cycle_tick >= 60)
		{
			e1_cycle_tick = 0;
			if (mode_display == 0)
				mode_display = 1;
			else
				mode_display = (mode_display % 3) + 1;
		}
	}

	// 告警判断（场景差异化）
	int alert_active = 0;
	if (mode_background == 0)
	{
		if (env.temperature > 30.0f || env.humidity > 70.0f || env.light_intensity < 200.0f || env.light_intensity > 600.0f)
			alert_active = 1;
	}
	else if (mode_background == 1)
	{
		if (env.temperature > 32.0f || env.humidity > 80.0f)
			alert_active = 1;
	}
	else
	{
		alert_active = 0;
	}
	if (alert_active && !last_alert_active && e5_voice_on && mode_background == 0 && !e5_idle)
	{
		char tbuf[256];
		snprintf(tbuf, sizeof(tbuf), "环境告警: 温度 %.1f摄氏度 湿度 百分之%.1f  光照 %.0f勒克斯", env.temperature, env.humidity, env.light_intensity);
		tts_speak(tbuf);
	}
	last_alert_active = alert_active;

	// 写 e5 状态 JSON
	FILE *f = fopen("./web/e5_status.json", "w");
	if (f)
	{
		int start = (env_hist_pos + ENV_HIST_SIZE - env_hist_count) % ENV_HIST_SIZE;
		fprintf(f, "{\n");
		fprintf(f, "  \"temperature\": %.1f,\n", env.temperature);
		fprintf(f, "  \"humidity\": %.1f,\n", env.humidity);
		fprintf(f, "  \"light\": %.1f,\n", env.light_intensity);
		fprintf(f, "  \"human\": %d,\n", env.human_detected);
		fprintf(f, "  \"fan_on\": %d,\n", fan_off);
		fprintf(f, "  \"fan_level\": %d,\n", dang[dang_idx]);
		fprintf(f, "  \"temperature_set\": %d,\n", temperature_set);
		fprintf(f, "  \"humidity_set\": %d,\n", humidity_set);
		fprintf(f, "  \"light_set\": %d,\n", light_set);
		fprintf(f, "  \"scene\": %d,\n", mode_background);
		fprintf(f, "  \"voice_on\": %d,\n", e5_voice_on);
		fprintf(f, "  \"sleep_time\": %d,\n", sleep_time);
		fprintf(f, "  \"display_mode\": %d,\n", mode_display);
		fprintf(f, "  \"brightness\": %d,\n", lightness);
		fprintf(f, "  \"fan_mode\": %d,\n", mode_fan);
		fprintf(f, "  \"light_mode\": %d,\n", light_mode);
		fprintf(f, "  \"idle\": %d,\n", e5_idle);
		fprintf(f, "  \"network\": %d,\n", is_network_available() ? 1 : 0);
		fprintf(f, "  \"alert\": \"%s\",\n", alert_active ? "异常" : "正常");
		char voice_role[256];
		char voice_text[512];
		escape_json_string(last_recognized_text, voice_text, sizeof(voice_text));
		fprintf(f, "  \"last_recognized_text\": \"%s\",\n", voice_text);
		escape_json_string(last_nlp_intent, voice_role, sizeof(voice_role));
		fprintf(f, "  \"last_nlp_intent\": \"%s\",\n", voice_role);
		fprintf(f, "  \"voice_history\": [\n");
		for (int i = 0; i < voice_history_count; i++)
		{
			escape_json_string(voice_history[i].role, voice_role, sizeof(voice_role));
			escape_json_string(voice_history[i].text, voice_text, sizeof(voice_text));
			fprintf(f, "    {\"role\":\"%s\",\"text\":\"%s\"}%s\n",
					voice_role,
					voice_text,
					i == voice_history_count - 1 ? "" : ",");
		}
		fprintf(f, "  ],\n");
		fprintf(f, "  \"history\": {\n");
		fprintf(f, "    \"temp\": [");
		for (int i = 0, idx = start; i < env_hist_count; i++, idx = (idx + 1) % ENV_HIST_SIZE)
		{
			fprintf(f, "%s%.1f", i ? ", " : "", env_hist_temp[idx]);
		}
		fprintf(f, "],\n");
		fprintf(f, "    \"hum\": [");
		for (int i = 0, idx = start; i < env_hist_count; i++, idx = (idx + 1) % ENV_HIST_SIZE)
		{
			fprintf(f, "%s%.1f", i ? ", " : "", env_hist_hum[idx]);
		}
		fprintf(f, "],\n");
		fprintf(f, "    \"lux\": [");
		for (int i = 0, idx = start; i < env_hist_count; i++, idx = (idx + 1) % ENV_HIST_SIZE)
		{
			fprintf(f, "%s%.1f", i ? ", " : "", env_hist_lux[idx]);
		}
		fprintf(f, "]\n");
		fprintf(f, "  }\n");
		fprintf(f, "}\n");
		fclose(f);
	}

	// 简单的UI文本同步，供旧的前端快速读取
	FILE *fu = fopen("./web/e5_ui.txt", "w");
	if (fu)
	{
		fprintf(fu, "scene=%d\nvoice=%d\nfan_on=%d\nfan_level=%d\n", mode_background, e5_voice_on, fan_off, dang[dang_idx]);
		fclose(fu);
	}
}

// 智能联动控制
void smart_control(int fd, i2c_probe_target *list)
{
	if (e5_idle)
	{
		e2_speed_control_all(fd, list, 0);
		fan_off = 0;
		return;
	}
	// 场景化风扇控制
	if (mode_background == 0)
	{
		// 工作模式：启用自动风扇，根据温度分档
		mode_fan = 1;
		int desired = 0;
		if (env.temperature < 26.0f)
			desired = 0;
		else if (env.temperature < 28.0f)
			desired = 40;
		else if (env.temperature < 30.0f)
			desired = 60;
		else
			desired = 80;
		if (!fan_manual_override)
		{
			if (desired == 0)
			{
				if (fan_off)
				{
					e2_speed_control_all(fd, list, 0);
					fan_off = 0;
				}
			}
			else
			{
				// 找到最接近的档位
				int best = 0, bestdiff = 10000;
				for (int i = 0; i < 6; i++)
				{
					int diff = abs(dang[i] - desired);
					if (diff == 0)
					{
						best = i;
						break;
					}
				}
				dang_idx = best;
				e2_speed_control_all(fd, list, desired);
				fan_off = 1;
			}
		}
	}
	else if (mode_background == 1)
	{
		// 休闲模式：手动优先，默认维持低档20%
		mode_fan = 0;
		if (!fan_manual_override)
		{
			// 设置为静音档 20%
			int desired = 20;
			int best = 0, bestdiff = 10000;
			for (int i = 0; i < 6; i++)
			{
				int diff = abs(dang[i] - desired);
				if (diff == 0)
				{
					best = i;
					break;
				}
			}
			dang_idx = best;
			e2_speed_control_all(fd, list, desired);
			fan_off = 1;
		}
	}
	else if (mode_background == 2)
	{
		// 睡眠模式：默认关闭风扇，但若温度过高 (>35) 则强制开启
		if (env.temperature > 35.0f)
		{
			// 强制打开（60%）
			int desired = 60;
			int best = 0, bestdiff = 10000;
			for (int i = 0; i < 6; i++)
			{
				int diff = abs(dang[i] - desired);
				if (diff == 0)
				{
					best = i;
					break;
				}
			}
			dang_idx = best;
			e2_speed_control_all(fd, list, desired);
			fan_off = 1;
		}
		else
		{
			e2_speed_control_all(fd, list, 0);
			fan_off = 0;
		}
	}

	// 自动调节E1亮度(基于光照)（仅在灯光自动模式下生效）
	if (light_mode == 1)
	{
		if (env.light_intensity < 100)
			lightness = 20;
		else if (env.light_intensity > 1000)
			lightness = 100;
		else
			lightness = 20 + (int)((env.light_intensity - 100) * 80 / 900);
	}
}

// 处理来自触控屏的命令（由 web/server.py 写入 web/e5_command.txt ）
void process_e5_command(int fd, i2c_probe_target *list)
{
	FILE *fcmd = fopen("./web/e5_command.txt", "r");
	if (!fcmd)
		return;

	char buf[1024];
	while (fgets(buf, sizeof(buf), fcmd))
	{
		// 格式: key=action&value=xxx 或简短的 token 解析
		char *tok = strtok(buf, "=&\n\r ");
		if (!tok)
			continue;
		char *action = NULL;
		char *value = NULL;
		// 简单解析两级命令
		if (strcmp(tok, "fan") == 0)
		{
			action = strtok(NULL, "=&\n\r ");
			value = strtok(NULL, "=&\n\r ");
			
			if (action)
			{
				if (strcmp(action, "on") == 0)
				{
					if (dang_idx == 0) dang_idx = 1;
					e2_speed_control_all(fd, list, dang[dang_idx]);
					fan_off = 1;
					printf("[E5] 风扇 开 (档位:%d%%)\n", dang[dang_idx]);
				}
				else if (strcmp(action, "off") == 0)
				{
					e2_speed_control_all(fd, list, 0);
					fan_off = 0;
					printf("[E5] 风扇 关\n");
				}
				else if (strcmp(action, "set") == 0)
				{
					int speed = 20; // 默认
					
					if (value && strcmp(value, "value") == 0)
					{
						char *speed_str = strtok(NULL, "=&\n\r ");
						if (speed_str) speed = atoi(speed_str);
					}
					// 兼容直接写数字的情况（"fan=set&60"）
					else if (value)
					{
						speed = atoi(value);
					}
					
					if (speed < 0) speed = 0;
					if (speed > 100) speed = 100;
					
					if (speed == 0)
					{
						e2_speed_control_all(fd, list, 0);
						fan_off = 0;
						printf("[E5] 风扇关闭 (0%%)\n");
					}
					else
					{
						// 找到最接近的预设档位
						int best_idx = 1;
						int best_diff = abs(dang[1] - speed);
						for (int i = 2; i < 6; i++)
						{
							int diff = abs(dang[i] - speed);
							if (diff < best_diff)
							{
								best_diff = diff;
								best_idx = i;
							}
						}
						dang_idx = best_idx;
						e2_speed_control_all(fd, list, dang[dang_idx]);
						fan_off = 1;
						printf("[E5] 风扇 档位设置 %d%% \n", speed, dang[dang_idx]);
					}
					fan_manual_override = 1;
				}
			}
		}

		else if (strcmp(tok, "scene") == 0)
		{
			action = strtok(NULL, "=&\n\r ");
			if (action)
			{
				mode_background = atoi(action);
				fan_manual_override = 0; // 使用触控切换场景时恢复场景自动控制
				printf("[E5] 场景设置 %d\n", mode_background);
				if (e5_voice_on)
				{
					char msg[128];
					switch (mode_background)
					{
					case 0:
						snprintf(msg, sizeof(msg), "已切换到工作模式");
						break;
					case 1:
						snprintf(msg, sizeof(msg), "已切换到休闲模式");
						break;
					case 2:
						snprintf(msg, sizeof(msg), "已切换到睡眠模式");
						break;
					default:
						snprintf(msg, sizeof(msg), "已切换到模式 %d", mode_background);
						break;
					}
					// 睡眠模式禁止语音
					if (mode_background != 2)
						tts_speak(msg);
				}
			}
		}
		else if (strcmp(tok, "voice") == 0)
		{
			action = strtok(NULL, "=&\n\r ");
			if (action)
			{
				if (strcmp(action, "start") == 0)
				{
					printf("[E5] 语音交互触发\n");
					start_voice_interaction(fd, list);
				}
				else
				{
					e5_voice_on = (strcmp(action, "on") == 0);
					printf("[E5] 语音播报 %s\n", e5_voice_on ? "开启" : "关闭");
				}
			}
		}
		else if (strcmp(tok, "speak_status") == 0)
		{
			if (mode_background != 2 && e5_voice_on)
			{
				char tbuf[256];
				snprintf(tbuf, sizeof(tbuf), "当前温度 %.1f摄氏度，湿度 百分之%.1f ，光照 %.0f勒克斯", env.temperature, env.humidity, env.light_intensity);
				tts_speak(tbuf);
			}
		}
		else if (strcmp(tok, "threshold") == 0)
		{
			action = strtok(NULL, "=&\n\r ");
			value = strtok(NULL, "=&\n\r ");
			if (action && value)
			{
				if (strcmp(action, "temp") == 0)
					temperature_set = atoi(value);
				else if (strcmp(action, "hum") == 0)
					humidity_set = atoi(value);
				else if (strcmp(action, "light") == 0)
					light_set = atoi(value);
				printf("[E5] 阈值更新 temp=%d hum=%d light=%d\n", temperature_set, humidity_set, light_set);
			}
		}
		else if (strcmp(tok, "sleep") == 0)
		{
			action = strtok(NULL, "=&\n\r ");
			if (action)
			{
				sleep_time = atoi(action);
				printf("[E5] 休眠时间设置 %d 分钟\n", sleep_time);
			}
		}
		else if (strcmp(tok, "brightness") == 0)
		{
			action = strtok(NULL, "=&\n\r ");
			if (action)
			{
				int val = atoi(action);
				if (val < 0)
					val = 0;
				if (val > 100)
					val = 100;
				lightness = val;
				update_e1_led(fd, list);
				printf("[E5] RGB 亮度: %d%%\n", lightness);
			}
		}
		else if(strcmp(tok,"light_mode") == 0)
		{
			action = strtok(NULL, "=&\n\r ");
			if (action)
			{
				light_mode = atoi(action);
				printf("[E5] 灯光模式 %d\n", light_mode);
			}
		}
		else if(strcmp(tok,"fan_mode") == 0)
		{
			action = strtok(NULL, "=&\n\r ");
			if (action)
			{
				mode_fan = atoi(action);
				printf("[E5] 风扇模式 %d\n", mode_fan);
			}
		}
		else if (strcmp(tok, "display") == 0)
		{
			action = strtok(NULL, "=&\n\r ");
			if (action)
			{
				mode_display = atoi(action);
				printf("[E5] 显示模式 %d\n", mode_display);
			}
		}
	}

	fclose(fcmd);
	remove("./web/e5_command.txt");
}

// 按键事件处理（长按/短按）
void handle_key_event(int fd, i2c_probe_target *list, int dev_idx, int key_num)
{
	if (key_idx < 0)
		return;
	// 长按
	if (s1_key[dev_idx].SW_long[key_num])
	{
		s1_key[dev_idx].SW_long[key_num] = false;
		printf("[长按] 按键 %d\n", key_num);
		switch (key_num)
		{
		case SW1:
			mode_set = !mode_set;
			printf("设置模式: %s\n", mode_set ? "开启" : "关闭");
			break;
		case SW2:
			printf("[长按] 按键2：触发语音交互\n");
			start_voice_interaction(fd, list);
			break;
		case SW3: // 休眠时间+
			if (sleep_time == 1)
				sleep_time = 3;
			else if (sleep_time == 3)
				sleep_time = 5;
			else if (sleep_time == 5)
				sleep_time = 10;
			printf("休眠时间: %d 分钟\n", sleep_time);
			break;
		case SW4: // 休眠时间-
			if (sleep_time == 10)
				sleep_time = 5;
			else if (sleep_time == 5)
				sleep_time = 3;
			else if (sleep_time == 3)
				sleep_time = 1;
			printf("休眠时间: %d 分钟\n", sleep_time);
			break;
		case SW5:
			temperature_set++;
			printf("温度阈值: %d ℃\n", temperature_set);
			break;
		case SW6:
			temperature_set--;
			printf("温度阈值: %d ℃\n", temperature_set);
			break;
		case SW7:
			humidity_set++;
			printf("湿度阈值: %d %%\n", humidity_set);
			break;
		case SW8:
			humidity_set--;
			printf("湿度阈值: %d %%\n", humidity_set);
			break;
		case SW9:
			light_set += 50;
			printf("光照阈值: %d lx\n", light_set);
			break;
		case SWA:
			light_set = (light_set >= 50) ? light_set - 50 : 0;
			printf("光照阈值: %d lx\n", light_set);
			break;
		case SWC:
			break;
		}
	}
	// 短按
	if (s1_key[dev_idx].SW_short[key_num])
	{
		s1_key[dev_idx].SW_short[key_num] = false;
		printf("[短按] 按键 %d\n", key_num);
		switch (key_num)
		{
		case SW1: // 风扇开/关
			if (fan_off)
			{
				e2_speed_control_all(fd, list, 0);
				fan_off = 0;
				printf("风扇关闭\n");
			}
			else
			{
				if (dang_idx == 0)
					dang_idx = 1; // 默认20%
				e2_speed_control_all(fd, list, dang[dang_idx]);
				fan_off = 1;
				printf("风扇开启 (档位: %d%%)\n", dang[dang_idx]);
			}
			// 用户手动操作风扇，标记为手动覆盖
			fan_manual_override = 1;
			break;
		case SW2: // 风扇档位切换(低->中->高->自动)
			if (mode_fan == 1)
			{
				mode_fan = 0;
				light_mode = 0;
				printf("风扇模式: 手动\n");
			}
			else
			{
				if (dang_idx == 5)
				{
					mode_fan = 1;
					printf("风扇模式: 自动\n");
					light_mode = 1;
					printf("灯光模式: 自动\n");
				}
				dang_idx = (dang_idx + 1) % 6;
				if (fan_off)
					e2_speed_control_all(fd, list, dang[dang_idx]);
				printf("风扇档位: %d%%\n", dang[dang_idx]);
			}
			break;
		case SW3: // E1显示模式切换
			mode_display = (mode_display + 1) % 4;
			printf("0为关闭,1为显示温度,2为显示湿度,3为显示光照\n");
			printf("显示模式: %d\n", mode_display);
			break;
		case SW4: // 场景模式切换
			// 循环切换 0->1->2->0
			mode_background = (mode_background + 1) % 3;
			fan_manual_override = 0; // 场景切换后恢复场景自动控制
			printf("场景模式: %d\n", mode_background);
			// 语音播报切换结果
			if (e5_voice_on)
			{
				char msg[128];
				switch (mode_background)
				{
				case 0:
					snprintf(msg, sizeof(msg), "已切换到工作模式");
					break;
				case 1:
					snprintf(msg, sizeof(msg), "已切换到休闲模式");
					break;
				case 2:
					snprintf(msg, sizeof(msg), "已切换到睡眠模式");
					break;
				default:
					snprintf(msg, sizeof(msg), "已切换到模式 %d", mode_background);
					break;
				}
				tts_speak(msg);
			}
			break;
		case SW5: // 亮度+
			if (light_mode == 0)
			{
				lightness += 10;
				if (lightness > 100)
					lightness = 100;
				printf("亮度: %d%%\n", lightness);
			}
			break;
		case SW6: // 亮度-
			if (light_mode == 0)
			{
				lightness -= 10;
				if (lightness < 0)
					lightness = 0;
				printf("亮度: %d%%\n", lightness);
			}
			break;
		case SW7: // 风扇转速+
			if (fan_off && dang_idx < 5)
			{
				dang_idx++;
				e2_speed_control_all(fd, list, dang[dang_idx]);
				printf("风扇档位: %d%%\n", dang[dang_idx]);
			}
			fan_manual_override = 1;
			break;
		case SW8: // 风扇转速-
			if (fan_off && dang_idx > 0)
			{
				dang_idx--;
				e2_speed_control_all(fd, list, dang[dang_idx]);
				printf("风扇档位: %d%%\n", dang[dang_idx]);
			}
			fan_manual_override = 1;
			break;
		case SW9: // 语音播报当前环境状态
		{
			// 睡眠模式完全静音
			if (mode_background == 2)
				break;
			char tbuf[256];
			snprintf(tbuf, sizeof(tbuf), "当前温度 %.1f摄氏度，湿度 百分之%.1f ，光照 %.0f勒克斯", env.temperature, env.humidity, env.light_intensity);
			tts_speak(tbuf);
		}
		break;
		case SWA: // 屏幕亮/灭
			if (mode_display == 0)
				mode_display = 1;
			else
				mode_display = 0;
			printf("屏幕%s\n", mode_display ? "亮起" : "熄灭");
			break;
		case SWC: // 系统重启
			// 调用安全重启函数
			system_reboot();
			break;
		}
	}
}

// 简单的网络连通性检测：尝试连接 Google DNS (8.8.8.8:53)
static bool check_network_connectivity() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);

    int ret = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
    return (ret == 0);
}

// 后台网络检测线程，每30秒更新一次全局标志
static void* network_monitor_thread(void* arg) {
    (void)arg;
    while (1) {
        bool current = check_network_connectivity();
        pthread_mutex_lock(&network_mutex);
        network_available = current;
        pthread_mutex_unlock(&network_mutex);
        printf("[网络] %s\n", current ? "在线" : "离线");
        sleep(30);
    }
    return NULL;
}

bool is_network_available() {
    pthread_mutex_lock(&network_mutex);
    bool ret = network_available;
    pthread_mutex_unlock(&network_mutex);
    return ret;
}

// ========== 主函数 ==========
int main()
{
	// 为 I2C 探测列表分配可修改的副本
	size_t list_size = sizeof(i2c_probe_target) * I2C_PROBE_LIST_LEN;
	i2c_probe_target *list = (i2c_probe_target *)malloc(list_size);
	if (!list)
	{
		perror("malloc failed");
		return -1;
	}
	memcpy(list, I2C_PROBE_LIST, list_size);

	printf("========================================\n");
	printf("  桌面智能管家系统   \n");
	printf("========================================\n\n");

	// 1. 探测所有I2C设备
	printf("[1/6] I2C设备探测中...\n");
	i2c_probe(list, I2C_PROBE_LIST_LEN);
	printf("探测完成\n\n");

	// 2. 查找各模块（若缺失会exit，请确保硬件齐全）
	printf("[2/6] 查找各模块...\n");
	I2c_s1_find(list);
	I2c_s2_find(list);
	I2c_s7_find(list);
	I2c_s8_find(list);
	I2c_e1_led_find(list);
	I2c_e1_tube_find(list);
	I2c_e2_find(list);
	printf("所有模块已找到\n\n");

	// 3. 打开I2C设备（使用S1所在总线）
	int fd = I2c_open(list[key_idx].path);
	if (fd < 0)
	{
		perror("打开I2C设备失败");
		free(list);
		return -1;
	}
	printf("I2C设备已打开，fd=%d\n\n", fd);

	// 4. 初始化各模块
	printf("[3/6] 初始化S1键盘...\n");
	I2c_s1_init_all(fd, list);
	printf("[4/6] 初始化E1数码管与RGB灯...\n");
	I2c_e1_tube_init_all(fd, list);
	I2c_e1_led_init_all(fd, list);
	printf("[5/6] 初始化E2风扇...\n");
	I2c_e2_init_all(fd, list);
	printf("[6/6] 初始化传感器...\n");
	I2c_s2_init_all(fd, list);
	I2c_s7_init_all(fd, list);
	// S8无需显式初始化
	printf("\n所有模块初始化完成！开始运行...\n\n");

	// 环境数据初始值
	memset(&env, 0, sizeof(env));
	env.temperature = 25.0;
	env.humidity = 50.0;
	env.light_intensity = 300.0;
	env.human_detected = 1;

	// 启动后台音频播放线程（用于 E4）
	if (pthread_create(&audio_thread_id, NULL, audio_thread, NULL) != 0)
	{
		perror("创建音频线程失败");
	}

	// 启动网络监控线程
    if (pthread_create(&network_thread_id, NULL, network_monitor_thread, NULL) != 0) 
	{
        perror("创建网络检测线程失败");
    }

	int loop_cnt = 0;
	int sht_delay = 0; // 用于控制SHT3x读取频率(每2秒一次)

	while (1)
	{
		loop_cnt++;
		sht_delay++;

		// 采集S8温湿度(每2秒一次，避免I2C过载)
		if (sht_delay >= 40)
		{ // 50ms * 40 = 2秒
			sht_delay = 0;
			if (sht_idx >= 0 && list[sht_idx].detected_count > 0)
			{
				s8_para sht = read_sht3x(fd, list[sht_idx].detected_addrs[0]);
				if (sht.temperature >= -45 && sht.temperature <= 125)
					env.temperature = sht.temperature;
				if (sht.humidity >= 0 && sht.humidity <= 100)
					env.humidity = sht.humidity;
			}
		}
		// 采集S2光照
		if (bright_idx >= 0 && list[bright_idx].detected_count > 0)
		{
			float lux = s2_read_bh1750_value(fd, list[bright_idx].detected_addrs[0]);
			if (lux >= 0 && lux < 100000)
				env.light_intensity = lux;
		}
		// 采集S7人体
		if (human_idx >= 0 && list[human_idx].detected_count > 0)
		{
			env.human_detected = I2c_human_detected(fd, list[human_idx].detected_addrs[0]);
		}
		env.timestamp = time(NULL);

		// 智能联动控制
		smart_control(fd, list);
		// 更新E1显示和LED
		update_e1_display(fd, list);
		update_e1_led(fd, list);
		// 同步到 E5 触控屏显示
		update_e5_display(fd, list);

		// 检查来自 E5 的命令文件（web/e5_command.txt）并处理
		process_e5_command(fd, list);

		// 按键处理
		if (key_idx >= 0)
		{
			for (int i = 0; i < list[key_idx].detected_count; i++)
			{
				// 扫描按键状态
				process_keys(fd, list[key_idx].detected_addrs[i], list);
				// 处理所有12个按键的事件
				for (int k = 1; k <= 12; k++)
				{
					handle_key_event(fd, list, i, k);
				}
			}
		}

		// 每100次循环(约5秒)打印一次状态
		if (loop_cnt % 100 == 0)
		{
			print_system_status();
		}

		// 提高响应速度：50ms 循环一次
		usleep(50000); // 50ms
	}

	I2c_close(fd);
	free(list);
	return 0;
}

// 安全重启：先 sync，再尝试 reboot syscall，失败时回退到 system("reboot")
void system_reboot(void)
{
	printf("开始重启系统...\n");
	fflush(stdout);
	sync();
	if (reboot(RB_AUTOBOOT) != 0)
	{
		perror("reboot syscall 失败，尝试使用 reboot 命令");
		int r = system("reboot");
		if (r == -1)
			perror("system reboot 失败");
	}
}
