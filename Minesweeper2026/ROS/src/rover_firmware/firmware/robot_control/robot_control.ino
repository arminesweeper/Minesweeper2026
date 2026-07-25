#include <PID_v1.h>

// MDD10A Driver Connection PINs
#define MDD10A_PWM1 9   // PWM Motor A (Right)
#define MDD10A_DIR1 12  // Dir Motor A (Right) - (Replaces old L298N_in1)
#define MDD10A_PWM2 11  // PWM Motor B (Left)
#define MDD10A_DIR2 7   // Dir Motor B (Left)  - (Replaces old L298N_in3)

// Wheel Encoders Connection PINs
#define right_encoder_phaseA 3  // Interrupt
#define right_encoder_phaseB 5
#define left_encoder_phaseA 2   // Interrupt
#define left_encoder_phaseB 4

// Gripper Motor + Magnet + Limit Switches
// NOTE: pins chosen to avoid conflicts with the wheel pins above.
// Adjust to match your actual wiring if different.
#define GRIPPER_PWM_PIN 10
#define GRIPPER_DIR_PIN 8
#define GRIPPER_MAGNET_PIN 6
#define GRIPPER_OPEN_LIMIT_PIN A0
#define GRIPPER_CLOSE_LIMIT_PIN A1

enum GripperState { GRIPPER_IDLE, GRIPPER_OPENING, GRIPPER_CLOSING };
GripperState gripper_state = GRIPPER_IDLE;
unsigned long gripper_motion_start_ms = 0;
// Max time allowed for a full open/close travel before we assume the limit
// switch failed and cut power to protect the motor. Measure your gripper's
// actual travel time and set this a bit above it.
const unsigned long GRIPPER_MAX_TRAVEL_MS = 3000;

bool is_gripper_cmd = false;
bool is_gripper_forward = true;

// Encoders
unsigned int right_encoder_counter = 0;
unsigned int left_encoder_counter = 0;
String right_wheel_sign = "p";  // 'p' = positive, 'n' = negative
String left_wheel_sign = "p";  // 'p' = positive, 'n' = negative
unsigned long last_millis = 0;
const unsigned long interval = 100;

// Interpret Serial Messages
bool is_right_wheel_cmd = false;
bool is_left_wheel_cmd = false;
bool is_right_wheel_forward = true;
bool is_left_wheel_forward = true;
char value[] = "00.00";
uint8_t value_idx = 0;
bool is_cmd_complete = false;

// PID
// Setpoint - Desired
double right_wheel_cmd_vel = 0.0;     // rad/s
double left_wheel_cmd_vel = 0.0;      // rad/s
// Input - Measurement
double right_wheel_meas_vel = 0.0;    // rad/s
double left_wheel_meas_vel = 0.0;     // rad/s
// Output - Command
double right_wheel_cmd = 0.0;         // 0-255
double left_wheel_cmd = 0.0;          // 0-255
// Tuning
double Kp_r = 11.5;
double Ki_r = 7.5;
double Kd_r = 0.1;
double Kp_l = 12.8;
double Ki_l = 8.3;
double Kd_l = 0.1;
// Controller
PID rightMotor(&right_wheel_meas_vel, &right_wheel_cmd, &right_wheel_cmd_vel, Kp_r, Ki_r, Kd_r, DIRECT);
PID leftMotor(&left_wheel_meas_vel, &left_wheel_cmd, &left_wheel_cmd_vel, Kp_l, Ki_l, Kd_l, DIRECT);

void setup() {
  // Init MDD10A Driver Connection PINs
  pinMode(MDD10A_PWM1, OUTPUT);
  pinMode(MDD10A_DIR1, OUTPUT);
  pinMode(MDD10A_PWM2, OUTPUT);
  pinMode(MDD10A_DIR2, OUTPUT);

  // Set Initial Motor Rotation Direction (Default Forward)
  digitalWrite(MDD10A_DIR1, HIGH);
  digitalWrite(MDD10A_DIR2, HIGH);

  rightMotor.SetMode(AUTOMATIC);
  leftMotor.SetMode(AUTOMATIC);
  Serial.begin(115200);

  // Init encoders
  pinMode(right_encoder_phaseB, INPUT);
  pinMode(left_encoder_phaseB, INPUT);
  // Set Callback for Wheel Encoders Pulse
  attachInterrupt(digitalPinToInterrupt(right_encoder_phaseA), rightEncoderCallback, RISING);
  attachInterrupt(digitalPinToInterrupt(left_encoder_phaseA), leftEncoderCallback, RISING);

  // Init gripper pins
  pinMode(GRIPPER_PWM_PIN, OUTPUT);
  pinMode(GRIPPER_DIR_PIN, OUTPUT);
  pinMode(GRIPPER_MAGNET_PIN, OUTPUT);
  pinMode(GRIPPER_OPEN_LIMIT_PIN, INPUT_PULLUP);
  pinMode(GRIPPER_CLOSE_LIMIT_PIN, INPUT_PULLUP);
  digitalWrite(GRIPPER_MAGNET_PIN, LOW);  // magnet off at boot
}

void loop() {
  // Read and Interpret Wheel + Gripper Commands
  if (Serial.available())
  {
    char chr = Serial.read();
    // Right Wheel Motor
    if(chr == 'r')
    {
      is_right_wheel_cmd = true;
      is_left_wheel_cmd = false;
      is_gripper_cmd = false;
      value_idx = 0;
      is_cmd_complete = false;
    }
    // Left Wheel Motor
    else if(chr == 'l')
    {
      is_right_wheel_cmd = false;
      is_left_wheel_cmd = true;
      is_gripper_cmd = false;
      value_idx = 0;
    }
    // Gripper Motor
    else if(chr == 'g')
    {
      is_right_wheel_cmd = false;
      is_left_wheel_cmd = false;
      is_gripper_cmd = true;
      value_idx = 0;
    }
    // Positive direction
    else if(chr == 'p')
    {
      if(is_right_wheel_cmd && !is_right_wheel_forward)
      {
        digitalWrite(MDD10A_DIR1, HIGH);
        is_right_wheel_forward = true;
      }
      else if(is_left_wheel_cmd && !is_left_wheel_forward)
      {
        digitalWrite(MDD10A_DIR2, HIGH);
        is_left_wheel_forward = true;
      }
      else if(is_gripper_cmd)
      {
        is_gripper_forward = true;
      }
    }
    // Negative direction
    else if(chr == 'n')
    {
      if(is_right_wheel_cmd && is_right_wheel_forward)
      {
        digitalWrite(MDD10A_DIR1, LOW);
        is_right_wheel_forward = false;
      }
      else if(is_left_wheel_cmd && is_left_wheel_forward)
      {
        digitalWrite(MDD10A_DIR2, LOW);
        is_left_wheel_forward = false;
      }
      else if(is_gripper_cmd)
      {
        is_gripper_forward = false;
      }
    }
    // Separator
    else if(chr == ',')
    {
      if(is_right_wheel_cmd)
      {
        right_wheel_cmd_vel = atof(value);
      }
      else if(is_left_wheel_cmd)
      {
        left_wheel_cmd_vel = atof(value);
        is_cmd_complete = true;
      }
      else if(is_gripper_cmd)
      {
        double gripper_cmd_value = atof(value);
        handleGripperCommand(gripper_cmd_value, is_gripper_forward);
      }
      // Reset for next command
      value_idx = 0;
      value[0] = '0';
      value[1] = '0';
      value[2] = '.';
      value[3] = '0';
      value[4] = '0';
      value[5] = '\0';
    }
    // Command Value
    else
    {
      if(value_idx < 5)
      {
        value[value_idx] = chr;
        value_idx++;
      }
    }
  }

  // Gripper limit switch monitoring - runs every loop, independent of serial
  bool open_limit_triggered = (digitalRead(GRIPPER_OPEN_LIMIT_PIN) == LOW);
  bool close_limit_triggered = (digitalRead(GRIPPER_CLOSE_LIMIT_PIN) == LOW);

  if (gripper_state == GRIPPER_OPENING && open_limit_triggered)
  {
    stopGripperMotor();
    digitalWrite(GRIPPER_MAGNET_PIN, HIGH);   // grab the mine
    gripper_state = GRIPPER_IDLE;
  }
  else if (gripper_state == GRIPPER_CLOSING && close_limit_triggered)
  {
    stopGripperMotor();
    digitalWrite(GRIPPER_MAGNET_PIN, LOW);    // drop the mine in the box
    gripper_state = GRIPPER_IDLE;
  }
  else if (gripper_state != GRIPPER_IDLE &&
           (millis() - gripper_motion_start_ms > GRIPPER_MAX_TRAVEL_MS))
  {
    // Safety: limit switch never triggered in time, cut power
    stopGripperMotor();
    gripper_state = GRIPPER_IDLE;
  }

  // Encoder
  unsigned long current_millis = millis();
  if(current_millis - last_millis >= interval)
  {
    right_wheel_meas_vel = (10 * right_encoder_counter * (60.0/385.0)) * 0.10472;
    left_wheel_meas_vel = (10 * left_encoder_counter * (60.0/385.0)) * 0.10472;

    rightMotor.Compute();
    leftMotor.Compute();

    // Ignore commands smaller than inertia
    if(right_wheel_cmd_vel == 0.0)
    {
      right_wheel_cmd = 0.0;
    }
    if(left_wheel_cmd_vel == 0.0)
    {
      left_wheel_cmd = 0.0;
    }

    String gripper_status = "gm";  // moving by default
    if (gripper_state == GRIPPER_IDLE)
    {
      if (open_limit_triggered)
      {
        gripper_status = "go";
      }
      else if (close_limit_triggered)
      {
        gripper_status = "gc";
      }
      else
      {
        gripper_status = "gm";  // idle but not at either end (shouldn't normally happen)
      }
    }

    String encoder_read = "r" + right_wheel_sign + String(right_wheel_meas_vel) +
                           ",l" + left_wheel_sign + String(left_wheel_meas_vel) +
                           "," + gripper_status + ",";
    Serial.println(encoder_read);
    last_millis = current_millis;
    right_encoder_counter = 0;
    left_encoder_counter = 0;

    // Send PWM outputs to MDD10A
    analogWrite(MDD10A_PWM1, right_wheel_cmd);
    analogWrite(MDD10A_PWM2, left_wheel_cmd);
  }
}

// New pulse from Right Wheel Encoder
void rightEncoderCallback()
{
  if(digitalRead(right_encoder_phaseB) == HIGH)
  {
    right_wheel_sign = "p";
  }
  else
  {
    right_wheel_sign = "n";
  }
  right_encoder_counter++;
}

// New pulse from Left Wheel Encoder
void leftEncoderCallback()
{
  if(digitalRead(left_encoder_phaseB) == HIGH)
  {
    left_wheel_sign = "n";
  }
  else
  {
    left_wheel_sign = "p";
  }
  left_encoder_counter++;
}

// Gripper: handle an incoming command (+1 open, -1 close, 0 stop)
void handleGripperCommand(double value, bool forward)
{
  if (value == 0.0)
  {
    stopGripperMotor();
    gripper_state = GRIPPER_IDLE;
  }
  else if (forward && gripper_state != GRIPPER_OPENING)
  {
    digitalWrite(GRIPPER_DIR_PIN, HIGH);
    analogWrite(GRIPPER_PWM_PIN, 255);
    gripper_motion_start_ms = millis();
    gripper_state = GRIPPER_OPENING;
  }
  else if (!forward && gripper_state != GRIPPER_CLOSING)
  {
    digitalWrite(GRIPPER_DIR_PIN, LOW);
    analogWrite(GRIPPER_PWM_PIN, 255);
    gripper_motion_start_ms = millis();
    gripper_state = GRIPPER_CLOSING;
  }
}

void stopGripperMotor()
{
  analogWrite(GRIPPER_PWM_PIN, 0);
}