/**
 * SampleCode.ino — Simple RPi serial drive + mine siren
 *
 * Minimal Arduino Mega sketch using the same pins and serial framing as
 * Minesweeper2026/Arduino/MotorController.
 *
 * What it does:
 *   - Moves the robot from Raspberry Pi serial commands (USB CDC, 115200)
 *   - Turns the 12V siren ON while metal detector OR any proximity sensor is active
 *
 * RPi command formats (same as MotorController):
 *   rp02.50,lp02.50,     forward  (rad/s, mapped open-loop to PWM)
 *   rn01.00,ln01.00,     reverse
 *   rp02.00,ln02.00,     turn left  (right +, left -)
 *   rn02.00,lp02.00,     turn right
 *   rp00.00,lp00.00,     stop
 *
 * Simple keys (Serial Monitor or RPi):
 *   w forward  s reverse  a left  d right  x stop
 *
 * Arduino -> RPi:
 *   M:0 / M:1            metal detector
 *   P:0,0,1,0,0          five proximity flags (0/1)
 *   S:READY / S:FWD ...  status
 *
 * Board: Arduino Mega 2560
 */

/* ---- Pins (MotorController Config.h) ---- */
const uint8_t MOTOR_R_PWM = 44;
const uint8_t MOTOR_R_DIR = 42;
const uint8_t MOTOR_L_PWM = 46;
const uint8_t MOTOR_L_DIR = 40;
const bool INVERT_LEFT_MOTOR = true;   // matches MotorController motorLeft_

const uint8_t MOTOR_R_PWM_2 = 9;
const uint8_t MOTOR_R_DIR_2 = 8;
const uint8_t MOTOR_L_PWM_2 = 11;
const uint8_t MOTOR_L_DIR_2 = 10;
const bool ENABLE_4WD = true;

const uint8_t METAL_PIN = 33;
const uint8_t PROX_PINS[] = {23, 25, 27, 29, 31};
const uint8_t NUM_PROX = 5;
const uint8_t SIREN_PIN = 37;

const uint8_t METAL_ACTIVE = HIGH;
const uint8_t PROX_ACTIVE = HIGH;
const uint8_t SIREN_ON = HIGH;   // SIREN_ACTIVE_HIGH

/* ---- Timing / mapping ---- */
const unsigned long BAUD = 115200;
const unsigned long SENSOR_MS = 50;
const unsigned long TELEMETRY_MS = 200;
const unsigned long CMD_TIMEOUT_MS = 1500;
const unsigned long METAL_DEBOUNCE_MS = 50;
const float MAX_WHEEL_RAD_S = 10.0f;   // rp10.00 -> PWM 255
const int KEY_PWM = 120;               // WASD open-loop speed

/* ---- Drive state ---- */
float cmd_right = 0.0f;
float cmd_left = 0.0f;
unsigned long last_cmd_ms = 0;

/* ---- Parser (rpXX.XX,lpXX.XX,) ---- */
char pending_motor = 0;
char pending_sign = 'p';
char value_buf[8];
uint8_t value_len = 0;
bool got_right = false;
bool got_left = false;
float pend_right = 0.0f;
float pend_left = 0.0f;

/* ---- Sensors ---- */
bool metal_raw = false;
bool metal_detected = false;
unsigned long metal_change_ms = 0;
bool prox_on[5] = {false, false, false, false, false};
bool mine_detected = false;
bool last_mine_detected = false;
bool last_metal = false;
bool last_prox[5] = {false, false, false, false, false};

unsigned long last_sensor_ms = 0;
unsigned long last_telem_ms = 0;

static void setMotor(uint8_t pwm_pin, uint8_t dir_pin, float pwm, bool invert) {
  pwm = constrain(pwm, -255.0f, 255.0f);
  bool forward = (pwm >= 0.0f);
  if (invert) {
    forward = !forward;
  }
  digitalWrite(dir_pin, forward ? HIGH : LOW);
  analogWrite(pwm_pin, (uint8_t)fabs(pwm));
}

static void applyDrive(float right_pwm, float left_pwm) {
  setMotor(MOTOR_R_PWM, MOTOR_R_DIR, right_pwm, false);
  setMotor(MOTOR_L_PWM, MOTOR_L_DIR, left_pwm, INVERT_LEFT_MOTOR);
  if (ENABLE_4WD) {
    setMotor(MOTOR_R_PWM_2, MOTOR_R_DIR_2, right_pwm, false);
    setMotor(MOTOR_L_PWM_2, MOTOR_L_DIR_2, left_pwm, INVERT_LEFT_MOTOR);
  }
}

static void stopDrive() {
  cmd_right = 0.0f;
  cmd_left = 0.0f;
  applyDrive(0, 0);
}

static float radsToPwm(float rad_s) {
  float pwm = (rad_s / MAX_WHEEL_RAD_S) * 255.0f;
  return constrain(pwm, -255.0f, 255.0f);
}

static void sirenWrite(bool on) {
  digitalWrite(SIREN_PIN, on ? SIREN_ON : !SIREN_ON);
}

static void applyVelCommand() {
  cmd_right = radsToPwm(pend_right);
  cmd_left = radsToPwm(pend_left);
  applyDrive(cmd_right, cmd_left);
  last_cmd_ms = millis();
  got_right = false;
  got_left = false;
}

static void finishValue() {
  value_buf[value_len] = '\0';
  float mag = atof(value_buf);
  if (pending_sign == 'n') {
    mag = -mag;
  }
  if (pending_motor == 'r') {
    pend_right = mag;
    got_right = true;
  } else if (pending_motor == 'l') {
    pend_left = mag;
    got_left = true;
  }
  pending_motor = 0;
  value_len = 0;
}

static void resetParser() {
  pending_motor = 0;
  value_len = 0;
  got_right = false;
  got_left = false;
}

static void handleKey(char c) {
  last_cmd_ms = millis();
  switch (c) {
    case 'w':
    case 'W':
      applyDrive(KEY_PWM, KEY_PWM);
      Serial.println(F("S:FWD"));
      break;
    case 's':
    case 'S':
      applyDrive(-KEY_PWM, -KEY_PWM);
      Serial.println(F("S:REV"));
      break;
    case 'a':
    case 'A':
      applyDrive(KEY_PWM, -KEY_PWM);
      Serial.println(F("S:LEFT"));
      break;
    case 'd':
    case 'D':
      applyDrive(-KEY_PWM, KEY_PWM);
      Serial.println(F("S:RIGHT"));
      break;
    case 'x':
    case 'X':
    case ' ':
      stopDrive();
      Serial.println(F("S:STOP"));
      break;
    default:
      break;
  }
}

static void processSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (pending_motor == 0) {
      if (c == 'r' || c == 'l') {
        pending_motor = c;
        pending_sign = 'p';
        value_len = 0;
        continue;
      }
      if (c == '\n' || c == '\r') {
        if (got_right && got_left) {
          applyVelCommand();
        }
        continue;
      }
      if (c == 'w' || c == 'W' || c == 'a' || c == 'A' || c == 's' || c == 'S' ||
          c == 'd' || c == 'D' || c == 'x' || c == 'X' || c == ' ') {
        handleKey(c);
      }
      continue;
    }

    /* After r/l: expect p or n, then digits, then comma */
    if (pending_sign == 'p' && value_len == 0 && (c == 'p' || c == 'n')) {
      pending_sign = c;
      continue;
    }

    if ((c >= '0' && c <= '9') || c == '.') {
      if (value_len < sizeof(value_buf) - 1) {
        value_buf[value_len++] = c;
      }
      continue;
    }

    if (c == ',') {
      finishValue();
      if (got_right && got_left) {
        applyVelCommand();
      }
      continue;
    }

    if (c == '\n' || c == '\r') {
      if (value_len > 0) {
        finishValue();
      }
      if (got_right && got_left) {
        applyVelCommand();
      }
      resetParser();
      continue;
    }

    resetParser();
  }
}

static void updateSensors() {
  bool metal_now = (digitalRead(METAL_PIN) == METAL_ACTIVE);
  unsigned long now = millis();
  if (metal_now != metal_raw) {
    metal_raw = metal_now;
    metal_change_ms = now;
  }
  if (now - metal_change_ms >= METAL_DEBOUNCE_MS) {
    metal_detected = metal_raw;
  }

  bool any_prox = false;
  for (uint8_t i = 0; i < NUM_PROX; ++i) {
    prox_on[i] = (digitalRead(PROX_PINS[i]) == PROX_ACTIVE);
    if (prox_on[i]) {
      any_prox = true;
    }
  }

  mine_detected = metal_detected || any_prox;
  sirenWrite(mine_detected);

  if (metal_detected != last_metal) {
    Serial.print(F("M:"));
    Serial.println(metal_detected ? 1 : 0);
    last_metal = metal_detected;
  }

  if (mine_detected && !last_mine_detected) {
    Serial.println(F("S:MINE"));
  } else if (!mine_detected && last_mine_detected) {
    Serial.println(F("S:CLEAR"));
  }
  last_mine_detected = mine_detected;
}

static void sendTelemetry() {
  Serial.print(F("P:"));
  for (uint8_t i = 0; i < NUM_PROX; ++i) {
    Serial.print(prox_on[i] ? 1 : 0);
    if (i + 1 < NUM_PROX) {
      Serial.print(',');
    }
  }
  Serial.println();
  Serial.print(F("M:"));
  Serial.println(metal_detected ? 1 : 0);
}

void setup() {
  pinMode(MOTOR_R_PWM, OUTPUT);
  pinMode(MOTOR_R_DIR, OUTPUT);
  pinMode(MOTOR_L_PWM, OUTPUT);
  pinMode(MOTOR_L_DIR, OUTPUT);
  pinMode(MOTOR_R_PWM_2, OUTPUT);
  pinMode(MOTOR_R_DIR_2, OUTPUT);
  pinMode(MOTOR_L_PWM_2, OUTPUT);
  pinMode(MOTOR_L_DIR_2, OUTPUT);

  pinMode(METAL_PIN, INPUT);
  for (uint8_t i = 0; i < NUM_PROX; ++i) {
    pinMode(PROX_PINS[i], INPUT);
  }

  pinMode(SIREN_PIN, OUTPUT);
  sirenWrite(false);
  stopDrive();

  Serial.begin(BAUD);
  metal_change_ms = millis();
  last_cmd_ms = millis();
  Serial.println(F("S:READY"));
  Serial.println(F("S:SampleCode  rp02.50,lp02.50,  or  w a s d x"));
}

void loop() {
  processSerial();

  unsigned long now = millis();
  if (now - last_cmd_ms >= CMD_TIMEOUT_MS) {
    if (cmd_right != 0.0f || cmd_left != 0.0f) {
      stopDrive();
      Serial.println(F("S:TIMEOUT"));
    }
  }

  if (now - last_sensor_ms >= SENSOR_MS) {
    last_sensor_ms = now;
    updateSensors();
  }

  if (now - last_telem_ms >= TELEMETRY_MS) {
    last_telem_ms = now;
    sendTelemetry();
  }
}
