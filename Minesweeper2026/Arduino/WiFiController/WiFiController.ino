#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

// Set these to your desired credentials.
const char *ssid = "Minesweeper_Bot";
const char *password = "12345678"; // Password must be at least 8 characters

ESP8266WebServer server(80);

// HTML for the remote control web page
String proximityData = "[0,0,0,0,0]";

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Minesweeper Command Center</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800&family=Outfit:wght@300;500;700&display=swap');
    
    :root {
      --bg-color: #0b0f19;
      --panel-bg: rgba(20, 25, 40, 0.65);
      --panel-border: rgba(255, 255, 255, 0.08);
      --accent-color: #00ffcc;
      --accent-hover: #00e6b8;
      --stop-color: #ff3366;
      --text-main: #ffffff;
      --text-muted: #8b9bb4;
      --font-primary: 'Inter', sans-serif;
      --font-display: 'Outfit', sans-serif;
    }

    * { box-sizing: border-box; }

    body {
      margin: 0;
      padding: 20px;
      font-family: var(--font-primary);
      background: radial-gradient(circle at top right, #1a233a 0%, var(--bg-color) 70%);
      color: var(--text-main);
      min-height: 100vh;
      overflow-x: hidden;
      display: flex;
      flex-direction: column;
      align-items: center;
      touch-action: manipulation; /* Prevent double-tap zoom */
    }

    .header {
      width: 100%;
      padding: 25px 0 20px;
      text-align: center;
      background: rgba(0, 0, 0, 0.2);
      backdrop-filter: blur(10px);
      border-bottom: 1px solid rgba(255, 255, 255, 0.05);
      margin-bottom: 30px;
      box-shadow: 0 4px 30px rgba(0, 0, 0, 0.5);
      margin-bottom: 25px;
      animation: fadeInDown 0.8s ease;
    }

    h1 {
      margin: 0;
      font-family: var(--font-display);
      font-weight: 800;
      font-size: 28px;
      letter-spacing: 2px;
      text-transform: uppercase;
      background: linear-gradient(90deg, var(--accent-color), #00b3ff);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      text-shadow: 0 0 30px rgba(0, 255, 204, 0.3);
    }

    .subtitle {
      font-size: 12px;
      color: var(--text-muted);
      margin-top: 5px;
      letter-spacing: 1.5px;
      text-transform: uppercase;
    }

    .dashboard {
      display: grid;
      grid-template-columns: 1fr;
      gap: 20px;
      width: 100%;
      max-width: 800px;
    }

    @media (min-width: 768px) {
      .dashboard {
        grid-template-columns: 1fr 1fr;
      }
    }

    .panel {
      background: var(--panel-bg);
      border: 1px solid var(--panel-border);
      border-radius: 20px;
      padding: 25px;
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      box-shadow: 0 10px 40px rgba(0, 0, 0, 0.4), inset 0 1px 0 rgba(255,255,255,0.05);
      animation: fadeIn 1s ease;
      display: flex;
      flex-direction: column;
      justify-content: space-between;
    }

    .panel-title {
      font-family: var(--font-display);
      font-size: 16px;
      font-weight: 700;
      color: var(--text-main);
      margin-bottom: 20px;
      display: flex;
      align-items: center;
      text-transform: uppercase;
      letter-spacing: 1px;
    }

    .container {
      width: 90%;
      max-width: 380px;
      background: var(--panel-bg);
      border-radius: 24px;
      padding: 30px 20px;
      box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3), inset 0 1px 0 rgba(255,255,255,0.1);
      border: 1px solid rgba(255, 255, 255, 0.05);
      backdrop-filter: blur(12px);
    }

    .info-panel {
      display: flex;
      justify-content: space-around;
      margin-bottom: 35px;
      padding: 15px;
      background: rgba(0, 0, 0, 0.4);
      border-radius: 16px;
      border: 1px solid rgba(255,255,255,0.03);
    }

    .info-item {
      display: flex;
      flex-direction: column;
      align-items: center;
    }

.info-label {
  font-size: 11px;
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: 1px;
}

.info-value {
  font-size: 15px;
  font-weight: 500;
  color: var(--accent-color);
  margin-top: 5px;
  display: flex;
  align-items: center;
}

.status-dot {

display: inline-block;
width: 8px;
height: 8px;
background-color: var(--accent-color);
border-radius: 50%;
margin-right: 8px;
box-shadow: 0 0 10px var(--accent-color);
animation: pulse 2s infinite;
}

@keyframes pulse {
  0% { box-shadow: 0 0 0 0 rgba(0, 255, 204, 0.4); }
  70% { box-shadow: 0 0 0 10px rgba(0, 255, 204, 0); }
  100% { box-shadow: 0 0 0 0 rgba(0, 255, 204, 0); }
}

.d-pad {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 15px;
  justify-items: center;
  align-items: center;
      gap: 15px;
    }

    .panel-title::before {
      content: '';
      display: inline-block;
      width: 8px;
      height: 8px;
      background: var(--accent-color);
      border-radius: 50%;
      margin-right: 10px;
      box-shadow: 0 0 10px var(--accent-color);
    }

    /* Proximity Bars */
    .prox-container {
      display: flex;
      flex-direction: column;
      gap: 12px;
    }
    
    .prox-row {
      display: flex;
      align-items: center;
      gap: 15px;
    }
    
    .prox-label {
      width: 30px;
      font-size: 13px;
      color: var(--text-muted);
      font-weight: 600;
    }

    .prox-bar-bg {
      flex: 1;
      height: 14px;
      background: rgba(0,0,0,0.4);
      border-radius: 10px;
      overflow: hidden;
      border: 1px solid rgba(255,255,255,0.05);
    }

    .prox-bar-fill {
      height: 100%;
      width: 0%;
      background: linear-gradient(90deg, #00b3ff, var(--accent-color));
      border-radius: 10px;
      transition: width 0.3s cubic-bezier(0.4, 0, 0.2, 1);
      box-shadow: 0 0 10px rgba(0, 255, 204, 0.5);
    }

    .prox-value {
      width: 40px;
      text-align: right;
      font-size: 13px;
      font-family: monospace;
      color: var(--accent-color);
    }

    /* Controls */
    .btn {
      background: linear-gradient(145deg, rgba(30, 40, 60, 0.7), rgba(15, 20, 30, 0.7));
      border: 1px solid var(--panel-border);
      color: white;
      font-family: var(--font-primary);
      font-size: 14px;
      font-weight: 600;
      border-radius: 14px;
      padding: 15px;
      cursor: pointer;
      transition: all 0.2s;
      box-shadow: 0 4px 15px rgba(0,0,0,0.3);
      outline: none;
      -webkit-tap-highlight-color: transparent;
      display: flex;
      justify-content: center;
      align-items: center;
    }

    .btn:active {
      transform: scale(0.95);
      border-color: var(--accent-color);
      box-shadow: inset 0 2px 10px rgba(0,0,0,0.5), 0 0 15px rgba(0, 255, 204, 0.2);
      color: var(--accent-color);
    }

    .btn-stop {
      background: linear-gradient(145deg, rgba(200, 30, 60, 0.8), rgba(150, 20, 40, 0.8));
      border-color: rgba(255, 51, 102, 0.4);
      color: #fff;
    }
    
    .btn-stop:active {
      border-color: var(--stop-color);
      box-shadow: inset 0 2px 10px rgba(0,0,0,0.6), 0 0 20px rgba(255, 51, 102, 0.4);
    }

    .icon {
      font-size: 22px;
      margin-bottom: 5px;
}

.empty2 { width: 80px; height: 80px; }

    /* D-PAD */
    .d-pad {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 12px;
      margin: 10px 0;
    }
    .d-pad .btn {
      height: 70px;
      font-size: 20px;
    }
    .empty { visibility: hidden; }

    /* Action Grid for Siren/Lift */
    .action-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      margin-bottom: 20px;
    }

    .lift-controls {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      gap: 8px;
    }
    
    .lift-controls .btn { padding: 12px 0; font-size: 12px; }

    /* Slider */
    .slider-container {
      margin-top: 10px;
    }
    
    .slider-header {
      display: flex;
      justify-content: space-between;
      font-size: 13px;
      margin-bottom: 10px;
      color: var(--text-muted);
    }

    .slider {
      -webkit-appearance: none;
      width: 100%;
      height: 8px;
      background: rgba(0,0,0,0.5);
      outline: none;
      border-radius: 4px;
      border: 1px solid var(--panel-border);
    }

    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 24px;
      height: 24px;
      border-radius: 50%;
      background: var(--accent-color);
      cursor: pointer;
      box-shadow: 0 0 15px rgba(0, 255, 204, 0.6);
      transition: transform 0.1s;
    }

    .slider::-webkit-slider-thumb:active {
      transform: scale(1.2);
    }

    /* Toast */
    #toast {
      position: fixed;
      bottom: -60px;
      left: 50%;
      transform: translateX(-50%);
      background: var(--accent-color);
      color: #000;
      padding: 12px 24px;
      border-radius: 30px;
      font-family: var(--font-display);
      font-weight: 700;
      font-size: 14px;
      transition: bottom 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275);
      z-index: 1000;
      box-shadow: 0 10px 20px rgba(0,0,0,0.3);
      letter-spacing: 1px;
      text-transform: uppercase;
    }
    #toast.show { bottom: 30px; }

    @keyframes fadeIn { from { opacity: 0; transform: translateY(10px); } to { opacity: 1; transform: translateY(0); } }
    @keyframes fadeInDown { from { opacity: 0; transform: translateY(-20px); } to { opacity: 1; transform: translateY(0); } }
  </style>
</head>
<body>

  <div class="header">
    <h1>Minesweeper</h1>
    <div class="subtitle">Tactical Remote Command</div>
  </div>

  <div class="dashboard">
    <!-- Motion Panel -->
    <div class="panel">
      <div class="panel-title">Navigation</div>
      <div class="d-pad">
        <div class="empty"></div>
        <button class="btn" onclick="sendCmd('forward')">▲</button>
        <div class="empty"></div>
        <button class="btn" onclick="sendCmd('left')">◀</button>
        <button class="btn btn-stop" onclick="sendCmd('stop')">STOP</button>
        <button class="btn" onclick="sendCmd('right')">▶</button>
        <div class="empty"></div>
        <button class="btn" onclick="sendCmd('backward')">▼</button>
        <div class="empty"></div>
      </div>
    </div>

    <!-- Telemetry Panel -->
    <div class="panel">
      <div class="panel-title">Proximity Sensors</div>
      <div class="prox-container">
        <div class="prox-row"><div class="prox-label">S1</div><div class="prox-bar-bg"><div class="prox-bar-fill" id="pb0"></div></div><div class="prox-value" id="pv0">0</div></div>
        <div class="prox-row"><div class="prox-label">S2</div><div class="prox-bar-bg"><div class="prox-bar-fill" id="pb1"></div></div><div class="prox-value" id="pv1">0</div></div>
        <div class="prox-row"><div class="prox-label">S3</div><div class="prox-bar-bg"><div class="prox-bar-fill" id="pb2"></div></div><div class="prox-value" id="pv2">0</div></div>
        <div class="prox-row"><div class="prox-label">S4</div><div class="prox-bar-bg"><div class="prox-bar-fill" id="pb3"></div></div><div class="prox-value" id="pv3">0</div></div>
        <div class="prox-row"><div class="prox-label">S5</div><div class="prox-bar-bg"><div class="prox-bar-fill" id="pb4"></div></div><div class="prox-value" id="pv4">0</div></div>
      </div>
    </div>

    <!-- Auxiliary Panel -->
    <div class="panel">
      <div class="panel-title">Auxiliary Systems</div>
      
      <div class="action-grid">
        <button class="btn" onclick="sendCmd('siren/on', 'Siren ON')" style="border-color: rgba(255, 51, 102, 0.5)">Siren ON</button>
        <button class="btn" onclick="sendCmd('siren/off', 'Siren OFF')">Siren OFF</button>
      </div>

      <div class="slider-header">
        <span>Lift Mechanism</span>
      </div>
      <div class="lift-controls">
        <button class="btn" onclick="sendCmd('lift/up', 'Lift UP')">UP</button>
        <button class="btn btn-stop" onclick="sendCmd('lift/stop', 'Lift STOP')">STOP</button>
        <button class="btn" onclick="sendCmd('lift/down', 'Lift DOWN')">DOWN</button>
      </div>

      <div class="slider-container">
        <div class="slider-header">
          <span>Camera Pan</span>
          <span id="servo-val" style="color: var(--accent-color)">90°</span>
        </div>
        <input type="range" min="0" max="180" value="90" class="slider" id="servoSlider" onchange="setServo(this.value)" oninput="updateServoLabel(this.value)">
      </div>
    </div>
  </div>

  <div id="toast">Command Sent</div>

  <script>
    function showToast(msg, isStop = false) {
      const toast = document.getElementById('toast');
      toast.innerText = msg;
      if (isStop || msg.includes('STOP') || msg.includes('ON')) {
        toast.style.background = 'var(--stop-color)';
        toast.style.color = '#fff';
        toast.style.boxShadow = '0 10px 20px rgba(255, 51, 102, 0.4)';
      } else {
        toast.style.background = 'var(--accent-color)';
        toast.style.color = '#000';
        toast.style.boxShadow = '0 10px 20px rgba(0, 255, 204, 0.3)';
      }
      toast.classList.add('show');
      setTimeout(() => { toast.classList.remove('show'); }, 1200);
    }

    function sendCmd(endpoint, msgOverride) {
      fetch('/' + endpoint).then(r => {
        if(r.ok) showToast(msgOverride || (endpoint.toUpperCase() + ' Executed'), endpoint === 'stop');
      }).catch(e => console.error(e));
    }

    function updateServoLabel(val) {
      document.getElementById('servo-val').innerText = val + '°';
    }

    function setServo(val) {
      fetch('/servo?angle=' + val).then(r => {
        if(r.ok) showToast('Camera ' + val + '°');
      });
    }

    // Telemetry Polling
    setInterval(() => {
      fetch('/status')
        .then(r => r.json())
        .then(data => {
          if (data.prox && data.prox.length === 5) {
            for (let i=0; i<5; i++) {
              let val = data.prox[i];
              let pct = (val / 1023) * 100;
              document.getElementById('pv'+i).innerText = val;
              document.getElementById('pb'+i).style.width = pct + '%';
            }
          }
        })
        .catch(e => console.log('Poll err:', e));
    }, 500);
  </script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", INDEX_HTML); }

void sendSerialCmd(const char *r_dir, float r_vel, const char *l_dir,
                   float l_vel) {
  Serial.printf("r%s%.2f,l%s%.2f\n", r_dir, r_vel, l_dir, l_vel);
}
void sendExtendedCmd(const char *cmd) { Serial.printf("%s\n", cmd); }

void handleForward() {
  sendSerialCmd("p", 15.0, "p", 15.0);
  server.send(200, "text/plain", "OK");
}

void handleBackward() {
  sendSerialCmd("n", 15.0, "n", 15.0);
  server.send(200, "text/plain", "OK");
}

void handleLeft() {
  sendSerialCmd("p", 10.0, "n", 10.0);
  server.send(200, "text/plain", "OK");
}

void handleRight() {
  sendSerialCmd("n", 10.0, "p", 10.0);
  server.send(200, "text/plain", "OK");
}

void handleStop() {
  sendSerialCmd("p", 0.0, "p", 0.0);
  server.send(200, "text/plain", "OK");
}

void handleSirenOn() {
  sendExtendedCmd("CBUZZ:ALERT");
  server.send(200, "text/plain", "OK");
}
void handleSirenOff() {
  sendExtendedCmd("CBUZZ:SILENT");
  server.send(200, "text/plain", "OK");
}
void handleLiftUp() {
  sendExtendedCmd("CLIFT:UP");
  server.send(200, "text/plain", "OK");
}
void handleLiftDown() {
  sendExtendedCmd("CLIFT:DN");
  server.send(200, "text/plain", "OK");
}
void handleLiftStop() {
  sendExtendedCmd("CLIFT:STOP");
  server.send(200, "text/plain", "OK");
}

void handleServo() {
  if (server.hasArg("angle")) {
    String angle = server.arg("angle");
    String cmd = "CSERVO:" + angle;
    sendExtendedCmd(cmd.c_str());
  }
  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  String json = "{\"prox\": " + proximityData + "}";
  server.send(200, "application/json", json);
}

void setup() {
  // ESP8266 TX is what talks to Arduino Mega RX2.
  // We use 115200 to match MotorController.ino
  Serial.begin(115200);

  // Wait for Mega to boot to avoid garbling its serial receive buffer
  delay(2000);
  Serial.println("\n\n--- ESP8266 Booting ---");

  // Explicitly set WiFi mode to Access Point
  WiFi.mode(WIFI_AP);

  // Set ESP8266 as an Access Point on Channel 6
  bool apStatus = WiFi.softAP(ssid, password, 6);

  if (apStatus) {
    Serial.println("Access Point Created Successfully (Channel 6)!");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Failed to create Access Point!");
  }

  // Setup Web Server Routes
  server.on("/", handleRoot);
  server.on("/forward", handleForward);
  server.on("/backward", handleBackward);
  server.on("/left", handleLeft);
  server.on("/right", handleRight);
  server.on("/stop", handleStop);

  server.on("/siren/on", handleSirenOn);
  server.on("/siren/off", handleSirenOff);
  server.on("/lift/up", handleLiftUp);
  server.on("/lift/down", handleLiftDown);
  server.on("/lift/stop", handleLiftStop);
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
      if (serialBuffer.startsWith("P:")) {
        String values = serialBuffer.substring(2);
        values.replace(",", ", ");
        proximityData = "[" + values + "]";
      }
      serialBuffer = "";
    } else {
      serialBuffer += c;
      if (serialBuffer.length() > 100)
        serialBuffer = "";
    }
  }
}

void loop() {
  server.handleClient();
  processSerialData();
}
