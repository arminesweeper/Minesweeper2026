#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

// WiFi Access Point Credentials
const char *ssid = "Minesweeper_Bot";
const char *password = "12345678"; // At least 8 characters

ESP8266WebServer server(80);

// Telemetry cached from Arduino Mega
String proximityData = "[0,0,0,0,0]";
String batteryData = "0.0";
uint8_t metalDetected = 0;
uint16_t ultrasonicCm = 0;
String liftState = "IDLE";
uint8_t magnetState = 0; // Default OFF
uint8_t topSwitchPressed = 0;
uint8_t botSwitchPressed = 0;
String lastMsg = "Ready";

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Minesweeper Command Center</title>
  <style>
    :root {
      --bg-dark: #080c14;
      --bg-card: rgba(16, 23, 38, 0.75);
      --border-card: rgba(0, 240, 255, 0.15);
      --border-card-hover: rgba(0, 240, 255, 0.4);
      --cyan: #00f0ff;
      --cyan-glow: rgba(0, 240, 255, 0.35);
      --emerald: #00ff88;
      --emerald-glow: rgba(0, 255, 136, 0.35);
      --amber: #ffaa00;
      --red: #ff2a5f;
      --red-glow: rgba(255, 42, 95, 0.45);
      --text-main: #f0f6fc;
      --text-dim: #7d8b9f;
      --font-stack: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    }

    * { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; }

    body {
      background: radial-gradient(circle at 50% 0%, #151d30 0%, var(--bg-dark) 75%);
      color: var(--text-main);
      font-family: var(--font-stack);
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 12px;
      touch-action: manipulation;
      user-select: none;
    }

    /* Header Bar */
    .header {
      width: 100%;
      max-width: 1200px;
      background: var(--bg-card);
      backdrop-filter: blur(16px);
      border: 1px solid var(--border-card);
      border-radius: 16px;
      padding: 14px 20px;
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      align-items: center;
      gap: 12px;
      margin-bottom: 12px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.4);
    }

    .title-group h1 {
      font-size: 20px;
      font-weight: 800;
      letter-spacing: 1.5px;
      text-transform: uppercase;
      background: linear-gradient(90deg, var(--cyan), #7928ca);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    .title-group .sub {
      font-size: 11px;
      color: var(--text-dim);
      letter-spacing: 1px;
      text-transform: uppercase;
    }

    .header-badges {
      display: flex;
      gap: 10px;
      align-items: center;
      flex-wrap: wrap;
    }

    .badge {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      padding: 6px 12px;
      border-radius: 20px;
      font-size: 12px;
      font-weight: 600;
      background: rgba(255,255,255,0.06);
      border: 1px solid rgba(255,255,255,0.1);
    }
    .badge-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--emerald);
      box-shadow: 0 0 8px var(--emerald);
      animation: pulse 2s infinite;
    }

    @keyframes pulse {
      0%, 100% { transform: scale(1); opacity: 1; }
      50% { transform: scale(1.3); opacity: 0.6; }
    }

    /* Mine Detection Banner */
    .mine-alert-banner {
      width: 100%;
      max-width: 1200px;
      padding: 14px 20px;
      border-radius: 14px;
      margin-bottom: 12px;
      display: none;
      align-items: center;
      justify-content: space-between;
      background: linear-gradient(90deg, rgba(255, 42, 95, 0.9), rgba(180, 20, 60, 0.9));
      box-shadow: 0 0 30px var(--red-glow);
      animation: flashAlert 0.8s infinite alternate;
      border: 1px solid rgba(255, 255, 255, 0.3);
    }
    .mine-alert-banner.active { display: flex; }

    @keyframes flashAlert {
      from { box-shadow: 0 0 15px var(--red-glow); transform: scale(0.995); }
      to { box-shadow: 0 0 35px rgba(255, 42, 95, 0.9); transform: scale(1); }
    }

    .mine-alert-text {
      display: flex;
      align-items: center;
      gap: 10px;
      font-size: 16px;
      font-weight: 800;
      letter-spacing: 1px;
      text-transform: uppercase;
    }

    /* Capture Range Banner */
    .capture-banner {
      width: 100%;
      max-width: 1200px;
      padding: 10px 18px;
      border-radius: 12px;
      margin-bottom: 12px;
      display: none;
      align-items: center;
      justify-content: space-between;
      background: linear-gradient(90deg, rgba(0, 255, 136, 0.2), rgba(0, 240, 255, 0.2));
      border: 1px solid var(--emerald);
      box-shadow: 0 0 20px var(--emerald-glow);
    }
    .capture-banner.active { display: flex; }

    /* Dashboard Layout Grid */
    .dashboard-grid {
      width: 100%;
      max-width: 1200px;
      display: grid;
      grid-template-columns: 1fr;
      gap: 14px;
    }
    @media (min-width: 768px) {
      .dashboard-grid { grid-template-columns: 1fr 1fr; }
    }
    @media (min-width: 1024px) {
      .dashboard-grid { grid-template-columns: 1.1fr 1.1fr 1fr; }
    }

    /* Cards */
    .card {
      background: var(--bg-card);
      backdrop-filter: blur(14px);
      border: 1px solid var(--border-card);
      border-radius: 18px;
      padding: 18px;
      display: flex;
      flex-direction: column;
      gap: 14px;
      box-shadow: 0 6px 24px rgba(0,0,0,0.3);
      transition: border-color 0.2s;
    }
    .card:hover { border-color: var(--border-card-hover); }

    .card-title {
      font-size: 13px;
      font-weight: 700;
      color: var(--text-dim);
      text-transform: uppercase;
      letter-spacing: 1px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }

    /* Hero Cycle Button */
    .btn-hero-cycle {
      width: 100%;
      padding: 20px 16px;
      border: none;
      border-radius: 16px;
      background: linear-gradient(135deg, #00f0ff 0%, #00ff88 50%, #0088ff 100%);
      color: #050b14;
      font-size: 18px;
      font-weight: 900;
      letter-spacing: 1.5px;
      text-transform: uppercase;
      cursor: pointer;
      box-shadow: 0 0 25px var(--cyan-glow);
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 10px;
      transition: transform 0.1s, box-shadow 0.2s;
    }
    .btn-hero-cycle:active {
      transform: scale(0.97);
      box-shadow: 0 0 40px rgba(0, 255, 136, 0.8);
    }
    .btn-hero-cycle:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }

    /* Virtual Touch Joystick */
    .joystick-container {
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      gap: 10px;
      margin: 8px 0;
    }
    #joyCanvas {
      background: radial-gradient(circle, rgba(0,240,255,0.06) 0%, rgba(0,0,0,0.4) 70%);
      border: 2px dashed rgba(0,240,255,0.25);
      border-radius: 50%;
      touch-action: none;
      box-shadow: inset 0 0 20px rgba(0,0,0,0.6);
      cursor: crosshair;
    }

    /* Speed Selector Pills */
    .speed-selector {
      display: flex;
      gap: 8px;
      width: 100%;
    }
    .speed-btn {
      flex: 1;
      padding: 8px;
      border-radius: 10px;
      background: rgba(255,255,255,0.05);
      border: 1px solid rgba(255,255,255,0.1);
      color: var(--text-dim);
      font-size: 11px;
      font-weight: 700;
      cursor: pointer;
      text-transform: uppercase;
      transition: all 0.2s;
    }
    .speed-btn.active {
      background: var(--cyan);
      color: #000;
      border-color: var(--cyan);
      box-shadow: 0 0 12px var(--cyan-glow);
    }

    /* D-Pad Buttons */
    .d-pad {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 8px;
      width: 100%;
    }
    .btn-dir {
      height: 52px;
      border-radius: 12px;
      background: rgba(255,255,255,0.05);
      border: 1px solid rgba(255,255,255,0.08);
      color: var(--text-main);
      font-size: 18px;
      font-weight: 700;
      display: flex;
      align-items: center;
      justify-content: center;
      cursor: pointer;
      transition: all 0.15s;
    }
    .btn-dir:active, .btn-dir.pressed {
      background: rgba(0,240,255,0.25);
      border-color: var(--cyan);
      color: var(--cyan);
      transform: scale(0.95);
    }
    .btn-dir.empty { visibility: hidden; }

    /* Action Buttons */
    .btn-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 8px;
    }
    .btn-act {
      padding: 12px;
      border-radius: 12px;
      background: rgba(255,255,255,0.06);
      border: 1px solid rgba(255,255,255,0.1);
      color: var(--text-main);
      font-size: 12px;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 0.5px;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 6px;
      transition: all 0.15s;
    }
    .btn-act:active {
      transform: scale(0.96);
      border-color: var(--cyan);
      box-shadow: 0 0 14px var(--cyan-glow);
    }
    .btn-act.emerald { border-color: rgba(0, 255, 136, 0.4); color: var(--emerald); }
    .btn-act.emerald:active { background: rgba(0, 255, 136, 0.2); }
    .btn-act.red { border-color: rgba(255, 42, 95, 0.4); color: var(--red); }
    .btn-act.red:active { background: rgba(255, 42, 95, 0.2); }
    
    .btn-estop {
      width: 100%;
      padding: 14px;
      border-radius: 12px;
      background: linear-gradient(135deg, rgba(255,42,95,0.8), rgba(180,20,50,0.8));
      border: 1px solid rgba(255,42,95,0.5);
      color: white;
      font-size: 14px;
      font-weight: 900;
      letter-spacing: 1px;
      text-transform: uppercase;
      cursor: pointer;
      box-shadow: 0 4px 15px rgba(255,42,95,0.3);
      transition: transform 0.1s;
    }
    .btn-estop:active { transform: scale(0.97); }

    /* Manual Lift Buttons */
    .btn-lift-up {
      padding: 14px 10px;
      border-radius: 12px;
      background: linear-gradient(135deg, rgba(0,240,255,0.2), rgba(0,102,170,0.3));
      border: 1px solid rgba(0,240,255,0.5);
      color: #fff;
      font-size: 13px;
      font-weight: 800;
      letter-spacing: 0.5px;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      transition: all 0.15s;
      user-select: none;
      -webkit-user-select: none;
    }
    .btn-lift-up:active, .btn-lift-up.pressed {
      background: var(--cyan);
      color: #000;
      box-shadow: 0 0 16px var(--cyan-glow);
      transform: scale(0.97);
    }
    .btn-lift-dn {
      padding: 14px 10px;
      border-radius: 12px;
      background: linear-gradient(135deg, rgba(0,240,255,0.12), rgba(0,102,170,0.2));
      border: 1px solid rgba(0,240,255,0.3);
      color: #fff;
      font-size: 13px;
      font-weight: 800;
      letter-spacing: 0.5px;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      transition: all 0.15s;
      user-select: none;
      -webkit-user-select: none;
    }
    .btn-lift-dn:active, .btn-lift-dn.pressed {
      background: var(--cyan);
      color: #000;
      box-shadow: 0 0 16px var(--cyan-glow);
      transform: scale(0.97);
    }
    .btn-lift-stop {
      width: 100%;
      padding: 12px;
      border-radius: 12px;
      background: linear-gradient(135deg, rgba(255,42,95,0.25), rgba(180,20,50,0.35));
      border: 1px solid rgba(255,42,95,0.6);
      color: #fff;
      font-size: 13px;
      font-weight: 900;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 6px;
      transition: all 0.15s;
      user-select: none;
      -webkit-user-select: none;
    }
    .btn-lift-stop:active, .btn-lift-stop.pressed {
      background: var(--red);
      color: #fff;
      box-shadow: 0 0 16px rgba(255,42,95,0.6);
      transform: scale(0.97);
    }

    /* Proximity Bars */
    .prox-list {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .prox-item {
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .prox-tag {
      width: 32px;
      font-size: 11px;
      font-weight: 700;
      color: var(--text-dim);
    }
    .prox-track {
      flex: 1;
      height: 12px;
      background: rgba(0,0,0,0.4);
      border-radius: 6px;
      overflow: hidden;
      border: 1px solid rgba(255,255,255,0.05);
    }
    .prox-fill {
      height: 100%;
      width: 0%;
      background: linear-gradient(90deg, var(--cyan), var(--emerald));
      border-radius: 6px;
      transition: width 0.2s cubic-bezier(0.4, 0, 0.2, 1);
    }
    .prox-fill.triggered {
      background: linear-gradient(90deg, var(--amber), var(--red));
    }
    .prox-num {
      width: 38px;
      text-align: right;
      font-size: 12px;
      font-family: monospace;
      color: var(--cyan);
    }

    /* Ultrasonic Gauge */
    .us-gauge-box {
      background: rgba(0,0,0,0.3);
      border-radius: 12px;
      padding: 12px;
      border: 1px solid rgba(255,255,255,0.05);
    }
    .us-header {
      display: flex;
      justify-content: space-between;
      font-size: 12px;
      color: var(--text-dim);
      margin-bottom: 6px;
    }
    .us-num {
      font-size: 18px;
      font-weight: 800;
      color: var(--cyan);
    }
    .us-track {
      width: 100%;
      height: 14px;
      background: rgba(0,0,0,0.5);
      border-radius: 7px;
      overflow: hidden;
      border: 1px solid rgba(255,255,255,0.08);
      position: relative;
    }
    .us-fill {
      height: 100%;
      width: 0%;
      background: linear-gradient(90deg, var(--emerald), var(--cyan));
      border-radius: 7px;
      transition: width 0.2s ease;
    }
    .us-marker {
      position: absolute;
      top: 0;
      left: 15%; /* 15 cm zone */
      width: 2px;
      height: 100%;
      background: var(--amber);
      box-shadow: 0 0 6px var(--amber);
    }

    /* Camera Servo Slider */
    .slider-box {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .slider-header {
      display: flex;
      justify-content: space-between;
      font-size: 12px;
      color: var(--text-dim);
    }
    .slider {
      -webkit-appearance: none;
      width: 100%;
      height: 8px;
      background: rgba(0,0,0,0.5);
      border-radius: 4px;
      border: 1px solid var(--border-card);
      outline: none;
    }
    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 22px;
      height: 22px;
      border-radius: 50%;
      background: var(--cyan);
      box-shadow: 0 0 10px var(--cyan);
      cursor: pointer;
    }
    .servo-presets {
      display: flex;
      gap: 6px;
    }
    .preset-btn {
      flex: 1;
      padding: 6px;
      font-size: 11px;
      font-weight: 600;
      border-radius: 8px;
      background: rgba(255,255,255,0.05);
      border: 1px solid rgba(255,255,255,0.1);
      color: var(--text-dim);
      cursor: pointer;
    }

    /* Toast Notification */
    #toast {
      position: fixed;
      bottom: -60px;
      left: 50%;
      transform: translateX(-50%);
      background: var(--cyan);
      color: #000;
      padding: 10px 22px;
      border-radius: 24px;
      font-size: 13px;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 1px;
      transition: bottom 0.3s cubic-bezier(0.18, 0.89, 0.32, 1.28);
      z-index: 999;
      box-shadow: 0 8px 24px rgba(0,0,0,0.5);
    }
    #toast.show { bottom: 25px; }

    /* Footer & Keybinds hint */
    .footer-hints {
      margin-top: 14px;
      font-size: 11px;
      color: var(--text-dim);
      text-align: center;
      line-height: 1.6;
    }
    .key-badge {
      display: inline-block;
      padding: 2px 6px;
      border-radius: 4px;
      background: rgba(255,255,255,0.1);
      border: 1px solid rgba(255,255,255,0.2);
      color: var(--text-main);
      font-family: monospace;
      font-weight: bold;
    }
  </style>
</head>
<body>

  <!-- Header -->
  <div class="header">
    <div class="title-group">
      <h1>Minesweeper Command Center</h1>
      <div class="sub">Autonomous & Tactical Teleop System</div>
    </div>
    <div class="header-badges">
      <div class="badge">
        <span class="badge-dot"></span>
        <span id="conn-stat">CONNECTED</span>
      </div>
      <div class="badge" id="gamepad-badge" style="display:none; color: var(--emerald);">
        🎮 <span id="gamepad-name">Gamepad</span>
      </div>
      <div class="badge">
        🔋 <span id="batt-val" style="color: var(--cyan); font-weight:800;">--</span> V
        <span id="batt-status" style="font-size: 10px; margin-left: 4px; opacity: 0.8;">OK</span>
      </div>
      <button id="audio-toggle-btn" class="badge" onclick="toggleAudio()" style="cursor:pointer; background:rgba(0,240,255,0.1); border-color:var(--cyan); color:var(--cyan);">
        🔊 Audio: ON
      </button>
    </div>
  </div>

  <!-- Metal Detector Alert Banner -->
  <div class="mine-alert-banner" id="mineBanner">
    <div class="mine-alert-text">
      <span>⚡</span>
      <span>METAL DETECTED — TARGET UNDER COILS!</span>
    </div>
    <button class="btn-act" style="background:#fff; color:#000; border:none; padding:8px 16px; font-weight:900;" onclick="sendCmd('magnet/on', 'Magnets Armed')">
      ARM MAGNETS
    </button>
  </div>

  <!-- Capture Range Banner -->
  <div class="capture-banner" id="captureBanner">
    <div style="display: flex; align-items:center; gap:8px; font-weight:700; font-size:13px; color:var(--emerald);">
      <span>🎯</span>
      <span>TARGET IN GRIPPER RANGE (&le; 15 cm)</span>
    </div>
    <button class="btn-act emerald" style="padding:6px 14px;" onclick="sendCmd('magnet/on', 'Magnets ON')">ARM MAGNETS</button>
  </div>

  <!-- 3-Column Dashboard Grid -->
  <div class="dashboard-grid">

    <!-- Column 1: Motion & Joystick -->
    <div class="card">
      <div class="card-title">
        <span>Motion Control</span>
        <span id="speed-label" style="color:var(--cyan);">Normal</span>
      </div>

      <!-- Speed selector -->
      <div class="speed-selector">
        <button class="speed-btn" onclick="setSpeedMode('slow', 3.0)">Slow (3 rad/s)</button>
        <button class="speed-btn active" onclick="setSpeedMode('normal', 6.0)">Normal (6 rad/s)</button>
        <button class="speed-btn" onclick="setSpeedMode('turbo', 10.0)">Turbo (10 rad/s)</button>
      </div>

      <!-- Touch Canvas Joystick -->
      <div class="joystick-container">
        <canvas id="joyCanvas" width="180" height="180"></canvas>
        <div style="font-size: 11px; color: var(--text-dim);">Touch / Drag or use Gamepad Stick</div>
      </div>

      <!-- Directional D-Pad -->
      <div class="d-pad">
        <div class="btn-dir empty"></div>
        <button class="btn-dir" id="btn-fwd" onmousedown="handleDirDown('forward')" onmouseup="handleDirUp()" onmouseleave="handleDirUp()" ontouchstart="handleDirDown('forward'); event.preventDefault();" ontouchend="handleDirUp(); event.preventDefault();">▲</button>
        <div class="btn-dir empty"></div>
        <button class="btn-dir" id="btn-left" onmousedown="handleDirDown('left')" onmouseup="handleDirUp()" onmouseleave="handleDirUp()" ontouchstart="handleDirDown('left'); event.preventDefault();" ontouchend="handleDirUp(); event.preventDefault();">◀</button>
        <button class="btn-dir" id="btn-stop" style="color:var(--red); font-weight:900;" onclick="sendStop()">STOP</button>
        <button class="btn-dir" id="btn-right" onmousedown="handleDirDown('right')" onmouseup="handleDirUp()" onmouseleave="handleDirUp()" ontouchstart="handleDirDown('right'); event.preventDefault();" ontouchend="handleDirUp(); event.preventDefault();">▶</button>
        <div class="btn-dir empty"></div>
        <button class="btn-dir" id="btn-back" onmousedown="handleDirDown('backward')" onmouseup="handleDirUp()" onmouseleave="handleDirUp()" ontouchstart="handleDirDown('backward'); event.preventDefault();" ontouchend="handleDirUp(); event.preventDefault();">▼</button>
        <div class="btn-dir empty"></div>
      </div>
    </div>

    <!-- Column 2: Mine Catching & Mechanism (Fully Manual) -->
    <div class="card">
      <div class="card-title">
        <span>Gripper Mechanism</span>
        <span id="lift-state-badge" style="color:var(--emerald); font-weight:700;">IDLE</span>
      </div>

      <!-- Manual Gripper Control Panel -->
      <div style="background:rgba(0,0,0,0.35); padding:14px; border-radius:14px; border:1px solid rgba(0,240,255,0.15); display:flex; flex-direction:column; gap:10px;">
        <div style="display:flex; justify-content:space-between; align-items:center;">
          <span style="font-size:11px; font-weight:800; color:var(--cyan); text-transform:uppercase; letter-spacing:0.5px;">Manual Lift Motor</span>
          <span style="font-size:10px; padding:2px 8px; border-radius:6px; background:rgba(0,255,136,0.12); color:var(--emerald); font-weight:700;">DIRECT DRIVE</span>
        </div>
        <div style="display:grid; grid-template-columns: 1fr 1fr; gap:10px;">
          <button class="btn-lift-up" id="btnLiftUp">
            <span style="font-size:20px;">▲</span>
            <span>HOLD TO RAISE</span>
          </button>
          <button class="btn-lift-dn" id="btnLiftDn">
            <span style="font-size:20px;">▼</span>
            <span>HOLD TO LOWER</span>
          </button>
        </div>
        <button class="btn-lift-stop" id="btnLiftStop" onclick="sendCmd('lift/stop', 'Lift Stopped')">
          <span>■ STOP LIFT MOTOR</span>
        </button>
        <div style="font-size:10px; color:var(--text-dim); text-align:center;">
          Press & hold to drive • Release or click Stop to halt immediately
        </div>
      </div>

      <!-- Limit Switches Status (Bypassed) -->
      <div style="background:rgba(0,0,0,0.25); padding:8px 12px; border-radius:10px; border:1px solid rgba(255,255,255,0.05); display:flex; justify-content:space-between; align-items:center;">
        <div style="display:flex; align-items:center; gap:6px;">
          <span id="top-sw-dot" style="width:8px; height:8px; border-radius:50%; background:#3a4454; transition:all 0.2s;"></span>
          <span style="font-size:10px; color:var(--text-dim);">Top D51: <b id="top-sw-text" style="color:#aaa;">REL</b></span>
        </div>
        <div style="font-size:9px; color:var(--amber); text-transform:uppercase; font-weight:700;">Switches Bypassed</div>
        <div style="display:flex; align-items:center; gap:6px;">
          <span id="bot-sw-dot" style="width:8px; height:8px; border-radius:50%; background:#3a4454; transition:all 0.2s;"></span>
          <span style="font-size:10px; color:var(--text-dim);">Bot D53: <b id="bot-sw-text" style="color:#aaa;">REL</b></span>
        </div>
      </div>

      <!-- Magnet Control & Status -->
      <div style="background:rgba(0,0,0,0.3); padding:12px; border-radius:12px; border:1px solid rgba(255,255,255,0.05); display:flex; justify-content:space-between; align-items:center;">
        <div>
          <div style="font-size:11px; color:var(--text-dim); text-transform:uppercase;">Electromagnets</div>
          <div id="mag-status-text" style="font-size:14px; font-weight:800; color:var(--text-dim);">OFF</div>
          <div style="font-size:9px; color:var(--text-dim); margin-top:2px;">Auto-energizes when mine detected</div>
        </div>
        <div style="display:flex; gap:6px;">
          <button class="btn-act emerald" style="padding:8px 14px; font-size:12px;" onclick="sendCmd('magnet/on', 'Magnets ON')">ON</button>
          <button class="btn-act red" style="padding:8px 14px; font-size:12px;" onclick="sendCmd('magnet/off', 'Magnets OFF')">OFF</button>
        </div>
      </div>

      <!-- Siren & Audio Alerts -->
      <div class="card-title" style="margin-top:4px;">Warning Siren</div>
      <div class="btn-grid">
        <button class="btn-act red" onclick="sendCmd('siren/on', 'Siren ON')">🚨 Siren ON</button>
        <button class="btn-act" onclick="sendCmd('siren/off', 'Siren OFF')">🔇 Siren OFF</button>
      </div>

      <!-- Safety E-Stop -->
      <button class="btn-estop" onclick="sendEStop()">🚨 EMERGENCY STOP</button>
      <div style="display:flex; gap:8px;">
        <button class="btn-act" style="flex:1;" onclick="sendCmd('clear', 'Faults Cleared')">Clear Faults</button>
        <button class="btn-act" style="flex:1;" onclick="sendCmd('reset', 'System Reset')">Reset Stats</button>
      </div>
    </div>

    <!-- Column 3: Sensors & Vision -->
    <div class="card">
      <div class="card-title">
        <span>Target Sensors</span>
        <span id="metal-state-pill" style="font-size:11px; padding:3px 8px; border-radius:10px; background:rgba(255,255,255,0.08); color:var(--text-dim);">No Metal</span>
      </div>

      <!-- Ultrasonic Distance Gauge -->
      <div class="us-gauge-box">
        <div class="us-header">
          <span>Gripper Ultrasonic Distance</span>
          <span class="us-num"><span id="us-val">--</span> cm</span>
        </div>
        <div class="us-track">
          <div class="us-fill" id="us-fill"></div>
          <div class="us-marker" title="Capture Threshold (15cm)"></div>
        </div>
        <div style="display:flex; justify-content:space-between; font-size:9px; color:var(--text-dim); margin-top:4px;">
          <span>0 cm</span>
          <span style="color:var(--amber);">15 cm (Capture)</span>
          <span>100 cm+</span>
        </div>
      </div>

      <!-- 5-Channel Proximity Sensors -->
      <div style="display:flex; justify-content:space-between; align-items:center; margin-top:6px; margin-bottom:4px;">
        <div class="card-title" style="margin:0;">Proximity Array</div>
        <button id="proxToggleBtn" class="btn-act red" style="padding:3px 10px; font-size:10px; border-radius:8px;" onclick="toggleProximity()">PROX: OFF</button>
      </div>
      <div class="prox-list">
        <div class="prox-item"><span class="prox-tag">P1</span><div class="prox-track"><div class="prox-fill" id="pb0"></div></div><span class="prox-num" id="pv0">0</span></div>
        <div class="prox-item"><span class="prox-tag">P2</span><div class="prox-track"><div class="prox-fill" id="pb1"></div></div><span class="prox-num" id="pv1">0</span></div>
        <div class="prox-item"><span class="prox-tag">P3</span><div class="prox-track"><div class="prox-fill" id="pb2"></div></div><span class="prox-num" id="pv2">0</span></div>
        <div class="prox-item"><span class="prox-tag">P4</span><div class="prox-track"><div class="prox-fill" id="pb3"></div></div><span class="prox-num" id="pv3">0</span></div>
        <div class="prox-item"><span class="prox-tag">P5</span><div class="prox-track"><div class="prox-fill" id="pb4"></div></div><span class="prox-num" id="pv4">0</span></div>
      </div>

      <!-- Camera Servo Pan Slider -->
      <div class="card-title" style="margin-top:6px;">Camera Pan Servo</div>
      <div class="slider-box">
        <div class="slider-header">
          <span>Angle</span>
          <span id="servo-label" style="color:var(--cyan); font-weight:700;">90°</span>
        </div>
        <input type="range" min="0" max="180" value="90" class="slider" id="servoSlider" oninput="onServoInput(this.value)" onchange="sendServo(this.value)">
        <div class="servo-presets">
          <button class="preset-btn" onclick="setServoPreset(45)">Left 45°</button>
          <button class="preset-btn" onclick="setServoPreset(90)">Center 90°</button>
          <button class="preset-btn" onclick="setServoPreset(135)">Right 135°</button>
        </div>
      </div>
    </div>

  </div>

  <!-- Keybind hints -->
  <div class="footer-hints">
    <b>Keyboard:</b> <span class="key-badge">W</span><span class="key-badge">A</span><span class="key-badge">S</span><span class="key-badge">D</span> Move | <span class="key-badge">SPACE</span> Stop | <span class="key-badge">U</span>/<span class="key-badge">Y</span> Lift UP | <span class="key-badge">J</span>/<span class="key-badge">X</span> Lift DOWN | <span class="key-badge">M</span> Magnets | <span class="key-badge">B</span> Siren<br>
    <b>Gamepad:</b> Left Stick / D-Pad = Drive | Right Stick Y or <span class="key-badge">RT</span>/<span class="key-badge">LT</span> / <span class="key-badge">Y</span>/<span class="key-badge">X</span> = Lift UP/DOWN | Right Stick X = Camera Servo | <span class="key-badge">RB</span> = Magnets | <span class="key-badge">LB</span> = Siren | <span class="key-badge">L3</span> = Speed | <span class="key-badge">Back</span> = Proximity | <span class="key-badge">Start</span> = E-Stop
  </div>

  <div id="toast">Command Executed</div>

  <script>
    // System & Velocity State
    let maxSpeed = 6.0; // rad/s
    let currentRightVel = 0.0;
    let currentLeftVel = 0.0;
    let driveInterval = null;
    let soundEnabled = true;
    let audioCtx = null;
    let lastMetal = 0;

    // Toast function
    function showToast(msg, isRed = false) {
      const t = document.getElementById('toast');
      t.innerText = msg;
      t.style.background = isRed ? 'var(--red)' : 'var(--cyan)';
      t.style.color = isRed ? '#fff' : '#000';
      t.classList.add('show');
      setTimeout(() => t.classList.remove('show'), 1100);
    }

    // Audio Alert Synthesizer
    function playBeep(freq = 880, duration = 0.12) {
      if (!soundEnabled) return;
      try {
        if (!audioCtx) {
          audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        }
        if (audioCtx.state === 'suspended') {
          audioCtx.resume();
        }
        const osc = audioCtx.createOscillator();
        const gain = audioCtx.createGain();
        osc.type = 'sine';
        osc.frequency.setValueAtTime(freq, audioCtx.currentTime);
        gain.gain.setValueAtTime(0.2, audioCtx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + duration);
        osc.connect(gain);
        gain.connect(audioCtx.destination);
        osc.start();
        osc.stop(audioCtx.currentTime + duration);
      } catch (e) {
        console.error('Audio err', e);
      }
    }

    function toggleAudio() {
      soundEnabled = !soundEnabled;
      const b = document.getElementById('audio-toggle-btn');
      b.innerText = soundEnabled ? '🔊 Audio: ON' : '🔇 Audio: OFF';
      if (soundEnabled) playBeep(600, 0.1);
    }

    // Speed Selector
    function setSpeedMode(mode, val) {
      maxSpeed = val;
      document.querySelectorAll('.speed-btn').forEach(b => b.classList.remove('active'));
      event.target.classList.add('active');
      document.getElementById('speed-label').innerText = mode.toUpperCase() + ' (' + val + ')';
      showToast('Speed: ' + mode.toUpperCase());
    }

    function cycleSpeedMode() {
      const speeds = [
        { mode: 'slow', val: 3.0 },
        { mode: 'normal', val: 6.0 },
        { mode: 'turbo', val: 8.5 }
      ];
      let idx = speeds.findIndex(s => Math.abs(s.val - maxSpeed) < 0.5);
      if (idx < 0) idx = 1;
      let nextIdx = (idx + 1) % speeds.length;
      let next = speeds[nextIdx];
      maxSpeed = next.val;
      document.querySelectorAll('.speed-btn').forEach(b => {
        b.classList.toggle('active', b.innerText.toLowerCase().includes(next.mode));
      });
      document.getElementById('speed-label').innerText = next.mode.toUpperCase() + ' (' + next.val + ')';
      showToast('Speed: ' + next.mode.toUpperCase());
    }

    // API Send Helpers with In-Flight Queueing
    let isDriveInFlight = false;
    let pendingDrive = null;

    function sendDrive(r, l) {
      currentRightVel = r;
      currentLeftVel = l;
      if (isDriveInFlight) {
        pendingDrive = { r, l };
        return;
      }
      isDriveInFlight = true;
      fetch('/drive?r=' + r.toFixed(2) + '&l=' + l.toFixed(2))
        .then(() => {
          isDriveInFlight = false;
          if (pendingDrive) {
            const next = pendingDrive;
            pendingDrive = null;
            sendDrive(next.r, next.l);
          }
        })
        .catch(e => {
          isDriveInFlight = false;
        });
    }

    function sendStop() {
      pendingDrive = null;
      currentRightVel = 0;
      currentLeftVel = 0;
      fetch('/stop').catch(() => {});
      showToast('STOP', true);
    }

    function sendCmd(endpoint, msg) {
      fetch('/' + endpoint).then(r => {
        if (r.ok) showToast(msg || endpoint.toUpperCase());
      }).catch(e => console.error(e));
    }

    function sendEStop() {
      fetch('/estop').then(() => showToast('EMERGENCY STOP', true));
    }

    function triggerHeroCycle() {
      playBeep(900, 0.2);
      sendCmd('magnet/on', 'Magnets Armed');
    }

    // Camera Servo
    function onServoInput(val) {
      document.getElementById('servo-label').innerText = val + '°';
    }
    function sendServo(val) {
      fetch('/servo?angle=' + val).then(() => showToast('Camera ' + val + '°'));
    }
    function setServoPreset(val) {
      document.getElementById('servoSlider').value = val;
      onServoInput(val);
      sendServo(val);
    }

    // D-Pad Touch / Mouse Handling
    let dpadTimer = null;
    function handleDirDown(dir) {
      if (dpadTimer) clearInterval(dpadTimer);
      let r = 0, l = 0;
      if (dir === 'forward') { r = maxSpeed; l = maxSpeed; }
      else if (dir === 'backward') { r = -maxSpeed; l = -maxSpeed; }
      else if (dir === 'left') { r = maxSpeed * 0.7; l = -maxSpeed * 0.7; }
      else if (dir === 'right') { r = -maxSpeed * 0.7; l = maxSpeed * 0.7; }
      sendDrive(r, l);
      dpadTimer = setInterval(() => sendDrive(r, l), 100);
    }
    function handleDirUp() {
      if (dpadTimer) {
        clearInterval(dpadTimer);
        dpadTimer = null;
      }
      sendStop();
    }

    // Keyboard Controller (Continuous Drive Loop)
    const keysPressed = {};
    let keyDriveTimer = null;

    function computeKeyboardVelocities() {
      const isUp = !!(keysPressed['w'] || keysPressed['arrowup']);
      const isDn = !!(keysPressed['s'] || keysPressed['arrowdown']);
      const isLt = !!(keysPressed['a'] || keysPressed['arrowleft']);
      const isRt = !!(keysPressed['d'] || keysPressed['arrowright']);

      // Synchronize visual direction buttons
      const btnFwd = document.getElementById('btn-fwd');
      const btnBack = document.getElementById('btn-back');
      const btnLeft = document.getElementById('btn-left');
      const btnRight = document.getElementById('btn-right');
      if (btnFwd) btnFwd.classList.toggle('pressed', isUp);
      if (btnBack) btnBack.classList.toggle('pressed', isDn);
      if (btnLeft) btnLeft.classList.toggle('pressed', isLt);
      if (btnRight) btnRight.classList.toggle('pressed', isRt);

      let fwd = 0;
      let turn = 0;
      if (isUp) fwd += 1;
      if (isDn) fwd -= 1;
      if (isLt) turn -= 1;
      if (isRt) turn += 1;

      if (fwd === 0 && turn === 0) {
        return null;
      }

      let r = 0, l = 0;
      if (fwd !== 0 && turn === 0) {
        r = fwd * maxSpeed;
        l = fwd * maxSpeed;
      } else if (fwd === 0 && turn !== 0) {
        r = -turn * maxSpeed * 0.75;
        l = turn * maxSpeed * 0.75;
      } else {
        r = (fwd * maxSpeed) - (turn * maxSpeed * 0.4);
        l = (fwd * maxSpeed) + (turn * maxSpeed * 0.4);
      }
      return { r, l };
    }

    function updateKeyboardDrive() {
      const v = computeKeyboardVelocities();
      if (!v) {
        if (keyDriveTimer) {
          clearInterval(keyDriveTimer);
          keyDriveTimer = null;
        }
        sendStop();
        return;
      }

      sendDrive(v.r, v.l);

      if (!keyDriveTimer) {
        keyDriveTimer = setInterval(() => {
          const curV = computeKeyboardVelocities();
          if (!curV) {
            clearInterval(keyDriveTimer);
            keyDriveTimer = null;
            sendStop();
          } else {
            sendDrive(curV.r, curV.l);
          }
        }, 90);
      }
    }

    window.addEventListener('keydown', (e) => {
      if (['INPUT', 'TEXTAREA'].includes(e.target.tagName)) return;
      const k = e.key.toLowerCase();
      if (['w', 's', 'a', 'd', 'arrowup', 'arrowdown', 'arrowleft', 'arrowright'].includes(k)) {
        if (!keysPressed[k]) {
          keysPressed[k] = true;
          updateKeyboardDrive();
        }
      } else if (e.key === ' ') {
        for (const key in keysPressed) delete keysPressed[key];
        if (keyDriveTimer) { clearInterval(keyDriveTimer); keyDriveTimer = null; }
        sendStop();
        sendCmd('lift/stop', 'Lift Stopped');
      } else if (k === 'u' || k === 'y') {
        if (!keysPressed[k]) {
          keysPressed[k] = true;
          sendCmd('lift/up', 'Lift UP');
        }
      } else if (k === 'j' || k === 'x') {
        if (!keysPressed[k]) {
          keysPressed[k] = true;
          sendCmd('lift/down', 'Lift DOWN');
        }
      } else if (k === 'm') {
        if (!keysPressed[k]) {
          keysPressed[k] = true;
          toggleMagnets();
        }
      } else if (k === 'b') {
        if (!keysPressed[k]) {
          keysPressed[k] = true;
          sendCmd('siren/on', 'Siren Alert');
        }
      }
    });

    window.addEventListener('keyup', (e) => {
      const k = e.key.toLowerCase();
      delete keysPressed[k];
      if (['w', 's', 'a', 'd', 'arrowup', 'arrowdown', 'arrowleft', 'arrowright'].includes(k)) {
        updateKeyboardDrive();
      }
      if (k === 'u' || k === 'y' || k === 'j' || k === 'x') {
        sendCmd('lift/stop', 'Lift Stopped');
      }
    });

    window.addEventListener('blur', () => {
      for (const k in keysPressed) delete keysPressed[k];
      if (keyDriveTimer) {
        clearInterval(keyDriveTimer);
        keyDriveTimer = null;
      }
      sendStop();
    });

    // Virtual Touch Joystick Canvas
    const canvas = document.getElementById('joyCanvas');
    const ctx = canvas.getContext('2d');
    const radius = canvas.width / 2;
    let joyActive = false;
    let joyPos = { x: radius, y: radius };

    function drawJoystick(x, y) {
      ctx.clearRect(0, 0, canvas.width, canvas.height);

      // Outer Ring
      ctx.beginPath();
      ctx.arc(radius, radius, radius - 15, 0, Math.PI * 2);
      ctx.strokeStyle = 'rgba(0, 240, 255, 0.4)';
      ctx.lineWidth = 3;
      ctx.stroke();

      // Inner Crosshairs
      ctx.beginPath();
      ctx.moveTo(radius - 20, radius);
      ctx.lineTo(radius + 20, radius);
      ctx.moveTo(radius, radius - 20);
      ctx.lineTo(radius, radius + 20);
      ctx.strokeStyle = 'rgba(255, 255, 255, 0.15)';
      ctx.lineWidth = 1;
      ctx.stroke();

      // Joystick Knob
      const grad = ctx.createRadialGradient(x, y, 4, x, y, 28);
      grad.addColorStop(0, '#00f0ff');
      grad.addColorStop(1, '#0066aa');
      ctx.beginPath();
      ctx.arc(x, y, 26, 0, Math.PI * 2);
      ctx.fillStyle = grad;
      ctx.shadowColor = 'rgba(0, 240, 255, 0.7)';
      ctx.shadowBlur = 15;
      ctx.fill();
      ctx.shadowBlur = 0;
    }
    drawJoystick(radius, radius);

    function updateJoystick(clientX, clientY) {
      const rect = canvas.getBoundingClientRect();
      let dx = clientX - (rect.left + radius);
      let dy = clientY - (rect.top + radius);
      const dist = Math.hypot(dx, dy);
      const maxDist = radius - 30;

      if (dist > maxDist) {
        dx = (dx / dist) * maxDist;
        dy = (dy / dist) * maxDist;
      }
      joyPos = { x: radius + dx, y: radius + dy };
      drawJoystick(joyPos.x, joyPos.y);

      // Normalize normalized X & Y (-1.0 to 1.0)
      const normX = dx / maxDist;
      const normY = -dy / maxDist; // Up is positive

      // Differential Drive mixing
      joyVFwd = normY * maxSpeed;
      joyVTurn = normX * maxSpeed * 0.75;
      const r = joyVFwd - joyVTurn;
      const l = joyVFwd + joyVTurn;
      sendDrive(r, l);
    }

    let joyTimer = null;
    let joyVFwd = 0;
    let joyVTurn = 0;

    function joyStart(e) {
      joyActive = true;
      const t = e.touches ? e.touches[0] : e;
      updateJoystick(t.clientX, t.clientY);
      if (joyTimer) clearInterval(joyTimer);
      joyTimer = setInterval(() => {
        if (joyActive) {
          const r = joyVFwd - joyVTurn;
          const l = joyVFwd + joyVTurn;
          sendDrive(r, l);
        }
      }, 90);
    }
    function joyMove(e) {
      if (!joyActive) return;
      const t = e.touches ? e.touches[0] : e;
      updateJoystick(t.clientX, t.clientY);
      e.preventDefault();
    }
    function joyEnd() {
      if (!joyActive) return;
      joyActive = false;
      if (joyTimer) {
        clearInterval(joyTimer);
        joyTimer = null;
      }
      joyPos = { x: radius, y: radius };
      drawJoystick(radius, radius);
      sendStop();
    }

    canvas.addEventListener('mousedown', joyStart);
    window.addEventListener('mousemove', joyMove);
    window.addEventListener('mouseup', joyEnd);
    canvas.addEventListener('touchstart', joyStart, { passive: false });
    window.addEventListener('touchmove', joyMove, { passive: false });
    window.addEventListener('touchend', joyEnd);
    window.addEventListener('touchcancel', joyEnd);

    // HTML5 Gamepad API Integration
    let gpConnected = false;
    let prevButtons = {};
    let lastGpDriveTime = 0;
    let lastGpServoTime = 0;
    let lastGpLiftCmd = 'stop';

    window.addEventListener('gamepadconnected', (e) => {
      gpConnected = true;
      document.getElementById('gamepad-badge').style.display = 'inline-flex';
      document.getElementById('gamepad-name').innerText = e.gamepad.id.substring(0, 20);
      showToast('Gamepad Connected: ' + e.gamepad.id.substring(0, 16));
    });
    window.addEventListener('gamepaddisconnected', () => {
      gpConnected = false;
      document.getElementById('gamepad-badge').style.display = 'none';
      showToast('Gamepad Disconnected', true);
    });

    function pollGamepad() {
      const gamepads = navigator.getGamepads ? navigator.getGamepads() : [];
      let gp = null;
      for (let i = 0; i < gamepads.length; i++) {
        if (gamepads[i]) { gp = gamepads[i]; break; }
      }
      if (gp) {
        const now = performance.now();

        // 1. Left Analog Stick (Driving & Steering)
        let ax0 = gp.axes[0] || 0; // steering
        let ax1 = gp.axes[1] || 0; // throttle (up is -1)
        const deadzone = 0.12;
        if (Math.abs(ax0) < deadzone) ax0 = 0;
        if (Math.abs(ax1) < deadzone) ax1 = 0;

        let hasDriveInput = false;

        if (Math.abs(ax0) > 0 || Math.abs(ax1) > 0) {
          hasDriveInput = true;
          if (now - lastGpDriveTime >= 90) {
            lastGpDriveTime = now;
            const vFwd = -ax1 * maxSpeed;
            const vTurn = ax0 * maxSpeed * 0.75;
            const r = vFwd - vTurn;
            const l = vFwd + vTurn;
            sendDrive(r, l);
          }
        }

        // 2. Gamepad D-Pad (Directional drive fallback when stick is centered)
        let dUp = gp.buttons[12] && gp.buttons[12].pressed;
        let dDn = gp.buttons[13] && gp.buttons[13].pressed;
        let dLt = gp.buttons[14] && gp.buttons[14].pressed;
        let dRt = gp.buttons[15] && gp.buttons[15].pressed;
        if (!hasDriveInput && (dUp || dDn || dLt || dRt)) {
          hasDriveInput = true;
          if (now - lastGpDriveTime >= 90) {
            lastGpDriveTime = now;
            let fw = (dUp ? 1 : 0) - (dDn ? 1 : 0);
            let tr = (dRt ? 1 : 0) - (dLt ? 1 : 0);
            let r = 0, l = 0;
            if (fw !== 0 && tr === 0) { r = fw * maxSpeed; l = fw * maxSpeed; }
            else if (fw === 0 && tr !== 0) { r = -tr * maxSpeed * 0.75; l = tr * maxSpeed * 0.75; }
            else { r = (fw * maxSpeed) - (tr * maxSpeed * 0.4); l = (fw * maxSpeed) + (tr * maxSpeed * 0.4); }
            sendDrive(r, l);
          }
        }

        if (!hasDriveInput && (currentRightVel !== 0 || currentLeftVel !== 0)) {
          if (!joyActive && !dpadTimer && !keyDriveTimer) {
            sendStop();
          }
        }

        // 3. Right Analog Stick Y & Triggers (RT / LT / Y / X) for Gripper Lift
        let ax3 = gp.axes[3] || 0; // vertical (up is negative)
        let isLtPressed = gp.buttons[6] && (gp.buttons[6].pressed || gp.buttons[6].value > 0.3);
        let isRtPressed = gp.buttons[7] && (gp.buttons[7].pressed || gp.buttons[7].value > 0.3);
        let isYPressed = gp.buttons[3] && gp.buttons[3].pressed;
        let isXPressed = gp.buttons[2] && gp.buttons[2].pressed;

        let gpLiftCmd = 'stop';
        if (ax3 < -0.3 || isRtPressed || isYPressed) {
          gpLiftCmd = 'up';
        } else if (ax3 > 0.3 || isLtPressed || isXPressed) {
          gpLiftCmd = 'down';
        }

        if (gpLiftCmd !== lastGpLiftCmd) {
          if (gpLiftCmd === 'up') {
            sendCmd('lift/up', 'Lift UP');
          } else if (gpLiftCmd === 'down') {
            sendCmd('lift/down', 'Lift DOWN');
          } else if (gpLiftCmd === 'stop') {
            if (lastGpLiftCmd === 'up' || lastGpLiftCmd === 'down') {
              sendCmd('lift/stop', 'Lift Stopped');
            }
          }
          lastGpLiftCmd = gpLiftCmd;
        }

        // 4. Right Analog Stick X for Camera Pan Servo
        let ax2 = gp.axes[2] || 0; // horizontal pan
        if (Math.abs(ax2) > 0.25 && now - lastGpServoTime >= 80) {
          lastGpServoTime = now;
          let currentAngle = parseInt(document.getElementById('servoSlider').value) || 90;
          let newAngle = currentAngle + Math.round(ax2 * 4);
          if (newAngle < 0) newAngle = 0;
          if (newAngle > 180) newAngle = 180;
          if (newAngle !== currentAngle) {
            setServoPreset(newAngle);
          }
        }

        // 5. Button Actions (Edge-triggered)
        // Button A (0): Clear Faults
        if (gp.buttons[0] && gp.buttons[0].pressed && !prevButtons[0]) {
          sendCmd('clear', 'Faults Cleared');
        }
        // Button B (1): Normal Stop
        if (gp.buttons[1] && gp.buttons[1].pressed && !prevButtons[1]) {
          sendStop();
          sendCmd('lift/stop', 'Lift Stopped');
        }
        // LB (4): Siren Alert
        if (gp.buttons[4] && gp.buttons[4].pressed && !prevButtons[4]) {
          sendCmd('siren/on', 'Siren Alert');
        }
        // RB (5): Toggle Electromagnet
        if (gp.buttons[5] && gp.buttons[5].pressed && !prevButtons[5]) {
          toggleMagnets();
        }
        // Back / Select (8): Toggle Proximity Array
        if (gp.buttons[8] && gp.buttons[8].pressed && !prevButtons[8]) {
          toggleProximity();
        }
        // Start (9): Emergency Stop
        if (gp.buttons[9] && gp.buttons[9].pressed && !prevButtons[9]) {
          sendEStop();
        }
        // L3 (10, Stick Click): Cycle Speed Mode
        if (gp.buttons[10] && gp.buttons[10].pressed && !prevButtons[10]) {
          cycleSpeedMode();
        }

        // Save button state for edge detection
        for (let b = 0; b < gp.buttons.length; b++) {
          prevButtons[b] = gp.buttons[b].pressed;
        }
      }
      requestAnimationFrame(pollGamepad);
    }
    requestAnimationFrame(pollGamepad);

    // Telemetry Polling (every 250ms)
    setInterval(() => {
      fetch('/status')
        .then(r => r.json())
        .then(data => {
          // Proximity Array
          if (data.prox && data.prox.length === 5) {
            for (let i = 0; i < 5; i++) {
              const val = data.prox[i];
              document.getElementById('pv' + i).innerText = val;
              const fill = document.getElementById('pb' + i);
              const pct = Math.min(100, Math.max(0, (val / 1023) * 100));
              fill.style.width = pct + '%';
              if (val > 500) {
                fill.classList.add('triggered');
              } else {
                fill.classList.remove('triggered');
              }
            }
          }

          // Metal Detector Alert
          const isMetal = (data.metal === 1);
          const banner = document.getElementById('mineBanner');
          const pill = document.getElementById('metal-state-pill');
          if (isMetal) {
            banner.classList.add('active');
            pill.innerText = 'TARGET DETECTED!';
            pill.style.background = 'var(--red)';
            pill.style.color = '#fff';
            if (lastMetal === 0) playBeep(1000, 0.35); // Edge trigger sound
          } else {
            banner.classList.remove('active');
            pill.innerText = 'No Metal';
            pill.style.background = 'rgba(255,255,255,0.08)';
            pill.style.color = 'var(--text-dim)';
          }
          lastMetal = data.metal;

          // Ultrasonic Distance
          if (data.us !== undefined) {
            const usVal = data.us;
            document.getElementById('us-val').innerText = usVal;
            const usFill = document.getElementById('us-fill');
            const pct = Math.min(100, Math.max(0, (usVal / 100) * 100));
            usFill.style.width = pct + '%';

            const capBanner = document.getElementById('captureBanner');
            if (usVal > 0 && usVal <= 15) {
              capBanner.classList.add('active');
            } else {
              capBanner.classList.remove('active');
            }
          }

          // Lift State
          if (data.lift) {
            const badge = document.getElementById('lift-state-badge');
            badge.innerText = data.lift;
            if (data.lift.includes('RAIS') || data.lift.includes('LOW')) {
              badge.style.color = 'var(--cyan)';
            } else if (data.lift.includes('FAULT')) {
              badge.style.color = 'var(--red)';
            } else {
              badge.style.color = 'var(--emerald)';
            }
          }

          // Limit Switches View
          if (data.top_sw !== undefined) {
            const topDot = document.getElementById('top-sw-dot');
            const topText = document.getElementById('top-sw-text');
            if (data.top_sw === 1) {
              topDot.style.background = 'var(--red)';
              topDot.style.boxShadow = '0 0 10px var(--red)';
              topText.innerText = 'PRESSED';
              topText.style.color = 'var(--red)';
            } else {
              topDot.style.background = '#3a4454';
              topDot.style.boxShadow = 'none';
              topText.innerText = 'RELEASED';
              topText.style.color = 'var(--text-dim)';
            }
          }
          if (data.bot_sw !== undefined) {
            const botDot = document.getElementById('bot-sw-dot');
            const botText = document.getElementById('bot-sw-text');
            if (data.bot_sw === 1) {
              botDot.style.background = 'var(--amber)';
              botDot.style.boxShadow = '0 0 10px var(--amber)';
              botText.innerText = 'PRESSED';
              botText.style.color = 'var(--amber)';
            } else {
              botDot.style.background = '#3a4454';
              botDot.style.boxShadow = 'none';
              botText.innerText = 'RELEASED';
              botText.style.color = 'var(--text-dim)';
            }
          }

          // Magnet State
          if (data.mag !== undefined) {
            const magText = document.getElementById('mag-status-text');
            if (data.mag === 1) {
              magText.innerText = 'ENERGIZED (ON)';
              magText.style.color = 'var(--emerald)';
            } else {
              magText.innerText = 'OFF';
              magText.style.color = 'var(--text-dim)';
            }
          }

          // Battery Voltage
          if (data.batt) {
            const v = parseFloat(data.batt);
            const valElem = document.getElementById('batt-val');
            const statElem = document.getElementById('batt-status');
            valElem.innerText = v.toFixed(2);

            if (v < 5.0) {
              statElem.innerText = 'No Batt';
              valElem.style.color = '#888';
            } else if (v < 9.8) {
              statElem.innerText = 'DEAD';
              valElem.style.color = 'var(--red)';
            } else if (v < 11.1) {
              statElem.innerText = 'Low';
              valElem.style.color = 'var(--amber)';
            } else {
              statElem.innerText = 'Good';
              valElem.style.color = 'var(--emerald)';
            }
          }
        })
        .catch(e => console.log('Poll err:', e));
    }, 250);

    // Manual Lift Button Hold-to-Move Bindings
    function setupLiftButton(btnId, moveCmd) {
      const btn = document.getElementById(btnId);
      if (!btn) return;
      let pressing = false;

      const start = (e) => {
        e.preventDefault();
        if (pressing) return;
        pressing = true;
        btn.classList.add('pressed');
        sendCmd(moveCmd, moveCmd);
      };
      const end = (e) => {
        e.preventDefault();
        if (!pressing) return;
        pressing = false;
        btn.classList.remove('pressed');
        sendCmd('lift/stop', 'Lift Stopped');
      };

      btn.addEventListener('mousedown', start);
      btn.addEventListener('mouseup', end);
      btn.addEventListener('mouseleave', end);
      btn.addEventListener('touchstart', start, {passive:false});
      btn.addEventListener('touchend', end, {passive:false});
      btn.addEventListener('touchcancel', end, {passive:false});
    }

    let proxEnabled = false;

    function toggleProximity() {
      proxEnabled = !proxEnabled;
      sendCmd(proxEnabled ? 'prox/on' : 'prox/off', proxEnabled ? 'Proximity Enabled' : 'Proximity Disabled');
      updateProxButton();
    }

    function updateProxButton() {
      const btn = document.getElementById('proxToggleBtn');
      if (btn) {
        if (proxEnabled) {
          btn.innerText = 'PROX: ON';
          btn.className = 'btn-act emerald';
        } else {
          btn.innerText = 'PROX: OFF';
          btn.className = 'btn-act red';
        }
      }
    }

    function toggleMagnets() {
      const magText = document.getElementById('mag-status-text');
      const isCurrentlyOn = magText && magText.innerText.includes('ON');
      sendCmd(isCurrentlyOn ? 'magnet/off' : 'magnet/on', isCurrentlyOn ? 'Magnets OFF' : 'Magnets ON');
    }

    setupLiftButton('btnLiftUp', 'lift/up');
    setupLiftButton('btnLiftDn', 'lift/down');
    updateProxButton();
  </script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", INDEX_HTML); }

// Continuous Drive State (repeater & failsafe)
float activeDriveR = 0.0f;
float activeDriveL = 0.0f;
unsigned long lastClientDriveMs = 0;
unsigned long lastSerialDriveTxMs = 0;
constexpr unsigned long CLIENT_DRIVE_TIMEOUT_MS = 600;

// Continuous analog drive endpoint for joystick & keyboard
void handleDrive() {
  if (server.hasArg("r") && server.hasArg("l")) {
    float r = server.arg("r").toFloat();
    float l = server.arg("l").toFloat();
    if (r > 10.0f) r = 10.0f;
    if (r < -10.0f) r = -10.0f;
    if (l > 10.0f) l = 10.0f;
    if (l < -10.0f) l = -10.0f;

    activeDriveR = r;
    activeDriveL = l;
    lastClientDriveMs = millis();

    char r_sign = (r >= 0.0f) ? 'p' : 'n';
    char l_sign = (l >= 0.0f) ? 'p' : 'n';
    Serial.printf("r%c%05.2f,l%c%05.2f,\n", r_sign, fabs(r), l_sign, fabs(l));
    lastSerialDriveTxMs = millis();
  }
  server.send(200, "text/plain", "OK");
}

// Universal command bridge (sends C<command>)
void handleExtendedCommand() {
  if (server.hasArg("c")) {
    String c = server.arg("c");
    if (!c.startsWith("C")) c = "C" + c;
    Serial.println(c);
  }
  server.send(200, "text/plain", "OK");
}

// Directional helpers
void handleForward() {
  activeDriveR = 4.0f; activeDriveL = 4.0f;
  lastClientDriveMs = millis();
  Serial.println("rp04.00,lp04.00,");
  lastSerialDriveTxMs = millis();
  server.send(200, "text/plain", "OK");
}
void handleBackward() {
  activeDriveR = -4.0f; activeDriveL = -4.0f;
  lastClientDriveMs = millis();
  Serial.println("rn04.00,ln04.00,");
  lastSerialDriveTxMs = millis();
  server.send(200, "text/plain", "OK");
}
void handleLeft() {
  activeDriveR = 3.0f; activeDriveL = -3.0f;
  lastClientDriveMs = millis();
  Serial.println("rp03.00,ln03.00,");
  lastSerialDriveTxMs = millis();
  server.send(200, "text/plain", "OK");
}
void handleRight() {
  activeDriveR = -3.0f; activeDriveL = 3.0f;
  lastClientDriveMs = millis();
  Serial.println("rn03.00,lp03.00,");
  lastSerialDriveTxMs = millis();
  server.send(200, "text/plain", "OK");
}
void handleStop() {
  activeDriveR = 0.0f;
  activeDriveL = 0.0f;
  lastClientDriveMs = 0;
  Serial.println("rp00.00,lp00.00,");
  server.send(200, "text/plain", "OK");
}

// Mechanism helpers
void handleLiftCycle() {
  Serial.println("CLIFT:CYCLE");
  server.send(200, "text/plain", "OK");
}
void handleLiftUp() {
  Serial.println("CLIFT:UP");
  server.send(200, "text/plain", "OK");
}
void handleLiftDown() {
  Serial.println("CLIFT:DN");
  server.send(200, "text/plain", "OK");
}
void handleLiftStop() {
  Serial.println("CLIFT:STOP");
  server.send(200, "text/plain", "OK");
}

void handleMagnetOn() {
  Serial.println("CMAGNET:ON");
  server.send(200, "text/plain", "OK");
}
void handleMagnetOff() {
  Serial.println("CMAGNET:OFF");
  server.send(200, "text/plain", "OK");
}
void handleMagnetToggle() {
  if (magnetState == 1) {
    handleMagnetOff();
  } else {
    handleMagnetOn();
  }
}

void handleProxOn() {
  Serial.println("CPROX:ON");
  server.send(200, "text/plain", "OK");
}
void handleProxOff() {
  Serial.println("CPROX:OFF");
  server.send(200, "text/plain", "OK");
}

void handleSirenOn() {
  Serial.println("CBUZZ:ALERT");
  server.send(200, "text/plain", "OK");
}
void handleSirenOff() {
  Serial.println("CBUZZ:SILENT");
  server.send(200, "text/plain", "OK");
}

void handleEStop() {
  activeDriveR = 0.0f;
  activeDriveL = 0.0f;
  lastClientDriveMs = 0;
  Serial.println("rp00.00,lp00.00,");
  Serial.println("CESTOP");
  server.send(200, "text/plain", "OK");
}
void handleClear() {
  Serial.println("CCLEAR");
  server.send(200, "text/plain", "OK");
}
void handleReset() {
  Serial.println("CRESET");
  server.send(200, "text/plain", "OK");
}

void handleServo() {
  if (server.hasArg("angle")) {
    int angle = server.arg("angle").toInt();
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    Serial.printf("CSERVO:%d\n", angle);
  }
  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  String json = "{\"prox\":" + proximityData +
                ",\"batt\":" + batteryData +
                ",\"metal\":" + String(metalDetected) +
                ",\"us\":" + String(ultrasonicCm) +
                ",\"lift\":\"" + liftState + "\"" +
                ",\"mag\":" + String(magnetState) +
                ",\"top_sw\":" + String(topSwitchPressed) +
                ",\"bot_sw\":" + String(botSwitchPressed) +
                ",\"msg\":\"" + lastMsg + "\"}";
  server.send(200, "application/json", json);
}

void setup() {
  // ESP8266 TX is connected to Arduino Mega RX2 (pin 17)
  // ESP8266 RX is connected to Arduino Mega TX2 (pin 16)
  Serial.begin(115200);

  // Give Mega time to boot
  delay(1500);

  // Setup WiFi Access Point on Channel 6
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password, 6);

  // HTTP Routes
  server.on("/", handleRoot);
  server.on("/drive", handleDrive);
  server.on("/cmd", handleExtendedCommand);
  server.on("/forward", handleForward);
  server.on("/backward", handleBackward);
  server.on("/left", handleLeft);
  server.on("/right", handleRight);
  server.on("/stop", handleStop);

  server.on("/lift/cycle", handleLiftCycle);
  server.on("/lift/up", handleLiftUp);
  server.on("/lift/down", handleLiftDown);
  server.on("/lift/stop", handleLiftStop);

  server.on("/magnet/on", handleMagnetOn);
  server.on("/magnet/off", handleMagnetOff);
  server.on("/magnet/toggle", handleMagnetToggle);

  server.on("/prox/on", handleProxOn);
  server.on("/prox/off", handleProxOff);

  server.on("/siren/on", handleSirenOn);
  server.on("/siren/off", handleSirenOff);

  server.on("/estop", handleEStop);
  server.on("/clear", handleClear);
  server.on("/reset", handleReset);

  server.on("/servo", handleServo);
  server.on("/status", handleStatus);

  server.begin();
}

String serialBuffer = "";

void processSerialData() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      serialBuffer.trim();
      if (serialBuffer.length() > 0) {
        if (serialBuffer.startsWith("P:")) {
          String values = serialBuffer.substring(2);
          values.replace(" ", "");
          proximityData = "[" + values + "]";
        } else if (serialBuffer.startsWith("B:")) {
          batteryData = serialBuffer.substring(2);
        } else if (serialBuffer.startsWith("M:")) {
          metalDetected = (serialBuffer.charAt(2) == '1') ? 1 : 0;
        } else if (serialBuffer.startsWith("U:")) {
          ultrasonicCm = serialBuffer.substring(2).toInt();
        } else if (serialBuffer.startsWith("L:")) {
          // Format: L:<state>,<mag>,<top>,<bot>
          int c1 = serialBuffer.indexOf(',');
          if (c1 > 2) {
            liftState = serialBuffer.substring(2, c1);
            int c2 = serialBuffer.indexOf(',', c1 + 1);
            if (c2 > c1) {
              String maskStr = serialBuffer.substring(c1 + 1, c2);
              maskStr.trim();
              magnetState = (maskStr != "0" && maskStr != "00") ? 1 : 0;
              int c3 = serialBuffer.indexOf(',', c2 + 1);
              if (c3 > c2) {
                topSwitchPressed = (serialBuffer.substring(c2 + 1, c3).toInt() > 0) ? 1 : 0;
                botSwitchPressed = (serialBuffer.substring(c3 + 1).toInt() > 0) ? 1 : 0;
              } else {
                topSwitchPressed = (serialBuffer.substring(c2 + 1).toInt() > 0) ? 1 : 0;
              }
            } else {
              String maskStr = serialBuffer.substring(c1 + 1);
              maskStr.trim();
              magnetState = (maskStr != "0" && maskStr != "00") ? 1 : 0;
            }
          } else {
            liftState = serialBuffer.substring(2);
          }
        } else if (serialBuffer.startsWith("S:")) {
          lastMsg = serialBuffer.substring(2);
        } else if (serialBuffer.startsWith("E:")) {
          lastMsg = "ERR:" + serialBuffer.substring(2);
        } else {
          // Check high-rate telemetry line: rp...lp...p0,m1,g...
          int mIdx = serialBuffer.indexOf(",m");
          if (mIdx >= 0 && (size_t)(mIdx + 2) < serialBuffer.length()) {
            metalDetected = (serialBuffer.charAt(mIdx + 2) == '1') ? 1 : 0;
          }
        }
      }
      serialBuffer = "";
    } else {
      if (serialBuffer.length() < 120) {
        serialBuffer += c;
      } else {
        serialBuffer = "";
      }
    }
  }
}

void loop() {
  server.handleClient();
  processSerialData();

  unsigned long now = millis();
  if (activeDriveR != 0.0f || activeDriveL != 0.0f) {
    if (now - lastClientDriveMs <= CLIENT_DRIVE_TIMEOUT_MS) {
      // Periodically transmit drive velocity frames to Mega to prevent safety timeout
      if (now - lastSerialDriveTxMs >= 70) {
        lastSerialDriveTxMs = now;
        char r_sign = (activeDriveR >= 0.0f) ? 'p' : 'n';
        char l_sign = (activeDriveL >= 0.0f) ? 'p' : 'n';
        Serial.printf("r%c%05.2f,l%c%05.2f,\n", r_sign, fabs(activeDriveR), l_sign, fabs(activeDriveL));
      }
    } else {
      // Client heartbeat timed out -> immediately stop motors
      activeDriveR = 0.0f;
      activeDriveL = 0.0f;
      Serial.println("rp00.00,lp00.00,");
    }
  }
}
