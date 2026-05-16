<template>
  <div class="app-root">
    <!-- Ambient Light Orbs -->
    <div class="ambient-orb ambient-orb-top"></div>
    <div class="ambient-orb ambient-orb-btm"></div>

    <!-- ==================== TopAppBar ==================== -->
    <header class="topbar">
      <div class="topbar-left">
        <h1 class="topbar-title">Lumina Home</h1>
        <span class="topbar-divider"></span>
        <span class="topbar-subtitle">智能室内环境控制中枢</span>
      </div>
      <div class="topbar-right">
        <!-- Clickable connection pill -->
        <button class="connection-pill" title="点击配置后端地址" @click="showConnPanel = !showConnPanel">
          <span class="conn-dot" :class="connectionClass"></span>
          <span class="text-label-sm text-on-surface-variant">{{ connectionText }}</span>
          <span class="material-symbols-outlined text-[18px] text-on-surface-variant ml-1">expand_more</span>
        </button>
        <div class="time-readout">{{ currentTimeText }}</div>
        <div class="icon-group">
          <button class="icon-btn" title="刷新连接" @click="reconnect">
            <span class="material-symbols-outlined text-[24px]">wifi</span>
          </button>
          <button class="icon-btn relative" title="通知">
            <span class="material-symbols-outlined text-[24px]">notifications</span>
            <span class="notif-dot"></span>
          </button>
        </div>
      </div>
    </header>

    <!-- ==================== Connection Config Panel ==================== -->
    <Transition name="panel-slide">
      <div v-if="showConnPanel" class="conn-panel glass-card">
        <div class="conn-panel-header">
          <h3 class="conn-panel-title">WebSocket 连接配置</h3>
          <button class="icon-btn" @click="showConnPanel = false">
            <span class="material-symbols-outlined text-[20px]">close</span>
          </button>
        </div>
        <div class="conn-panel-body">
          <label class="conn-label" for="ws-url">后端 WebSocket 地址</label>
          <div class="conn-input-row">
            <input
              id="ws-url"
              v-model="backendUrlInput"
              class="conn-input"
              placeholder="ws://your-server:8080/ws"
              type="text"
              @keyup.enter="saveBackendUrl"
            />
            <button class="conn-save-btn" @click="saveBackendUrl">保存并重连</button>
          </div>
          <p class="conn-hint">地址将保存在浏览器本地，下次打开自动使用。</p>
          <p class="conn-status">
            当前状态:
            <span :class="connectionClass === 'conn-online' ? 'text-emerald-400' : 'text-red-400'">
              {{ isConnected ? '已连接' : '未连接' }}
            </span>
            <span class="conn-url-display" v-if="backendUrlInput">{{ backendUrlInput }}</span>
          </p>
        </div>
      </div>
    </Transition>

    <!-- ==================== SideNavBar ==================== -->
    <nav class="sidenav">
      <button class="nav-item" :class="{ active: currentPage === 'dashboard' }" title="仪表盘" @click="currentPage = 'dashboard'">
        <span class="material-symbols-outlined text-[28px] nav-icon">grid_view</span>
      </button>
      <button class="nav-item" :class="{ active: currentPage === 'climate' }" title="气候环境" @click="currentPage = 'climate'">
        <span class="material-symbols-outlined text-[28px] nav-icon">thermostat</span>
      </button>
      <button class="nav-item" :class="{ active: currentPage === 'lighting' }" title="照明控制" @click="currentPage = 'lighting'">
        <span class="material-symbols-outlined text-[28px] nav-icon">lightbulb</span>
      </button>
      <button class="nav-item" :class="{ active: currentPage === 'security' }" title="安防监控" @click="currentPage = 'security'">
        <span class="material-symbols-outlined text-[28px] nav-icon">shield</span>
      </button>
      <button class="nav-item mt-auto" :class="{ active: currentPage === 'settings' }" title="系统设置" @click="currentPage = 'settings'">
        <span class="material-symbols-outlined text-[28px] nav-icon">settings</span>
      </button>
    </nav>

    <!-- ==================== Main Content ==================== -->
    <main class="main-content">
      <div class="main-inner">

        <!-- ========== PAGE: Dashboard 仪表盘 ========== -->
        <template v-if="currentPage === 'dashboard'">
          <div class="section-header">
            <div>
              <h2 class="section-title">环境概览</h2>
              <p class="section-desc">所有系统运行正常，环境参数处于舒适区间。</p>
            </div>
          </div>

          <div class="bento-grid">
            <!-- Card 1: Power -->
            <div class="card-power" :class="{ 'is-off': deviceShadow.power === 'off' }" role="button" tabindex="0" @click="togglePower" @keydown.enter.prevent="togglePower">
              <div class="card-power-overlay"></div>
              <div class="card-power-top">
                <div class="card-power-icon-wrap">
                  <span class="material-symbols-outlined text-[40px]" style="font-variation-settings:'FILL'1">power_settings_new</span>
                </div>
                <span class="card-power-badge">主控</span>
              </div>
              <div class="card-power-bot">
                <h3 class="card-power-title">{{ powerTitle }}</h3>
                <p class="card-power-hint">{{ deviceShadow.power === 'on' ? '点击以一键休眠' : '点击以开启系统' }}</p>
              </div>
            </div>

            <!-- Card 2: Lux -->
            <div class="card-sensor">
              <div class="sensor-icon-wrap sensor-icon-tertiary">
                <span class="material-symbols-outlined text-tertiary text-[32px]">light_mode</span>
              </div>
              <div>
                <p class="sensor-label">室内光照</p>
                <div class="sensor-value-row">
                  <span class="sensor-value">{{ luxText }}</span>
                  <span class="sensor-unit">Lux</span>
                </div>
              </div>
            </div>

            <!-- Card 3: Presence -->
            <div class="card-sensor card-sensor-presence">
              <div class="presence-pulse"></div>
              <div class="sensor-icon-wrap sensor-icon-primary">
                <span class="presence-ring"></span>
                <span class="material-symbols-outlined text-primary text-[32px]" style="font-variation-settings:'FILL'1">sensors</span>
              </div>
              <div>
                <p class="sensor-label">存在传感</p>
                <h4 class="presence-state">{{ humanText }}</h4>
              </div>
            </div>

            <!-- Card 4: Mode -->
            <div class="card-mode">
              <div class="card-mode-head">
                <p class="sensor-label">运行模式</p>
                <span class="material-symbols-outlined text-on-surface-variant">tune</span>
              </div>
              <div class="mode-toggle-wrap">
                <button class="mode-btn" :class="deviceShadow.mode==='auto'?'mode-btn-active':''" @click="setMode('auto')">自动模式</button>
                <button class="mode-btn" :class="deviceShadow.mode==='manual'?'mode-btn-active':''" @click="setMode('manual')">手动模式</button>
              </div>
            </div>

            <!-- Card 5: Blind -->
            <div class="card-slider">
              <div class="card-slider-head">
                <div class="slider-label-inline">
                  <span class="material-symbols-outlined text-on-surface-variant text-[20px]">blinds</span>
                  <p class="sensor-label">百叶窗角度</p>
                </div>
                <span class="slider-value">{{ deviceShadow.blind_angle }}°</span>
              </div>
              <div class="slider-track-wrap">
                <input v-model.number="deviceShadow.blind_angle" :disabled="isAuto" class="slider-input" max="180" min="0" type="range" @change="syncState" />
              </div>
              <div class="slider-marks"><span>0°</span><span>90°</span><span>180°</span></div>
            </div>

            <!-- Card 6: Lighting -->
            <div class="card-lighting">
              <div class="card-lighting-head">
                <p class="sensor-label">全局照明控制</p>
                <span class="material-symbols-outlined text-on-surface-variant">light</span>
              </div>
              <div class="lighting-buttons">
                <button v-for="opt in lightOptions" :key="opt.value" class="light-btn" :class="lightButtonClass(opt.value)" :disabled="isAuto" @click="setLightLevel(opt.value)">
                  <span>{{ opt.label }}</span>
                  <span v-if="deviceShadow.light_level===opt.value" class="light-dot-active"></span>
                </button>
              </div>
            </div>
          </div>
        </template>

        <!-- ========== PAGE: Climate 气候环境 ========== -->
        <template v-if="currentPage === 'climate'">
          <div class="section-header">
            <div>
              <h2 class="section-title">气候环境</h2>
              <p class="section-desc">实时监测室内温度、湿度与空气质量。</p>
            </div>
          </div>
          <div class="bento-grid">
            <!-- Temperature Gauge -->
            <div class="card-sensor card-climate-temp">
              <div class="sensor-icon-wrap sensor-icon-warm">
                <span class="material-symbols-outlined text-[32px]" style="color:#ffb786">device_thermostat</span>
              </div>
              <div class="climate-temp-info">
                <p class="sensor-label">室内温度</p>
                <div class="sensor-value-row">
                  <span class="sensor-value">24.5</span>
                  <span class="sensor-unit">°C</span>
                </div>
                <p class="climate-sub">舒适范围 18°C ~ 26°C</p>
              </div>
            </div>

            <!-- Humidity -->
            <div class="card-sensor card-climate-humid">
              <div class="sensor-icon-wrap sensor-icon-cool">
                <span class="material-symbols-outlined text-[32px]" style="color:#60a5fa">humidity_percentage</span>
              </div>
              <div class="climate-temp-info">
                <p class="sensor-label">相对湿度</p>
                <div class="sensor-value-row">
                  <span class="sensor-value">52</span>
                  <span class="sensor-unit">%</span>
                </div>
                <p class="climate-sub">舒适范围 40% ~ 60%</p>
              </div>
            </div>

            <!-- Air Quality / CO2 -->
            <div class="card-sensor card-climate-aq">
              <div class="sensor-icon-wrap sensor-icon-good">
                <span class="material-symbols-outlined text-[32px]" style="color:#34d399">air</span>
              </div>
              <div class="climate-temp-info">
                <p class="sensor-label">空气质量 (CO₂)</p>
                <div class="sensor-value-row">
                  <span class="sensor-value">680</span>
                  <span class="sensor-unit">ppm</span>
                </div>
                <p class="climate-sub climate-good">优秀 ( &lt; 800ppm )</p>
              </div>
            </div>

            <!-- Outdoor Weather (mock) -->
            <div class="card-sensor card-climate-outdoor">
              <div class="sensor-icon-wrap" style="background:rgba(96,165,250,0.15);border:1px solid rgba(96,165,250,0.3)">
                <span class="material-symbols-outlined text-[32px]" style="color:#60a5fa">wb_sunny</span>
              </div>
              <div class="climate-temp-info">
                <p class="sensor-label">室外天气</p>
                <div class="sensor-value-row">
                  <span class="sensor-value">22</span>
                  <span class="sensor-unit">°C 晴</span>
                </div>
                <p class="climate-sub">微风 3级 · 紫外线 中等</p>
              </div>
            </div>

            <!-- HVAC Status -->
            <div class="card-sensor card-climate-hvac">
              <div class="sensor-icon-wrap" style="background:rgba(233,30,140,0.15);border:1px solid rgba(255,179,217,0.3)">
                <span class="material-symbols-outlined text-[32px]" style="color:#ffb3d9">ac_unit</span>
              </div>
              <div class="climate-temp-info">
                <p class="sensor-label">空调状态</p>
                <div class="sensor-value-row">
                  <span class="sensor-value" style="font-size:28px;line-height:1.2">制冷中</span>
                </div>
                <p class="climate-sub">设定温度 24°C · 低风速</p>
              </div>
            </div>
          </div>
        </template>

        <!-- ========== PAGE: Lighting 照明控制 ========== -->
        <template v-if="currentPage === 'lighting'">
          <div class="section-header">
            <div>
              <h2 class="section-title">分区照明</h2>
              <p class="section-desc">独立控制各房间亮度与开关状态。</p>
            </div>
          </div>
          <div class="bento-grid">
            <!-- Living Room -->
            <div class="card-zone card-zone-on">
              <div class="card-zone-head">
                <div class="zone-icon-wrap">
                  <span class="material-symbols-outlined text-[36px]" style="font-variation-settings:'FILL'1">weekend</span>
                </div>
                <div>
                  <p class="zone-name">客厅</p>
                  <p class="zone-level">亮度 {{ zoneLiving.level }}%</p>
                </div>
                <button class="zone-toggle zone-toggle-on" @click="toggleZone('living')">
                  <span class="material-symbols-outlined text-[24px]">{{ zoneLiving.on ? 'toggle_on' : 'toggle_off' }}</span>
                </button>
              </div>
              <div class="slider-track-wrap" style="margin-top:12px">
                <input v-model.number="zoneLiving.level" :disabled="!zoneLiving.on" class="slider-input" max="100" min="0" type="range" />
              </div>
            </div>

            <!-- Bedroom -->
            <div class="card-zone card-zone-off">
              <div class="card-zone-head">
                <div class="zone-icon-wrap">
                  <span class="material-symbols-outlined text-[36px]" style="font-variation-settings:'FILL'1">bed</span>
                </div>
                <div>
                  <p class="zone-name">卧室</p>
                  <p class="zone-level">已关闭</p>
                </div>
                <button class="zone-toggle zone-toggle-off" @click="toggleZone('bedroom')">
                  <span class="material-symbols-outlined text-[24px]">{{ zoneBedroom.on ? 'toggle_on' : 'toggle_off' }}</span>
                </button>
              </div>
              <div class="slider-track-wrap" style="margin-top:12px">
                <input v-model.number="zoneBedroom.level" :disabled="!zoneBedroom.on" class="slider-input" max="100" min="0" type="range" />
              </div>
            </div>

            <!-- Kitchen -->
            <div class="card-zone card-zone-on">
              <div class="card-zone-head">
                <div class="zone-icon-wrap">
                  <span class="material-symbols-outlined text-[36px]" style="font-variation-settings:'FILL'1">cooking</span>
                </div>
                <div>
                  <p class="zone-name">厨房</p>
                  <p class="zone-level">亮度 {{ zoneKitchen.level }}%</p>
                </div>
                <button class="zone-toggle zone-toggle-on" @click="toggleZone('kitchen')">
                  <span class="material-symbols-outlined text-[24px]">{{ zoneKitchen.on ? 'toggle_on' : 'toggle_off' }}</span>
                </button>
              </div>
              <div class="slider-track-wrap" style="margin-top:12px">
                <input v-model.number="zoneKitchen.level" :disabled="!zoneKitchen.on" class="slider-input" max="100" min="0" type="range" />
              </div>
            </div>

            <!-- Bathroom -->
            <div class="card-zone card-zone-off">
              <div class="card-zone-head">
                <div class="zone-icon-wrap">
                  <span class="material-symbols-outlined text-[36px]" style="font-variation-settings:'FILL'1">bathtub</span>
                </div>
                <div>
                  <p class="zone-name">浴室</p>
                  <p class="zone-level">已关闭</p>
                </div>
                <button class="zone-toggle zone-toggle-off" @click="toggleZone('bathroom')">
                  <span class="material-symbols-outlined text-[24px]">{{ zoneBathroom.on ? 'toggle_on' : 'toggle_off' }}</span>
                </button>
              </div>
              <div class="slider-track-wrap" style="margin-top:12px">
                <input v-model.number="zoneBathroom.level" :disabled="!zoneBathroom.on" class="slider-input" max="100" min="0" type="range" />
              </div>
            </div>

            <!-- Study -->
            <div class="card-zone card-zone-on">
              <div class="card-zone-head">
                <div class="zone-icon-wrap">
                  <span class="material-symbols-outlined text-[36px]" style="font-variation-settings:'FILL'1">menu_book</span>
                </div>
                <div>
                  <p class="zone-name">书房</p>
                  <p class="zone-level">亮度 {{ zoneStudy.level }}%</p>
                </div>
                <button class="zone-toggle zone-toggle-on" @click="toggleZone('study')">
                  <span class="material-symbols-outlined text-[24px]">{{ zoneStudy.on ? 'toggle_on' : 'toggle_off' }}</span>
                </button>
              </div>
              <div class="slider-track-wrap" style="margin-top:12px">
                <input v-model.number="zoneStudy.level" :disabled="!zoneStudy.on" class="slider-input" max="100" min="0" type="range" />
              </div>
            </div>

            <!-- All Off -->
            <div class="card-sensor" style="cursor:pointer;justify-content:center;grid-column:span 8" @click="allZonesOff">
              <div class="sensor-icon-wrap" style="background:rgba(255,180,171,0.15);border:1px solid rgba(255,180,171,0.3)">
                <span class="material-symbols-outlined text-[32px]" style="color:#ffb4ab">power_off</span>
              </div>
              <div>
                <p class="sensor-label">一键全关</p>
                <p class="climate-sub">关闭所有分区照明</p>
              </div>
            </div>
          </div>
        </template>

        <!-- ========== PAGE: Security 安防监控 ========== -->
        <template v-if="currentPage === 'security'">
          <div class="section-header">
            <div>
              <h2 class="section-title">安防监控</h2>
              <p class="section-desc">门窗传感器与移动侦测状态一览。</p>
            </div>
          </div>
          <div class="bento-grid">
            <!-- Alarm Status -->
            <div class="card-power" :class="{ 'is-off': !alarmArmed }" style="grid-row:span 1" @click="alarmArmed = !alarmArmed">
              <div class="card-power-overlay"></div>
              <div class="card-power-top">
                <div class="card-power-icon-wrap">
                  <span class="material-symbols-outlined text-[40px]" style="font-variation-settings:'FILL'1">{{ alarmArmed ? 'shield_lock' : 'shield' }}</span>
                </div>
                <span class="card-power-badge">{{ alarmArmed ? '已布防' : '已撤防' }}</span>
              </div>
              <div class="card-power-bot">
                <h3 class="card-power-title">{{ alarmArmed ? '安防已激活' : '安防已解除' }}</h3>
                <p class="card-power-hint">{{ alarmArmed ? '点击撤防' : '点击布防' }}</p>
              </div>
            </div>

            <!-- Door Sensor -->
            <div class="card-sensor">
              <div class="sensor-icon-wrap" style="background:rgba(52,211,153,0.15);border:1px solid rgba(52,211,153,0.3)">
                <span class="material-symbols-outlined text-[32px]" style="color:#34d399">door_front</span>
              </div>
              <div>
                <p class="sensor-label">入户门</p>
                <div class="sensor-value-row">
                  <span class="presence-state" style="color:#34d399;text-shadow:0 0 12px rgba(52,211,153,0.4)">已关闭</span>
                </div>
                <p class="climate-sub">最后开启: 2 小时前</p>
              </div>
            </div>

            <!-- Window Sensor 1 -->
            <div class="card-sensor">
              <div class="sensor-icon-wrap" style="background:rgba(52,211,153,0.15);border:1px solid rgba(52,211,153,0.3)">
                <span class="material-symbols-outlined text-[32px]" style="color:#34d399">window</span>
              </div>
              <div>
                <p class="sensor-label">客厅窗户</p>
                <div class="sensor-value-row">
                  <span class="presence-state" style="color:#34d399;text-shadow:0 0 12px rgba(52,211,153,0.4)">已关闭</span>
                </div>
                <p class="climate-sub">状态正常</p>
              </div>
            </div>

            <!-- Window Sensor 2 -->
            <div class="card-sensor">
              <div class="sensor-icon-wrap" style="background:rgba(255,180,171,0.15);border:1px solid rgba(255,180,171,0.3)">
                <span class="material-symbols-outlined text-[32px]" style="color:#ffb4ab">window_open</span>
              </div>
              <div>
                <p class="sensor-label">厨房窗户</p>
                <div class="sensor-value-row">
                  <span class="presence-state" style="color:#ffb4ab;text-shadow:0 0 12px rgba(255,180,171,0.4)">已开启</span>
                </div>
                <p class="climate-sub">已开启 30 分钟</p>
              </div>
            </div>

            <!-- Motion Detector -->
            <div class="card-sensor card-sensor-presence">
              <div class="presence-pulse"></div>
              <div class="sensor-icon-wrap sensor-icon-primary">
                <span class="presence-ring"></span>
                <span class="material-symbols-outlined text-primary text-[32px]" style="font-variation-settings:'FILL'1">motion_sensor_active</span>
              </div>
              <div>
                <p class="sensor-label">移动侦测</p>
                <h4 class="presence-state">{{ humanText }}</h4>
                <p class="climate-sub">客厅区域</p>
              </div>
            </div>

            <!-- Camera Status -->
            <div class="card-sensor">
              <div class="sensor-icon-wrap" style="background:rgba(96,165,250,0.15);border:1px solid rgba(96,165,250,0.3)">
                <span class="material-symbols-outlined text-[32px]" style="color:#60a5fa">videocam</span>
              </div>
              <div>
                <p class="sensor-label">摄像头</p>
                <div class="sensor-value-row">
                  <span class="presence-state" style="color:#60a5fa;text-shadow:0 0 12px rgba(96,165,250,0.4)">3 路在线</span>
                </div>
                <p class="climate-sub">门口 · 客厅 · 阳台</p>
              </div>
            </div>
          </div>
        </template>

        <!-- ========== PAGE: Settings 系统设置 ========== -->
        <template v-if="currentPage === 'settings'">
          <div class="section-header">
            <div>
              <h2 class="section-title">系统设置</h2>
              <p class="section-desc">配置后端连接、查看系统信息。</p>
            </div>
          </div>
          <div class="bento-grid">
            <!-- Backend URL Config -->
            <div class="settings-card settings-card-wide">
              <div class="settings-card-head">
                <span class="material-symbols-outlined text-[28px] text-primary">dns</span>
                <h3 class="settings-card-title">后端 WebSocket 地址</h3>
              </div>
              <div class="conn-input-row" style="margin-top:16px">
                <input
                  v-model="backendUrlInput"
                  class="conn-input settings-input"
                  placeholder="ws://your-server:8080/ws"
                  type="text"
                />
                <button class="conn-save-btn" @click="saveBackendUrl">保存并重连</button>
              </div>
              <button class="settings-reset" @click="resetBackendUrl">恢复默认 (ws://localhost:8080/ws)</button>
            </div>

            <!-- Connection Info -->
            <div class="settings-card">
              <div class="settings-card-head">
                <span class="material-symbols-outlined text-[28px]" :style="isConnected?'color:#34d399':'color:#6b7280'">{{ isConnected ? 'cloud_done' : 'cloud_off' }}</span>
                <h3 class="settings-card-title">连接状态</h3>
              </div>
              <p class="settings-value">{{ isConnected ? '已连接到后端' : '未连接' }}</p>
              <p class="settings-hint" v-if="isConnected">WebSocket 通信正常</p>
              <p class="settings-hint" v-else>请检查后端地址是否正确，或后端是否已启动</p>
            </div>

            <!-- System Info -->
            <div class="settings-card">
              <div class="settings-card-head">
                <span class="material-symbols-outlined text-[28px] text-on-surface-variant">info</span>
                <h3 class="settings-card-title">关于系统</h3>
              </div>
              <p class="settings-value">Lumina Home v1.0</p>
              <p class="settings-hint">智能室内环境控制中枢</p>
              <p class="settings-hint">Golang + Vue3 · WebSocket + MQTT</p>
            </div>

            <!-- Privacy -->
            <div class="settings-card">
              <div class="settings-card-head">
                <span class="material-symbols-outlined text-[28px] text-on-surface-variant">privacy_tip</span>
                <h3 class="settings-card-title">数据说明</h3>
              </div>
              <p class="settings-hint">后端地址仅保存在您的浏览器本地存储中，不会上传到任何第三方服务器。所有传感器数据通过 WebSocket 实时传输，不经过云端。</p>
            </div>
          </div>
        </template>

      </div>
    </main>
  </div>
</template>

<script setup>
import { computed, onMounted, onUnmounted, reactive, ref } from 'vue'

// ==================== Page Navigation ====================
const currentPage = ref('dashboard')

// ==================== Device Shadow ====================
const deviceShadow = reactive({
  mode: 'manual',
  power: 'off',
  blind_angle: 90,
  light_level: 2,
  lux: 450,
  human: true,
  timestamp: 0,
})

// ==================== WebSocket ====================
const isConnected = ref(false)
const currentClock = ref(new Date())
let clockTimer = null
let ws = null
let reconnectTimer = null

const DEFAULT_WS_URL = 'ws://localhost:8080/ws'
const backendUrlInput = ref(DEFAULT_WS_URL)
const showConnPanel = ref(false)

// ==================== Zone Lighting State ====================
const zoneLiving = reactive({ on: true, level: 80 })
const zoneBedroom = reactive({ on: false, level: 0 })
const zoneKitchen = reactive({ on: true, level: 60 })
const zoneBathroom = reactive({ on: false, level: 0 })
const zoneStudy = reactive({ on: true, level: 45 })

// ==================== Security State ====================
const alarmArmed = ref(false)

// ==================== Constants ====================
const lightOptions = [
  { value: 0, label: '关闭' },
  { value: 1, label: '低亮' },
  { value: 2, label: '中亮' },
  { value: 3, label: '高亮' },
]

// ==================== Computed ====================
const isAuto = computed(() => deviceShadow.mode === 'auto')
const luxText = computed(() => Number(deviceShadow.lux || 0).toFixed(0))
const humanText = computed(() => (deviceShadow.human ? '有人活动' : '无人'))
const connectionText = computed(() => (isConnected.value ? '设备已连接' : '正在连接'))
const connectionClass = computed(() => (isConnected.value ? 'conn-online' : 'conn-offline'))
const powerTitle = computed(() => (deviceShadow.power === 'on' ? '系统已开启' : '系统已关闭'))
const currentTimeText = computed(() => currentClock.value.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' }))

// ==================== WebSocket ====================
function connectWS() {
  if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null }
  if (ws) { ws.close(); ws = null }

  const url = backendUrlInput.value || DEFAULT_WS_URL
  try {
    ws = new WebSocket(url)
  } catch (e) {
    isConnected.value = false
    scheduleReconnect()
    return
  }

  ws.onopen = () => { isConnected.value = true }
  ws.onmessage = (event) => {
    try {
      const payload = JSON.parse(event.data)
      Object.assign(deviceShadow, payload)
    } catch (error) {
      console.error('WS 数据解析失败', error)
    }
  }
  ws.onclose = () => { isConnected.value = false; scheduleReconnect() }
  ws.onerror = () => { isConnected.value = false }
}

function scheduleReconnect() {
  if (reconnectTimer) clearTimeout(reconnectTimer)
  reconnectTimer = setTimeout(connectWS, 3000)
}

function reconnect() {
  if (ws) ws.close()
  connectWS()
}

function syncState() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    deviceShadow.timestamp = Math.floor(Date.now() / 1000)
    ws.send(JSON.stringify(deviceShadow))
  }
}

// ==================== Backend URL ====================
function saveBackendUrl() {
  const url = backendUrlInput.value.trim()
  if (!url) {
    backendUrlInput.value = DEFAULT_WS_URL
  } else {
    backendUrlInput.value = url
  }
  localStorage.setItem('lumina_backend_url', backendUrlInput.value)
  showConnPanel.value = false
  reconnect()
}

function resetBackendUrl() {
  backendUrlInput.value = DEFAULT_WS_URL
  localStorage.setItem('lumina_backend_url', DEFAULT_WS_URL)
  reconnect()
}

// ==================== Actions ====================
function setMode(mode) {
  if (deviceShadow.mode === mode) return
  deviceShadow.mode = mode
  syncState()
}

function setLightLevel(level) {
  if (isAuto.value) return
  deviceShadow.light_level = level
  syncState()
}

function togglePower() {
  if (isAuto.value) return
  deviceShadow.power = deviceShadow.power === 'on' ? 'off' : 'on'
  syncState()
}

function lightButtonClass(level) {
  return { 'light-btn-active': deviceShadow.light_level === level }
}

function toggleZone(zone) {
  const map = { living: zoneLiving, bedroom: zoneBedroom, kitchen: zoneKitchen, bathroom: zoneBathroom, study: zoneStudy }
  const z = map[zone]
  if (!z) return
  z.on = !z.on
  if (z.on && z.level === 0) z.level = 50
}

function allZonesOff() {
  ;[zoneLiving, zoneBedroom, zoneKitchen, zoneBathroom, zoneStudy].forEach(z => { z.on = false })
}

// ==================== Lifecycle ====================
onMounted(() => {
  // Restore backend URL
  const saved = localStorage.getItem('lumina_backend_url')
  if (saved) backendUrlInput.value = saved

  currentClock.value = new Date()
  clockTimer = setInterval(() => { currentClock.value = new Date() }, 1000)

  connectWS()
})

onUnmounted(() => {
  if (clockTimer) clearInterval(clockTimer)
  if (reconnectTimer) clearTimeout(reconnectTimer)
  if (ws) ws.close()
})
</script>