// DOM 元素
const tempEl = document.getElementById('temp');
const humEl = document.getElementById('hum');
const luxEl = document.getElementById('lux');
const humanEl = document.getElementById('human');
const fanEl = document.getElementById('fan');
const e1ModeEl = document.getElementById('e1_mode');
const sceneEl = document.getElementById('scene');
const idleEl = document.getElementById('idle');
const alertSpan = document.getElementById('alert');
const voiceToggleBtn = document.getElementById('voice_toggle');
const speakStatusBtn = document.getElementById('speak_status');
const startVoiceBtn = document.getElementById('start_voice');
const voiceHistoryEl = document.getElementById('voice_history');
const fanSpeedSlider = document.getElementById('fan_speed');
const fanSpeedVal = document.getElementById('fan_speed_val');
const brightnessSlider = document.getElementById('brightness_slider');
const brightnessVal = document.getElementById('brightness_val');
const fanModeBtn = document.getElementById('fan_mode_toggle');
const lightModeBtn = document.getElementById('light_mode_toggle');
const thTemp = document.getElementById('th_temp');
const thHum = document.getElementById('th_hum');
const thLight = document.getElementById('th_light');
const sleepInput = document.getElementById('sleep_time');
const rebootBtn = document.getElementById('reboot_btn');
const networkEl = document.getElementById('network');

let currentVoiceOn = true;
let currentFanMode = 0;
let currentLightMode = 0;
let chart = null;

// 发送命令
async function sendCommand(endpoint, payload) {
    try {
        const res = await fetch(endpoint, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        if (!res.ok) console.error('命令失败', endpoint);
        setTimeout(fetchStatus, 300);
    } catch(e) { console.error(e); }
}

// 图表初始化
function initChart() {
    const ctx = document.getElementById('envChart').getContext('2d');
    chart = new Chart(ctx, {
        type: 'line',
        data: { labels: [], datasets: [
            { label: '温度 (°C)', data: [], borderColor: '#ff7c60', tension: 0.2, fill: false },
            { label: '湿度 (%)', data: [], borderColor: '#5eb3ff', tension: 0.2, fill: false },
            { label: '光照 (lx/10)', data: [], borderColor: '#f9d371', tension: 0.2, fill: false }
        ] },
        options: {
            responsive: true,
            maintainAspectRatio: true,
            plugins: { legend: { position: 'top', labels: { color: '#ccc' } } },
            scales: {
                y: { grid: { color: '#2a3a4a' }, ticks: { color: '#ddd' } },
                x: { ticks: { color: '#aaa', autoSkip: true, maxTicksLimit: 6 } }
            }
        }
    });
}

function updateChart(history) {
    if (!chart) initChart();
    if (!history || !history.temp) return;
    chart.data.labels = Array(history.temp.length).fill('');
    chart.data.datasets[0].data = history.temp;
    chart.data.datasets[1].data = history.hum;
    chart.data.datasets[2].data = history.lux.map(v => v / 10);
    chart.update();
}

function renderVoiceHistory(items) {
    if (!voiceHistoryEl) return;
    if (!items || !items.length) {
        voiceHistoryEl.innerHTML = '<p>等待语音交互...</p>';
        return;
    }
    voiceHistoryEl.innerHTML = items.map(entry => {
        let roleClass = '';
        if (entry.role === 'user') roleClass = 'voice-user';
        else if (entry.role === 'assistant') roleClass = 'voice-assistant';
        else roleClass = 'voice-system';
        return `<div class="voice-entry ${roleClass}"><strong>${escapeHtml(entry.role)}：</strong>${escapeHtml(entry.text)}</div>`;
    }).join('');
}

function escapeHtml(str) {
    return str.replace(/[&<>]/g, function(m) {
        if (m === '&') return '&amp;';
        if (m === '<') return '&lt;';
        if (m === '>') return '&gt;';
        return m;
    });
}

async function fetchStatus() {
    try {
        const r = await fetch('e5_status.json?t=' + Date.now());
        if (!r.ok) throw new Error();
        const data = await r.json();

        tempEl.textContent = data.temperature.toFixed(1) + '°C';
        humEl.textContent = data.humidity.toFixed(1) + '%';
        luxEl.textContent = data.light.toFixed(0) + 'lx';
        humanEl.textContent = data.human ? '有人' : '无人';
        fanEl.textContent = data.fan_on ? data.fan_level + '%' : '关闭';

        const displayModes = ['关闭', '温度', '湿度', '光照'];
        e1ModeEl.textContent = displayModes[data.display_mode] || '--';
        const scenes = ['工作', '休闲', '睡眠'];
        sceneEl.textContent = scenes[data.scene] || '未知';
        idleEl.textContent = data.idle ? '待机' : '正常';
        networkEl.textContent = data.network ? '在线' : '离线';
        alertSpan.textContent = data.alert || '环境正常';

        currentVoiceOn = !!data.voice_on;
        voiceToggleBtn.textContent = currentVoiceOn ? '语音播报 开' : '语音播报 关';

        currentFanMode = data.fan_mode ?? 0;
        fanModeBtn.textContent = currentFanMode ? '风扇模式: 自动' : '风扇模式: 手动';
        currentLightMode = data.light_mode ?? 0;
        lightModeBtn.textContent = currentLightMode ? '灯光模式: 自动' : '灯光模式: 手动';

        thTemp.value = data.temperature_set ?? 28;
        thHum.value = data.humidity_set ?? 60;
        thLight.value = data.light_set ?? 300;
        sleepInput.value = data.sleep_time ?? 1;

        brightnessSlider.value = data.brightness ?? 50;
        brightnessVal.textContent = brightnessSlider.value;
        fanSpeedSlider.value = data.fan_level ?? 20;
        fanSpeedVal.textContent = fanSpeedSlider.value;

        if (data.history) updateChart(data.history);
        renderVoiceHistory(data.voice_history);
        document.getElementById('raw').textContent = JSON.stringify(data, null, 2);
    } catch(e) {
        alertSpan.textContent = '无法连接至智能管家';
    }
}

// 启动轮询
fetchStatus();
setInterval(fetchStatus, 800);

// 按钮事件绑定（确保 DOM 元素存在）
document.getElementById('fan_on').onclick = () => sendCommand('/api/fan', { action: 'on', speed: parseInt(fanSpeedSlider.value) });
document.getElementById('fan_off').onclick = () => sendCommand('/api/fan', { action: 'off' });
document.getElementById('fan_set').onclick = () => sendCommand('/api/fan', { action: 'set', speed: parseInt(fanSpeedSlider.value) });
document.getElementById('fan_speed_up').onclick = () => {
    let v = parseInt(fanSpeedSlider.value) + 20;
    if (v > 100) v = 100;
    fanSpeedSlider.value = v;
    fanSpeedVal.textContent = v;
    sendCommand('/api/fan', { action: 'set', speed: v });
};
document.getElementById('fan_speed_down').onclick = () => {
    let v = parseInt(fanSpeedSlider.value) - 20;
    if (v < 0) v = 0;
    fanSpeedSlider.value = v;
    fanSpeedVal.textContent = v;
    sendCommand('/api/fan', { action: 'set', speed: v });
};
fanModeBtn.onclick = () => sendCommand('/api/fanmode', { mode: currentFanMode ? 0 : 1 });

brightnessSlider.oninput = () => brightnessVal.textContent = brightnessSlider.value;
document.getElementById('brightness_set').onclick = () => sendCommand('/api/brightness', { brightness: parseInt(brightnessSlider.value) });
document.getElementById('brightness_up').onclick = () => {
    let v = parseInt(brightnessSlider.value) + 10;
    if (v > 100) v = 100;
    brightnessSlider.value = v;
    brightnessVal.textContent = v;
    sendCommand('/api/brightness', { brightness: v });
};
document.getElementById('brightness_down').onclick = () => {
    let v = parseInt(brightnessSlider.value) - 10;
    if (v < 0) v = 0;
    brightnessSlider.value = v;
    brightnessVal.textContent = v;
    sendCommand('/api/brightness', { brightness: v });
};
lightModeBtn.onclick = () => sendCommand('/api/lightmode', { mode: currentLightMode ? 0 : 1 });

document.querySelectorAll('[data-display]').forEach(btn => {
    btn.onclick = () => sendCommand('/api/display', { mode: parseInt(btn.dataset.display) });
});
document.querySelectorAll('[data-scene]').forEach(btn => {
    btn.onclick = () => sendCommand('/api/scene', { scene: parseInt(btn.dataset.scene) });
});

voiceToggleBtn.onclick = () => sendCommand('/api/voice', { action: currentVoiceOn ? 'off' : 'on' });
speakStatusBtn.onclick = () => sendCommand('/api/speak_status', {});
startVoiceBtn.onclick = () => sendCommand('/api/voice', { action: 'start' });

document.getElementById('set_temp').onclick = () => sendCommand('/api/threshold', { type: 'temp', value: parseInt(thTemp.value) });
document.getElementById('set_hum').onclick = () => sendCommand('/api/threshold', { type: 'hum', value: parseInt(thHum.value) });
document.getElementById('set_light').onclick = () => sendCommand('/api/threshold', { type: 'light', value: parseInt(thLight.value) });
document.getElementById('set_sleep').onclick = () => sendCommand('/api/sleep', { minutes: parseInt(sleepInput.value) });
rebootBtn.onclick = () => sendCommand('/api/reboot', {});