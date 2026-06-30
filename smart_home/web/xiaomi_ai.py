#!/usr/bin/env python3
import os
import sys
import json
import time
import base64
import re
import urllib.parse
import urllib.request
from urllib import request, parse

CONFIG_PATH = os.path.join(os.path.dirname(__file__), 'xiaomi_config.json')
DEFAULT_BASE_URL = 'https://api.xiaomimimo.com/v1'
DEFAULT_MODEL = 'mimo-v2.5'
SYSTEM_PROMPT = (
    'You are MiMo, an AI assistant developed by Xiaomi. '
    'Today is date: Tuesday, December 16, 2025. Your knowledge cutoff date is December 2024.'
)


def load_config():
    try:
        with open(CONFIG_PATH, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception:
        return {}


def get_base_url():
    cfg = load_config()
    return cfg.get('BASE_URL') or DEFAULT_BASE_URL


def get_api_key():
    cfg = load_config()
    return cfg.get('API_KEY') or cfg.get('APP_KEY') or os.environ.get('MIMO_API_KEY')


def call_api(url, headers, data):
    req = request.Request(url, data=data, headers=headers)
    with request.urlopen(req, timeout=60) as resp:
        body = resp.read()
        return body.decode('utf-8', errors='ignore')


def build_chat_payload(messages, model=None, max_tokens=1024):
    return json.dumps(
        {
            'model': model or DEFAULT_MODEL,
            'messages': messages,
            'max_completion_tokens': max_tokens,
            'temperature': 0.2,
            'top_p': 0.9,
            'stream': False,
        },
        ensure_ascii=False,
    ).encode('utf-8')


def parse_assistant_text(response_text):
    try:
        payload = json.loads(response_text)
        choices = payload.get('choices', [])
        if choices:
            message = choices[0].get('message', {})
            text = message.get('content', '')
            return text.strip()
    except Exception:
        pass
    return response_text.strip()


def chat_with_audio(audio_path, instruction):
    if not os.path.exists(audio_path):
        return json.dumps({'status': 'error', 'text': ''}, ensure_ascii=False)

    with open(audio_path, 'rb') as f:
        audio_bytes = f.read()
    audio_b64 = base64.b64encode(audio_bytes).decode('ascii')
    mime_type = 'audio/wav'
    audio_data = f'data:{mime_type};base64,{audio_b64}'

    messages = [
        {'role': 'system', 'content': SYSTEM_PROMPT},
        {
            'role': 'user',
            'content': [
                {'type': 'input_audio', 'input_audio': {'data': audio_data}},
                {'type': 'text', 'text': instruction},
            ],
        },
    ]

    url = get_base_url().rstrip('/') + '/chat/completions'
    api_key = get_api_key()
    if not api_key:
        return json.dumps({'status': 'error', 'text': ''}, ensure_ascii=False)

    headers = {
        'Content-Type': 'application/json',
        'api-key': api_key,
    }

    try:
        result = call_api(url, headers, build_chat_payload(messages))
        text = parse_assistant_text(result)
        return json.dumps({'status': 'ok', 'text': text}, ensure_ascii=False)
    except Exception:
        return json.dumps({'status': 'error', 'text': ''}, ensure_ascii=False)


def get_tts_model():
    cfg = load_config()
    return cfg.get('TTS_MODEL') or 'mimo-v2.5-tts'


def get_tts_voice():
    cfg = load_config()
    return cfg.get('TTS_VOICE') or 'mimo_default'


def build_tts_payload(messages, model=None, audio_format='wav', voice=None, max_tokens=1024):
    return json.dumps(
        {
            'model': model or get_tts_model(),
            'messages': messages,
            'audio': {
                'format': audio_format,
                'voice': voice or get_tts_voice(),
            },
            'max_completion_tokens': max_tokens,
            'temperature': 0.2,
            'top_p': 0.9,
            'stream': False,
        },
        ensure_ascii=False,
    ).encode('utf-8')


def parse_audio_data(response_text):
    try:
        payload = json.loads(response_text)
        choices = payload.get('choices', [])
        if choices:
            message = choices[0].get('message', {})
            audio = message.get('audio', {})
            return audio.get('data', '')
    except Exception:
        pass
    return ''


def chat_text(text, instruction, model=None):
    url = get_base_url().rstrip('/') + '/chat/completions'
    api_key = get_api_key()
    if not api_key:
        return ''

    messages = [
        {'role': 'system', 'content': SYSTEM_PROMPT},
        {'role': 'user', 'content': f'{instruction}\n用户指令：{text}'},
    ]

    headers = {
        'Content-Type': 'application/json',
        'api-key': api_key,
    }

    try:
        result = call_api(url, headers, build_chat_payload(messages, model=model))
        text = parse_assistant_text(result)
        return text
    except Exception:
        return ''


def asr(audio_path):
    result = chat_with_audio(audio_path, '请将这段音频中的语音内容准确转写为中文文本，仅返回转写结果，不要附加解释。')
    try:
        payload = json.loads(result)
        return payload
    except Exception:
        return {'status': 'error', 'text': ''}


def nlp(text):
    """
    自然语言理解：优先本地关键词匹配（快速可靠），
    本地无法识别时再尝试云端 API。
    """
    text_lower = text.strip().lower()

    # ========== 1. 本地关键词匹配（优先） ==========
    # --- 灯光控制 ---
    if '开灯' in text_lower or '打开灯' in text_lower or '点亮' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'light_on', 'slots': ''}, ensure_ascii=False)
    if '关灯' in text_lower or '关闭灯' in text_lower or '熄灭' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'light_off', 'slots': ''}, ensure_ascii=False)

    # 亮度设置
    if '变亮' in text_lower or '太暗' in text_lower in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'light_brightness', 'slots': '80'}, ensure_ascii=False)
    if '变暗' in text_lower or '太亮' in text_lower in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'light_brightness', 'slots': '20'}, ensure_ascii=False)
    # 颜色设置 没用
    color_map = {
        '红': 'red', '红色': 'red',
        '绿': 'green', '绿色': 'green',
        '蓝': 'blue', '蓝色': 'blue',
        '黄': 'yellow', '黄色': 'yellow',
        '白': 'white', '白色': 'white',
        '橙': 'orange', '橙色': 'orange',
        '紫': 'purple', '紫色': 'purple'
    }
    for kw, color in color_map.items():
        if kw in text_lower and ('灯' in text_lower or '颜色' in text_lower):
            return json.dumps({'status': 'ok', 'intent': 'light_color', 'slots': color}, ensure_ascii=False)

    # --- 风扇控制 ---
    if '打开风扇' in text_lower or '开风扇' in text_lower or '风扇打开' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'fan_on', 'slots': ''}, ensure_ascii=False)
    if '关闭风扇' in text_lower or '关风扇' in text_lower or '风扇关闭' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'fan_off', 'slots': ''}, ensure_ascii=False)

    # 数字调速
    speed_match = re.search(r'(?:风扇|速度)\s*(\d{1,3})', text_lower)
    if speed_match:
        val = speed_match.group(1)
        return json.dumps({'status': 'ok', 'intent': 'fan_speed', 'slots': val}, ensure_ascii=False)
    if '高速' in text_lower or '大风' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'fan_speed', 'slots': '80'}, ensure_ascii=False)
    if '中速' in text_lower or '中风' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'fan_speed', 'slots': '50'}, ensure_ascii=False)
    if '低速' in text_lower or '小风' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'fan_speed', 'slots': '20'}, ensure_ascii=False)

    # --- 环境信息 ---
    if '温度' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'announce_temperature', 'slots': ''}, ensure_ascii=False)
    if '湿度' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'announce_humidity', 'slots': ''}, ensure_ascii=False)
    if '光照' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'announce_lightness', 'slots': ''}, ensure_ascii=False)
    if '显示模式' in text_lower or '切换显示' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'switch_display', 'slots': ''}, ensure_ascii=False)

    # --- 天气查询（真实天气）---
    # 匹配模式：北京天气、上海的天气、天气怎么样、今天天气如何等
    weather_match = re.search(r'([\u4e00-\u9fa5]{2,}?)的?天气', text_lower)
    if weather_match:
        city = weather_match.group(1)
        # 简单过滤掉“今天”、“明天”等词
        if city in ['今天', '明天', '后天', '天气']:
            # 没有指定城市，使用默认城市
            city = ''
        return json.dumps({'status': 'ok', 'intent': 'real_weather', 'slots': city}, ensure_ascii=False)
    # 如果没有明确城市，仅说“天气”或“天气怎么样”
    if '天气' in text_lower and not re.search(r'[如何怎样]', text_lower):
        return json.dumps({'status': 'ok', 'intent': 'real_weather', 'slots': ''}, ensure_ascii=False)
    
    # --- 日期时间 ---
    if '日期' in text_lower or '几号' in text_lower or '今天几号' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'query_date', 'slots': ''}, ensure_ascii=False)
    if '时间' in text_lower or '几点' in text_lower or '现在时间' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'query_time', 'slots': ''}, ensure_ascii=False)

    # --- 场景模式改变 ---
    if '场景模式' in text_lower and '切换' in text_lower:
        return json.dumps({'status': 'ok', 'intent': 'background_change', 'slots': ''}, ensure_ascii=False)
    # ========== 2. 云端 API 补充（仅本地未匹配时） ==========
    api_text = chat_text(
        text,
        '请把这条用户指令解析成JSON，只返回JSON对象，格式为 {"intent":"<意图>","slots":"<参数>"}。如果无法识别意图，请返回 {"intent":"","slots":""}。',
    )
    if api_text:
        try:
            parsed = json.loads(api_text)
            intent = parsed.get('intent', '')
            slots = parsed.get('slots', '')
            if intent:
                return json.dumps({'status': 'ok', 'intent': intent, 'slots': slots}, ensure_ascii=False)
        except Exception:
            pass

    # 完全无法识别
    return json.dumps({'status': 'ok', 'intent': '', 'slots': ''}, ensure_ascii=False)

def tts(text, output_path):
    if not text:
        return json.dumps({'status': 'error', 'text': ''}, ensure_ascii=False)

    url = get_base_url().rstrip('/') + '/chat/completions'
    api_key = get_api_key()
    if api_key:
        messages = [
            {'role': 'user', 'content': '请用自然、清晰的普通话语气朗读下面的内容。'},
            {'role': 'assistant', 'content': text},
        ]
        headers = {
            'Content-Type': 'application/json',
            'api-key': api_key,
        }
        try:
            result = call_api(url, headers, build_tts_payload(messages))
            audio_data = parse_audio_data(result)
            if audio_data:
                audio_bytes = base64.b64decode(audio_data)
                with open(output_path, 'wb') as f:
                    f.write(audio_bytes)
                return json.dumps({'status': 'ok', 'text': text}, ensure_ascii=False)
        except Exception:
            pass

    # 本地 TTS 降级
    try:
        import subprocess
        if os.path.exists('/usr/bin/pico2wave'):
            subprocess.run(['/usr/bin/pico2wave', '-l=zh-CN', '-w', output_path, text], check=True)
            return json.dumps({'status': 'ok', 'text': text}, ensure_ascii=False)
        if os.path.exists('/usr/bin/espeak-ng'):
            subprocess.run(['/usr/bin/espeak-ng', '-v', 'zh', '-w', output_path, text], check=True)
            return json.dumps({'status': 'ok', 'text': text}, ensure_ascii=False)
        if os.path.exists('/usr/bin/espeak'):
            subprocess.run(['/usr/bin/espeak', '-v', 'zh', '-w', output_path, text], check=True)
            return json.dumps({'status': 'ok', 'text': text}, ensure_ascii=False)
    except Exception:
        pass
    return json.dumps({'status': 'error', 'text': ''}, ensure_ascii=False)

def get_real_weather(city):
    """
    通过 wttr.in 获取真实天气，返回格式化的中文天气描述。
    失败时返回 None。
    """
    if not city:
        cfg = load_config()
        city = cfg.get('DEFAULT_CITY', '天津')
    
    encoded_city = urllib.parse.quote(city)
    # 使用 | 分隔字段；lang=zh 请求中文
    url = f"https://wttr.in/{encoded_city}?format=%C|%t|%w|%h&lang=zh"
    
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'curl/7.68.0'})
        with urllib.request.urlopen(req, timeout=5) as response:
            data = response.read().decode('utf-8').strip()
        
        # 按 | 分割：天气状况 | 温度 | 风力 | 湿度
        parts = data.split('|')
        if len(parts) >= 4:
            condition = parts[0].strip()
            temp = parts[1].strip().replace('+', '')
            wind_raw = parts[2].strip()
            humidity = parts[3].strip()
            
            # 增强的英译中字典，覆盖更全面的天气描述
            weather_en2zh = {
                'Mist': '雾', 'Fog': '雾', 'Haze': '霾',
                'Light Rain': '小雨', 'Light Rain, Mist': '小雨, 雾', 
                'Rain': '雨', 'Heavy Rain': '大雨', 'Rain, Mist': '雨, 雾',
                'Partly Cloudy': '多云', 'Cloudy': '多云', 'Overcast': '阴',
                'Sunny': '晴', 'Clear': '晴', 'Snow': '雪',
                'Thunderstorm': '雷阵雨', 'Drizzle': '毛毛雨',
                'Mostly Cloudy': '大部分多云'
            }
            
            # 检查是否需要翻译（不包含中文字符）
            if not re.search(r'[\u4e00-\u9fff]', condition):
                # 优先尝试精确匹配
                if condition in weather_en2zh:
                    condition = weather_en2zh[condition]
                else:
                    # 尝试部分匹配（例如：将 "Light Rain, Mist" 转为 "小雨, 雾"）
                    translated = condition
                    for en, zh in weather_en2zh.items():
                        if en in condition:
                            translated = translated.replace(en, zh)
                    condition = translated
            
            # 处理风力中的风向箭头
            arrow_map = {
                '←': '西风', '↙': '西南风', '↓': '南风', '↘': '东南风',
                '→': '东风', '↗': '东北风', '↑': '北风', '↖': '西北风'
            }
            arrow_match = re.match(r'([←↙↓↘→↗↑↖])(\d+km/h)', wind_raw)
            if arrow_match:
                arrow, speed = arrow_match.groups()
                wind_desc = arrow_map.get(arrow, '') + speed
            else:
                wind_desc = wind_raw
            
            # 组装自然流畅的播报文本
            desc = f"{city}天气"
            if condition:
                desc += f"，{condition}"
            desc += f"，温度{temp}"
            if wind_desc:
                desc += f"，风力{wind_desc}"
            if humidity:
                desc += f"，湿度{humidity}"
            return desc
        else:
            # 格式不符合预期时的降级处理
            return f"{city}天气：{data}"
    except Exception as e:
        print(f"[天气API] 获取失败: {e}")
        return None

def skill(skill_type):
    if skill_type == 'weather':
        prompt = '请回答一条简短的天气查询内容。'
    elif skill_type == 'schedule':
        prompt = '请回答一条简短的日程提醒内容。'
    else:
        prompt = '请回答一条简短的问题。'

    text = chat_text('', prompt)
    if text:
        return json.dumps({'status': 'ok', 'text': text}, ensure_ascii=False)

    # fallback
    if skill_type == 'weather':
        return json.dumps({'status': 'ok', 'text': '今天天气晴朗，温度适中，适合外出。'}, ensure_ascii=False)
    if skill_type == 'schedule':
        return json.dumps({'status': 'ok', 'text': '您今天下午有一项会议，晚上有锻炼计划。'}, ensure_ascii=False)
    return json.dumps({'status': 'ok', 'text': '当前技能暂不可用。'}, ensure_ascii=False)


def main():
    if len(sys.argv) < 2:
        print(json.dumps({'status': 'error', 'text': 'missing command'}, ensure_ascii=False))
        return
    cmd = sys.argv[1]
    if cmd == 'asr' and len(sys.argv) == 3:
        result = asr(sys.argv[2])
        print(json.dumps(result, ensure_ascii=False) if isinstance(result, dict) else result)
    elif cmd == 'nlp' and len(sys.argv) == 3:
        print(nlp(sys.argv[2]))
    elif cmd == 'tts' and len(sys.argv) == 4:
        print(tts(sys.argv[2], sys.argv[3]))
    elif cmd == 'skill' and len(sys.argv) == 3:
        print(skill(sys.argv[2]))
    elif cmd == 'real_weather' and len(sys.argv) == 3:
        city = sys.argv[2]
        result = get_real_weather(city)
        if result:
            print(json.dumps({'status': 'ok', 'text': result}, ensure_ascii=False))
        else:
            # 失败时返回空，让 C++ 降级到原有 AI 生成
            print(json.dumps({'status': 'error', 'text': ''}, ensure_ascii=False))
    else:
        print(json.dumps({'status': 'error', 'text': 'invalid command'}, ensure_ascii=False))


if __name__ == '__main__':
    main()
