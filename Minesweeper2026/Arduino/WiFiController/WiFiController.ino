#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Set these to your desired credentials.
const char *ssid = "Minesweeper_Bot";
const char *password = "12345678"; // Password must be at least 8 characters

ESP8266WebServer server(80);

// HTML for the remote control web page
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Minesweeper Command Center</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Outfit:wght@300;500;700&display=swap');
    
    :root {
      --bg-color: #0b0f19;
      --panel-bg: rgba(20, 25, 40, 0.6);
      --accent-color: #00ffcc;
      --stop-color: #ff3366;
      --text-main: #ffffff;
      --text-muted: #8b9bb4;
    }

    body {
      margin: 0;
      padding: 0;
      font-family: 'Outfit', sans-serif;
      background: radial-gradient(circle at 50% 0%, #1a233a 0%, var(--bg-color) 100%);
      color: var(--text-main);
      display: flex;
      flex-direction: column;
      align-items: center;
      min-height: 100vh;
      overflow-x: hidden;
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
    }

    h1 {
      margin: 0;
      font-weight: 700;
      font-size: 26px;
      letter-spacing: 2px;
      text-transform: uppercase;
      background: linear-gradient(90deg, var(--accent-color), #00b3ff);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }

    .subtitle {
      font-size: 13px;
      color: var(--text-muted);
      margin-top: 5px;
      font-weight: 300;
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
    }

    .btn {
      width: 80px;
      height: 80px;
      border-radius: 20px;
      border: 1px solid rgba(255, 255, 255, 0.08);
      background: linear-gradient(145deg, rgba(30, 40, 60, 0.8), rgba(15, 20, 30, 0.8));
      color: white;
      font-family: 'Outfit', sans-serif;
      font-size: 14px;
      font-weight: 500;
      cursor: pointer;
      box-shadow: 5px 5px 15px rgba(0,0,0,0.4), 
                  -5px -5px 15px rgba(255,255,255,0.02);
      transition: all 0.15s ease;
      display: flex;
      flex-direction: column;
      justify-content: center;
      align-items: center;
      outline: none;
      -webkit-tap-highlight-color: transparent;
    }

    .btn:active {
      transform: scale(0.92);
      box-shadow: inset 5px 5px 10px rgba(0,0,0,0.5), 
                  inset -5px -5px 10px rgba(255,255,255,0.02);
      border-color: var(--accent-color);
      color: var(--accent-color);
    }

    .btn-stop {
      background: linear-gradient(145deg, rgba(220, 30, 60, 0.8), rgba(180, 20, 40, 0.8));
      border: 1px solid rgba(255, 100, 100, 0.3);
      font-weight: 700;
      letter-spacing: 1px;
      box-shadow: 0 0 20px rgba(255, 51, 102, 0.2);
    }

    .btn-stop:active {
      border-color: var(--stop-color);
      color: #fff;
      box-shadow: inset 5px 5px 10px rgba(0,0,0,0.5), 
                  0 0 30px rgba(255, 51, 102, 0.6);
    }

    .icon {
      font-size: 22px;
      margin-bottom: 5px;
    }

    .empty { width: 80px; height: 80px; }
    
    #toast {
      position: fixed;
      bottom: -60px;
      left: 50%;
      transform: translateX(-50%);
      background: var(--accent-color);
      color: #000;
      padding: 12px 24px;
      border-radius: 30px;
      font-weight: 600;
      font-size: 14px;
      transition: bottom 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275);
      z-index: 1000;
      box-shadow: 0 10px 20px rgba(0,0,0,0.3);
      letter-spacing: 1px;
      text-transform: uppercase;
    }
    
    #toast.show {
      bottom: 30px;
    }
  </style>
</head>
<body>

  <div class="header">
    <h1>Minesweeper</h1>
    <div class="subtitle">TACTICAL REMOTE CONTROL SYSTEM</div>
  </div>

  <div class="container">
    <div class="info-panel">
      <div class="info-item">
        <span class="info-label">Link Status</span>
        <span class="info-value"><span class="status-dot"></span>Active</span>
      </div>
      <div class="info-item">
        <span class="info-label">Control Mode</span>
        <span class="info-value" style="color: #00b3ff;">Manual Override</span>
      </div>
    </div>

    <div class="d-pad">
      <div class="empty"></div>
      <button class="btn" onclick="sendCommand('forward')">
        <span class="icon">▲</span>FWD
      </button>
      <div class="empty"></div>
      
      <button class="btn" onclick="sendCommand('left')">
        <span class="icon">◀</span>LEFT
      </button>
      <button class="btn btn-stop" onclick="sendCommand('stop')">STOP</button>
      <button class="btn" onclick="sendCommand('right')">
        <span class="icon">▶</span>RIGHT
      </button>
      
      <div class="empty"></div>
      <button class="btn" onclick="sendCommand('backward')">
        <span class="icon">▼</span>REV
      </button>
      <div class="empty"></div>
    </div>
  </div>

  <div id="toast">Command Sent</div>

  <script>
    function showToast(cmd) {
      const toast = document.getElementById('toast');
      toast.innerText = cmd + ' Executed';
      if (cmd === 'stop') {
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

    function sendCommand(cmd) {
      fetch('/' + cmd)
        .then(response => {
          if(response.ok) showToast(cmd);
        })
        .catch(error => console.error('Error:', error));
    }
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void sendSerialCmd(const char* right_dir, float right_vel, const char* left_dir, float left_vel) {
  // Format: rp15.00,ln15.00\n
  Serial.printf("r%s%.2f,l%s%.2f\n", right_dir, right_vel, left_dir, left_vel);
}

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
  
  if(apStatus) {
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

  server.begin();
}

void loop() {
  server.handleClient();
}
