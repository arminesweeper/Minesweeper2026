# API Reference

**Minesweeper Robot Motor Controller — Complete API Documentation**

All classes, public functions, variables, interrupts, packets, state machines, and configuration parameters documented in Doxygen style.

---

## Table of Contents

1. [Config.h — Configuration](#configh)
2. [MotorDriver — Motor HAL](#motordriver)
3. [Encoder — Quadrature Encoder](#encoder)
4. [PIDController — PID with Anti-Windup](#pidcontroller)
5. [MotionController — Velocity Profiling](#motioncontroller)
6. [SerialProtocol — Communication](#serialprotocol)
7. [Odometry — Pose Estimation](#odometry)
8. [IMU — MPU6050 Driver](#imu)
9. [LiftController — Lift Mechanism](#liftcontroller)
10. [Sensors — Peripherals](#sensors)
11. [Safety — Fault Detection](#safety)
12. [Diagnostics — Statistics](#diagnostics)
13. [Interrupts](#interrupts)
14. [Packet Formats](#packet-formats)

---

## Config.h

Central configuration file.  All tunable parameters and hardware pin assignments.

### Feature Flags

```cpp
/** @brief Enable hardware watchdog timer (2-second timeout) */
#define ENABLE_WATCHDOG 1

/** @brief Enable odometry position tracking */
#define ENABLE_ODOMETRY 1

/** @brief Enable detailed diagnostics output */
#define ENABLE_DIAGNOSTICS 1

/** @brief Enable safety monitoring (timeout, faults) */
#define ENABLE_SAFETY_MONITOR 1

/** @brief Enable MPU6050 IMU integration */
#define ENABLE_IMU 1

/** @brief Enable lift mechanism controller */
#define ENABLE_LIFT 1

/** @brief Enable sensor subsystem (proximity, metal, buzzer, LED) */
#define ENABLE_SENSORS 1
```

### Namespace: Pins

```cpp
namespace Pins {
// Motor Driver — Cytron MDD10A Rev 2.0 (Sign-Magnitude Mode)
constexpr uint8_t MOTOR_R_PWM = 9;       ///< Right motor PWM (Timer 2, 8-bit)
constexpr uint8_t MOTOR_R_DIR = 12;      ///< Right motor direction
constexpr uint8_t MOTOR_L_PWM = 11;      ///< Left motor PWM (Timer 1, 16-bit)
constexpr uint8_t MOTOR_L_DIR = 7;       ///< Left motor direction

// Wheel Encoders
constexpr uint8_t ENCODER_R_A = 3;       ///< Right encoder Phase A (INT5)
constexpr uint8_t ENCODER_R_B = 5;       ///< Right encoder Phase B
constexpr uint8_t ENCODER_L_A = 2;       ///< Left encoder Phase A (INT4)
constexpr uint8_t ENCODER_L_B = 4;       ///< Left encoder Phase B

// IMU (MPU6050 via I2C)
constexpr uint8_t IMU_SDA = 20;          ///< I2C data (Wire library default)
constexpr uint8_t IMU_SCL = 21;          ///< I2C clock (Wire library default)

// Lift Mechanism
constexpr uint8_t LIFT_PWM = 10;         ///< Lift motor PWM output
constexpr uint8_t LIFT_DIR = 8;          ///< Lift motor direction
constexpr uint8_t LIMIT_SW_TOP = 28;     ///< Top limit switch (active LOW)
constexpr uint8_t LIMIT_SW_BOTTOM = 29;  ///< Bottom limit switch (active LOW)

// Electromagnets
constexpr uint8_t MAGNET_1 = 22;         ///< Electromagnet channel 1
constexpr uint8_t MAGNET_2 = 23;         ///< Electromagnet channel 2
constexpr uint8_t MAGNET_3 = 24;         ///< Electromagnet channel 3
constexpr uint8_t MAGNET_4 = 25;         ///< Electromagnet channel 4
constexpr uint8_t MAGNET_5 = 26;         ///< Electromagnet channel 5

// Sensors
constexpr uint8_t METAL_DETECTOR = 27;   ///< Metal detector digital input (active LOW)
constexpr uint8_t PROXIMITY_1 = A1;      ///< Proximity sensor 1 (analog)
constexpr uint8_t PROXIMITY_2 = A2;      ///< Proximity sensor 2 (analog)
constexpr uint8_t PROXIMITY_3 = A3;      ///< Proximity sensor 3 (analog)
constexpr uint8_t PROXIMITY_4 = A4;      ///< Proximity sensor 4 (analog)
constexpr uint8_t PROXIMITY_5 = A5;      ///< Proximity sensor 5 (analog)

// Indicators
constexpr uint8_t BUZZER = 6;            ///< Buzzer output (PWM capable)
constexpr uint8_t WARNING_LED = 13;      ///< Warning LED (also built-in LED)

// Battery
constexpr uint8_t BATTERY_SENSE = A0;    ///< Voltage divider analog input
}
```

### Namespace: EncoderSpec

```cpp
namespace EncoderSpec {
constexpr double PPR = 385.0;               ///< Pulses Per Revolution
constexpr double RADIANS_PER_PULSE = TWO_PI / PPR; ///< Radians per encoder pulse
constexpr double RPM_TO_RADS = TWO_PI / 60.0;      ///< RPM to rad/s conversion
}
```

### Namespace: RobotParams

```cpp
namespace RobotParams {
constexpr double WHEEL_DIAMETER_MM = 65.0;  ///< Wheel diameter in millimeters
constexpr double WHEEL_RADIUS_M = 0.0325;   ///< Wheel radius in meters
constexpr double WHEEL_BASE_MM = 150.0;     ///< Center-to-center wheel distance (mm)
constexpr double WHEEL_BASE_M = 0.150;      ///< Wheel base in meters
constexpr double TICKS_PER_METER = PPR / (PI * WHEEL_DIAMETER_MM / 1000.0);
}
```

### Namespace: Timing

```cpp
namespace Timing {
constexpr unsigned long CONTROL_INTERVAL_MS = 100;    ///< PID loop period (10 Hz)
constexpr double CONTROL_INTERVAL_S = 0.1;            ///< PID loop period in seconds
constexpr unsigned long TELEMETRY_INTERVAL_MS = 100;   ///< Telemetry TX rate (10 Hz)
constexpr unsigned long IMU_INTERVAL_MS = 20;          ///< IMU read rate (50 Hz)
constexpr unsigned long SENSOR_INTERVAL_MS = 50;       ///< Sensor read rate (20 Hz)
constexpr unsigned long LIFT_INTERVAL_MS = 50;         ///< Lift update rate (20 Hz)
}
```

### Namespace: PIDTuning

```cpp
namespace PIDTuning {
constexpr double KP_RIGHT = 11.5;           ///< Right motor proportional gain
constexpr double KI_RIGHT = 7.5;            ///< Right motor integral gain
constexpr double KD_RIGHT = 0.1;            ///< Right motor derivative gain
constexpr double KP_LEFT = 12.8;            ///< Left motor proportional gain
constexpr double KI_LEFT = 8.3;             ///< Left motor integral gain
constexpr double KD_LEFT = 0.1;             ///< Left motor derivative gain
constexpr double OUTPUT_MIN = -255.0;        ///< Minimum PID output (full reverse PWM)
constexpr double OUTPUT_MAX = 255.0;         ///< Maximum PID output (full forward PWM)
constexpr double INTEGRAL_WINDUP_LIMIT = 200.0; ///< Max integral accumulation
}
```

### Namespace: MotionLimits

```cpp
namespace MotionLimits {
constexpr double MAX_ACCELERATION = 15.0;    ///< Max acceleration (rad/s²)
constexpr double MAX_DECELERATION = 20.0;    ///< Max deceleration (rad/s²)
constexpr double MIN_OUTPUT_DEADBAND = 8.0;  ///< Min PWM to overcome stiction
constexpr double VELOCITY_TOLERANCE = 0.01;  ///< Near-zero velocity threshold (rad/s)
}
```

### Namespace: Safety

```cpp
namespace Safety {
constexpr unsigned long COMMAND_TIMEOUT_MS = 1500;   ///< Serial command timeout (ms)
constexpr unsigned long WATCHDOG_TIMEOUT_MS = 2000;  ///< Hardware WDT timeout (ms)
constexpr double MAX_VELOCITY_RAD_S = 10.0;          ///< Max allowed velocity (rad/s)
constexpr uint16_t BATTERY_VOLTAGE_LOW = 10800;      ///< Low battery threshold (mV)
constexpr uint16_t BATTERY_VOLTAGE_HIGH = 16800;     ///< High battery threshold (mV)
constexpr double VOLTAGE_DIVIDER_RATIO = 3.3;        ///< Voltage divider R1+R2/R2
}
```

### Namespace: LiftConfig

```cpp
namespace LiftConfig {
constexpr uint8_t LIFT_PWM_SPEED = 200;        ///< Lift motor PWM (0-255)
constexpr unsigned long STALL_TIMEOUT_MS = 5000; ///< Max time for lift travel (ms)
constexpr unsigned long DEBOUNCE_MS = 50;       ///< Limit switch debounce (ms)
constexpr uint8_t NUM_MAGNETS = 5;              ///< Number of electromagnets
}
```

### Namespace: SensorConfig

```cpp
namespace SensorConfig {
constexpr uint16_t PROXIMITY_THRESHOLD = 500;  ///< Proximity detection ADC threshold
constexpr uint8_t NUM_PROXIMITY = 5;           ///< Number of proximity sensors
constexpr unsigned long METAL_DEBOUNCE_MS = 100; ///< Metal detector debounce (ms)
constexpr uint8_t ADC_SAMPLES = 4;             ///< Number of ADC averages per read
}
```

### Namespace: IMUConfig

```cpp
namespace IMUConfig {
constexpr uint8_t MPU6050_ADDR = 0x68;         ///< I2C address (AD0 = LOW)
constexpr uint16_t CALIBRATION_SAMPLES = 500;  ///< Gyro calibration sample count
constexpr double COMPLEMENTARY_ALPHA = 0.98;   ///< Complementary filter coefficient
constexpr double GYRO_SCALE = 131.0;           ///< LSB/°/s for ±250°/s range
constexpr double ACCEL_SCALE = 16384.0;        ///< LSB/g for ±2g range
}
```

---

## MotorDriver

**File:** `MotorDriver.h` / `MotorDriver.cpp`

### struct MotorPinConfig

```cpp
/** @brief Motor driver channel pin configuration */
struct MotorPinConfig {
    uint8_t pwm_pin;  ///< PWM output pin
    uint8_t dir_pin;  ///< Direction control pin
};
```

### class MotorDriver

```cpp
/**
 * @brief Cytron MDD10A motor driver interface (sign-magnitude mode)
 *
 * Handles bidirectional PWM output with direction pin control.
 * Supports motor inversion for physically reversed mounting.
 */
class MotorDriver {
public:
    /**
     * @brief Construct motor driver
     * @param config Pin configuration (pwm_pin, dir_pin)
     * @param inverted true if motor mounted in reverse direction
     */
    explicit MotorDriver(const MotorPinConfig& config, bool inverted = false);

    /** @brief Initialize pins to OUTPUT, set safe defaults */
    void begin();

    /**
     * @brief Set motor output
     * @param pwm Signed PWM value [-255.0, +255.0]
     * @note Positive = forward, Negative = reverse
     * @note Values are clamped to valid range
     */
    void setOutput(double pwm);

    /** @brief Graceful stop (PWM = 0, direction unchanged) */
    void stop();

    /** @brief Emergency stop (immediate PWM = 0, direct port write) */
    void emergencyStop();

    /** @return Last written signed PWM value */
    double getOutput() const;

    /** @return true if current direction is forward */
    bool isForward() const;

    /** @return true if |PWM| < 0.5 */
    bool isStopped() const;
};
```

---

## Encoder

**File:** `Encoder.h` / `Encoder.cpp`

### struct EncoderPinConfig

```cpp
/** @brief Encoder channel pin configuration */
struct EncoderPinConfig {
    uint8_t phase_a_pin;   ///< Phase A (must be interrupt-capable)
    uint8_t phase_b_pin;   ///< Phase B (general digital I/O)
    uint8_t interrupt_id;  ///< Interrupt number (for documentation)
};
```

### class Encoder

```cpp
/**
 * @brief Quadrature encoder reader with ISR-driven counting
 *
 * Uses Phase A rising-edge interrupts with Phase B direction detection.
 * All shared counters are volatile and accessed via ATOMIC_BLOCK.
 */
class Encoder {
public:
    explicit Encoder(const EncoderPinConfig& config, bool direction_inverted = false);

    /**
     * @brief Initialize encoder pins and attach ISR
     * @param isr Function pointer to the ISR for this encoder
     */
    void begin(void (*isr)(void));

    /**
     * @brief Atomically read and reset pulse counter
     * @return Signed pulse count since last call
     * @note Thread-safe via ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
     */
    int32_t getPulsesAndReset();

    /** @brief Read pulse count without resetting (atomic) */
    int32_t getPulses() const;

    /** @brief Reset pulse counter to zero (atomic) */
    void reset();

    /**
     * @brief Calculate angular velocity from pulse count
     * @param pulses Signed pulse count over sample period
     * @param dt_sec Sample period in seconds
     * @return Velocity in rad/s (signed, direction-corrected)
     */
    double calculateVelocity(int32_t pulses, double dt_sec) const;

    /** @return Last ISR-detected direction (+1 or -1) */
    int8_t getDirection() const;

    /** @return true if encoder error detected */
    bool hasError() const;

    /** @brief Clear error flag */
    void clearError();

    /** @return Total accumulated pulses since last resetTotal() */
    int32_t getTotalPulses() const;

    /** @brief Reset total pulse counter */
    void resetTotal();

    // Static ISR callbacks
    static void isrCallbackRight();
    static void isrCallbackLeft();
    static Encoder* instance_right_;
    static Encoder* instance_left_;
};
```

---

## PIDController

**File:** `PIDController.h` / `PIDController.cpp`

```cpp
/**
 * @brief Custom PID controller with anti-windup and output saturation
 *
 * Features:
 * - Integral clamping to ±INTEGRAL_WINDUP_LIMIT
 * - Back-calculation anti-windup at output saturation
 * - Enable/disable control
 * - Hot-tuning support
 */
class PIDController {
public:
    /**
     * @param kp Proportional gain
     * @param ki Integral gain
     * @param kd Derivative gain
     * @param output_min Minimum output value
     * @param output_max Maximum output value
     * @param sample_time_ms Control loop period in milliseconds
     */
    PIDController(double kp, double ki, double kd,
                  double output_min, double output_max, int sample_time_ms);

    /** @brief Initialize and enable controller */
    void begin();

    /**
     * @brief Compute one PID iteration
     * @param setpoint Desired value (rad/s)
     * @param input Measured value (rad/s)
     * @return Computed output [-255, +255]
     */
    double compute(double setpoint, double input);

    /** @brief Enable or disable controller */
    void setEnabled(bool enabled);
    bool isEnabled() const;

    /** @brief Reset integral term and error history */
    void reset();

    /** @brief Update PID gains at runtime */
    void setTunings(double kp, double ki, double kd);

    /** @return Last computed output value */
    double getOutput() const;

    /** @brief Change output limits */
    void setOutputLimits(double min, double max);

    /** @brief Change sample time */
    void setSampleTime(int ms);
};
```

---

## MotionController

**File:** `MotionController.h` / `MotionController.cpp`

### struct WheelMotionState

```cpp
/** @brief Complete state for one wheel */
struct WheelMotionState {
    double target_velocity;    ///< Commanded velocity from serial (rad/s)
    double profiled_velocity;  ///< Ramp-limited velocity (rad/s)
    double measured_velocity;  ///< Encoder-measured velocity (rad/s)
    double pid_output;         ///< PID computed PWM output
};
```

### class MotionController

```cpp
/**
 * @brief Motion planning with velocity ramping and deadband
 */
class MotionController {
public:
    MotionController();
    void begin();

    /** @brief Set right wheel target (clamped to MAX_VELOCITY_RAD_S) */
    void setRightTarget(double velocity);

    /** @brief Set left wheel target (clamped to MAX_VELOCITY_RAD_S) */
    void setLeftTarget(double velocity);

    /**
     * @brief Update velocity ramp profiles
     * @param dt_sec Time since last call in seconds
     */
    void updateProfiles(double dt_sec);

    /** @brief Apply deadband compensation to PID outputs */
    void applyDeadband();

    /** @brief Immediately zero all velocities and outputs */
    void emergencyStop();

    const WheelMotionState& getRightState() const;
    const WheelMotionState& getLeftState() const;
    WheelMotionState& getRightStateMutable();
    WheelMotionState& getLeftStateMutable();

    /** @return true if both profiled velocities < VELOCITY_TOLERANCE */
    bool isStopped() const;

    /** @brief Reset all state to zero */
    void reset();
};
```

---

## SerialProtocol

**File:** `SerialProtocol.h` / `SerialProtocol.cpp`

### struct ParsedCommand

```cpp
/** @brief Single parsed wheel command */
struct ParsedCommand {
    char wheel;       ///< 'r' or 'l'
    char sign;        ///< 'p' (positive) or 'n' (negative)
    double velocity;  ///< Velocity magnitude in rad/s
    bool valid;       ///< true if parsing succeeded
};
```

### enum class State

```cpp
/** @brief Serial parser state machine states */
enum class State : uint8_t {
    WAITING_PREFIX,     ///< Waiting for 'r', 'l', or 'C'
    READING_DIRECTION,  ///< Expecting 'p' or 'n'
    READING_VALUE,      ///< Accumulating digit characters
    READING_COMMAND,    ///< Reading extended command string
    COMPLETE            ///< Command fully parsed
};
```

### class SerialProtocol

```cpp
/**
 * @brief Serial protocol handler for ROS ↔ Arduino communication
 *
 * Inbound: "rp02.50,ln01.30,"  (velocity commands)
 * Outbound: "rp2.310,ln1.280," (velocity telemetry)
 *
 * Extended commands via 'C' prefix:
 *   "CLIFT:UP\n"   — raise lift
 *   "CLIFT:DN\n"   — lower lift
 *   "CMAG:1:ON\n"  — activate magnet 1
 *   "CMAG:ALL:OFF\n" — deactivate all magnets
 */
class SerialProtocol {
public:
    SerialProtocol();
    void begin();

    /**
     * @brief Process incoming serial data (non-blocking)
     * @param[out] right_vel Parsed right wheel velocity (rad/s, signed)
     * @param[out] left_vel Parsed left wheel velocity (rad/s, signed)
     * @return true if a complete velocity command pair was received
     */
    bool processInput(double& right_vel, double& left_vel);

    /**
     * @brief Check for extended commands
     * @param[out] cmd_buf Buffer to receive command string
     * @param buf_size Size of cmd_buf
     * @return true if an extended command was received
     */
    bool getExtendedCommand(char* cmd_buf, size_t buf_size) const;

    /** @brief Send velocity telemetry: "rp2.310,ln1.280," */
    void sendTelemetry(double right_vel, double left_vel) const;

    /** @brief Send odometry: "O:x,y,θ," */
    void sendOdometry(double x, double y, double theta) const;

    /** @brief Send IMU data: "I:yaw,pitch,roll" */
    void sendIMU(double yaw, double pitch, double roll) const;

    /** @brief Send proximity data: "P:v1,v2,v3,v4,v5" */
    void sendProximity(const uint16_t* values, uint8_t count) const;

    /** @brief Send metal detector: "M:0" or "M:1" */
    void sendMetalDetect(bool detected) const;

    /** @brief Send lift state: "L:STATE_NAME" */
    void sendLiftState(const char* state_str) const;

    /** @brief Send status: "S:message" */
    void sendStatus(const char* message) const;

    /** @brief Send error: "E:code:message" */
    void sendError(uint8_t error_code, const char* message) const;

    /** @return millis() timestamp of last complete command */
    unsigned long getLastCommandTime() const;

    /** @return true if serial data is available */
    bool dataAvailable() const;
};
```

---

## Odometry

**File:** `Odometry.h` / `Odometry.cpp`

### struct Pose

```cpp
/** @brief Robot pose in 2D space */
struct Pose {
    double x;      ///< X position (meters)
    double y;      ///< Y position (meters)
    double theta;  ///< Heading (radians, [-π, π])
};
```

### class Odometry

```cpp
/**
 * @brief Differential drive odometry using unicycle model
 *
 * Uses exact arc integration when ω ≠ 0 and straight-line
 * approximation when ω ≈ 0.
 */
class Odometry {
public:
    Odometry();
    void begin();

    /**
     * @brief Update pose estimate
     * @param right_vel Right wheel angular velocity (rad/s)
     * @param left_vel Left wheel angular velocity (rad/s)
     * @param dt_sec Time step (seconds)
     */
    void update(double right_vel, double left_vel, double dt_sec);

    const Pose& getPose() const;
    double getX() const;
    double getY() const;
    double getTheta() const;
    double getThetaDegrees() const;
    double getDistanceTraveled() const;
    void reset();
    void setPosition(double x, double y, double theta);
};
```

---

## IMU

**File:** `IMU.h` / `IMU.cpp`

### struct IMUData

```cpp
/** @brief Processed IMU measurements */
struct IMUData {
    double yaw;     ///< Yaw angle (degrees, integrated gyro Z)
    double pitch;   ///< Pitch angle (degrees, complementary filter)
    double roll;    ///< Roll angle (degrees, complementary filter)
    double gyro_x;  ///< Angular velocity X (°/s)
    double gyro_y;  ///< Angular velocity Y (°/s)
    double gyro_z;  ///< Angular velocity Z (°/s)
    double accel_x; ///< Linear acceleration X (g)
    double accel_y; ///< Linear acceleration Y (g)
    double accel_z; ///< Linear acceleration Z (g)
    double temperature; ///< Die temperature (°C)
};
```

### class IMU

```cpp
/**
 * @brief MPU6050 IMU driver with complementary filter
 *
 * Direct I2C register access via Wire library.
 * No external library dependency.
 *
 * Startup: calibrates gyro bias over 500 samples (~2.5 seconds).
 * Runtime: burst-reads 14 bytes per update for efficiency.
 */
class IMU {
public:
    IMU();

    /**
     * @brief Initialize MPU6050 and calibrate gyro
     * @return true if device responds on I2C
     */
    bool begin();

    /**
     * @brief Read sensors and update filtered angles
     * @param dt_sec Time since last call (seconds)
     */
    void update(double dt_sec);

    /** @return true if MPU6050 responds to WHO_AM_I query */
    bool isConnected() const;

    const IMUData& getData() const;
    double getYaw() const;
    double getPitch() const;
    double getRoll() const;
    double getGyroZ() const;
    double getTemperature() const;

    /** @brief Reset yaw integrator to zero */
    void resetYaw();

    /** @return true if I2C communication error detected */
    bool hasError() const;
    void clearError();
};
```

---

## LiftController

**File:** `LiftController.h` / `LiftController.cpp`

### enum class LiftState

```cpp
/** @brief Lift mechanism states */
enum class LiftState : uint8_t {
    IDLE,      ///< Lift at rest, ready for commands
    RAISING,   ///< Lift motor moving up
    RAISED,    ///< Lift at top position (top limit switch active)
    LOWERING,  ///< Lift motor moving down
    LOWERED,   ///< Lift at bottom position (bottom limit switch active)
    FAULT      ///< Stall or limit switch error
};
```

### class LiftController

```cpp
/**
 * @brief Lift mechanism with state machine, limit switches, and electromagnets
 *
 * Drives a DC motor via PWM/DIR for vertical lift.
 * Monitors top/bottom limit switches (active LOW with internal pull-ups).
 * Controls 5 electromagnets for mine pickup.
 * Includes stall timeout protection.
 */
class LiftController {
public:
    LiftController();
    void begin();

    /**
     * @brief Update state machine (call at LIFT_INTERVAL_MS)
     * @note Reads limit switches, checks stall timeout, updates motor
     */
    void update();

    /** @brief Command lift to raise */
    void raise();

    /** @brief Command lift to lower */
    void lower();

    /** @brief Emergency stop lift motor */
    void stop();

    /**
     * @brief Set individual electromagnet state
     * @param index Magnet index [0-4]
     * @param on true to energize, false to de-energize
     */
    void setMagnet(uint8_t index, bool on);

    /** @brief Energize all electromagnets */
    void setAllMagnets(bool on);

    /** @return Current lift state */
    LiftState getState() const;

    /** @return Human-readable state string */
    const char* getStateString() const;

    /** @return true if top limit switch is active */
    bool isAtTop() const;

    /** @return true if bottom limit switch is active */
    bool isAtBottom() const;

    /** @return true if lift is in FAULT state */
    bool hasFault() const;

    /** @brief Clear fault and return to IDLE */
    void clearFault();

    /**
     * @brief Get magnet state bitmask
     * @return Bits 0-4 correspond to magnets 1-5 (1 = energized)
     */
    uint8_t getMagnetState() const;
};
```

---

## Sensors

**File:** `Sensors.h` / `Sensors.cpp`

### enum class BuzzerPattern

```cpp
/** @brief Buzzer sound patterns */
enum class BuzzerPattern : uint8_t {
    SILENT,       ///< No sound
    BEEP,         ///< Single short beep (50 ms)
    ALERT,        ///< Slow beeping (200 ms on/off)
    ALARM,        ///< Fast beeping (100 ms on/off)
    MINE_DETECT   ///< Continuous 500 Hz tone
};
```

### enum class LEDPattern

```cpp
/** @brief Warning LED blink patterns */
enum class LEDPattern : uint8_t {
    OFF,          ///< LED off
    ON,           ///< LED continuously on
    SLOW_BLINK,   ///< 500 ms on/off (1 Hz)
    FAST_BLINK,   ///< 100 ms on/off (5 Hz)
    HEARTBEAT     ///< Double-pulse heartbeat pattern
};
```

### class Sensors

```cpp
/**
 * @brief Sensor manager for proximity, metal detection, buzzer, and LED
 *
 * Handles non-blocking reads with configurable intervals.
 * Proximity sensors use multi-sample averaging.
 * Metal detector uses software debounce.
 */
class Sensors {
public:
    Sensors();
    void begin();

    /** @brief Update all sensor readings (call at SENSOR_INTERVAL_MS) */
    void update();

    /**
     * @brief Get proximity sensor value
     * @param index Sensor index [0-4]
     * @return ADC value [0-1023]
     */
    uint16_t getProximity(uint8_t index) const;

    /** @brief Get all 5 proximity values */
    void getAllProximity(uint16_t* values) const;

    /**
     * @brief Check if proximity sensor detects obstacle
     * @param index Sensor index [0-4]
     * @return true if ADC value > PROXIMITY_THRESHOLD
     */
    bool isProximityTriggered(uint8_t index) const;

    /** @return true if metal detector is triggered (debounced) */
    bool isMetalDetected() const;

    /** @brief Set buzzer pattern */
    void setBuzzerPattern(BuzzerPattern pattern);

    /** @brief Set LED pattern */
    void setLEDPattern(LEDPattern pattern);

    /** @brief Single beep (non-blocking) */
    void beep();

    /** @return Current buzzer pattern */
    BuzzerPattern getBuzzerPattern() const;

    /** @return Current LED pattern */
    LEDPattern getLEDPattern() const;
};
```

---

## Safety

**File:** `Safety.h` / `Safety.cpp`

### enum class SystemState

```cpp
/** @brief System operating states */
enum class SystemState : uint8_t {
    BOOTING,        ///< System initializing
    ACTIVE,         ///< Normal operation
    ESTOP_TIMEOUT,  ///< Command timeout triggered
    ESTOP_MANUAL,   ///< Manual emergency stop
    FAULT_ENCODER,  ///< Encoder error detected
    FAULT_BATTERY,  ///< Battery voltage out of range
    FAULT_IMU,      ///< IMU communication failure
    FAULT_LIFT,     ///< Lift mechanism fault
    FAULT_UNKNOWN   ///< Unknown fault condition
};
```

### struct FaultFlags

```cpp
/** @brief Bit-packed fault flags (1 byte total) */
struct FaultFlags {
    bool command_timeout : 1;  ///< Serial command timeout
    bool encoder_right : 1;   ///< Right encoder fault
    bool encoder_left : 1;    ///< Left encoder fault
    bool battery_low : 1;     ///< Battery voltage below threshold
    bool battery_high : 1;    ///< Battery voltage above threshold
    bool velocity_limit : 1;  ///< Velocity exceeds sanity limit
    bool watchdog_reset : 1;  ///< Previous boot was WDT reset
    bool imu_fault : 1;       ///< IMU communication error
};
```

### class SafetyMonitor

```cpp
/**
 * @brief Safety monitoring with watchdog, timeout, and fault detection
 */
class SafetyMonitor {
public:
    SafetyMonitor();
    void begin();

    /**
     * @brief Run all safety checks
     * @param last_command_time millis() of last valid command
     * @param right_vel Current right wheel velocity
     * @param left_vel Current left wheel velocity
     * @return Current system state
     */
    SystemState update(unsigned long last_command_time,
                       double right_vel, double left_vel);

    void triggerEStop();
    void clearEStop();

    SystemState getState() const;
    bool isActive() const;
    FaultFlags getFaults() const;
    bool hasFault() const;
    void setEncoderFault(bool is_right, bool fault);
    void setIMUFault(bool fault);
    void setLiftFault(bool fault);
    uint16_t readBatteryVoltage() const;
    void resetWatchdog();
    const char* getStateString() const;
};
```

---

## Diagnostics

**File:** `Diagnostics.h` / `Diagnostics.cpp`

### struct SystemStats

```cpp
/** @brief Runtime performance statistics */
struct SystemStats {
    uint32_t loop_count;        ///< Total loop() iterations
    uint32_t command_count;     ///< Valid commands received
    uint32_t timeout_count;     ///< Timeout events
    uint32_t control_cycles;    ///< PID computation cycles
    uint32_t max_loop_time_us;  ///< Worst-case loop time (µs)
    uint32_t min_loop_time_us;  ///< Best-case loop time (µs)
    uint32_t avg_loop_time_us;  ///< Average loop time (µs)
    uint32_t last_loop_time_us; ///< Most recent loop time (µs)
};
```

### class Diagnostics

```cpp
/**
 * @brief Runtime diagnostics and performance monitoring
 */
class Diagnostics {
public:
    Diagnostics();
    void begin();
    void loopStart();   ///< Call at top of loop()
    void loopEnd();     ///< Call at bottom of loop()

    void incrementCommandCount();
    void incrementTimeoutCount();
    void incrementControlCycleCount();

    /**
     * @brief Send periodic status report via serial
     * @param state Current SystemState
     * @param faults Current FaultFlags
     */
    void sendStatusReport(SystemState state, FaultFlags faults);

    const SystemStats& getStats() const;
    void resetStats();

    /** @return Estimated free SRAM in bytes */
    static uint16_t getFreeSRAM();

    /** @return Uptime in seconds since boot */
    static uint32_t getUptimeSeconds();
};
```

---

## Interrupts

| ISR Function | Trigger | Pin | Frequency | Max Latency |
|-------------|---------|-----|-----------|-------------|
| `rightEncoderISR()` | Phase A rising | D3 (INT5) | Up to ~10 kHz | < 2 µs |
| `leftEncoderISR()` | Phase A rising | D2 (INT4) | Up to ~10 kHz | < 2 µs |

### ISR Implementation

```cpp
/**
 * @brief Right encoder interrupt service routine
 * @note Called on RISING edge of Phase A
 * @note Reads Phase B to determine direction
 * @note Increments/decrements volatile pulse counters
 * @note Execution time: < 2 µs
 */
void rightEncoderISR();

/**
 * @brief Left encoder interrupt service routine
 * @note Identical behavior to rightEncoderISR but for left encoder
 */
void leftEncoderISR();
```

---

## Packet Formats

### Inbound (ROS → Arduino)

#### Velocity Command

```
Format:  r[p|n]XX.XX,l[p|n]XX.XX,[\n]
Example: rp02.50,ln01.30,
Fields:
  r/l  — wheel selector (right/left)
  p/n  — direction (positive/negative)
  XX.XX — velocity magnitude in rad/s (max 5 chars)
  ,    — field separator
  \n   — optional line terminator
```

#### Extended Command

```
Format:  C<COMMAND>\n
Examples:
  CLIFT:UP\n      — raise lift mechanism
  CLIFT:DN\n      — lower lift mechanism
  CLIFT:STOP\n    — stop lift motor
  CMAG:1:ON\n     — energize magnet 1
  CMAG:1:OFF\n    — de-energize magnet 1
  CMAG:ALL:ON\n   — energize all magnets
  CMAG:ALL:OFF\n  — de-energize all magnets
  CBUZZ:ALERT\n   — set buzzer alert pattern
  CBUZZ:SILENT\n  — silence buzzer
  CRESET\n        — reset all subsystems
  CDIAG\n         — request immediate diagnostics report
```

### Outbound (Arduino → ROS)

#### Velocity Telemetry

```
Format:  r[p|n]X.XXX,l[p|n]X.XXX,\n
Example: rp2.310,ln1.280,
Rate:    10 Hz
```

#### Odometry

```
Format:  O:X.XXXX,Y.YYYY,T.TTTT,\n
Example: O:0.1234,0.5678,1.2345,
Rate:    10 Hz
```

#### Metal Detector

```
Format:  M:[0|1]\n
Example: M:1
Rate:    On change or 20 Hz
```

#### Proximity Sensors

```
Format:  P:V1,V2,V3,V4,V5\n
Example: P:120,340,560,780,900
Rate:    20 Hz
Values:  ADC [0-1023]
```

#### IMU

```
Format:  I:YAW,PITCH,ROLL\n
Example: I:45.2,-2.1,0.3
Rate:    10 Hz (decimated from 50 Hz internal)
Units:   Degrees
```

#### Lift State

```
Format:  L:STATE[,MAG_MASK]\n
Example: L:RAISED,1F
Rate:    On change
States:  IDLE, RAISING, RAISED, LOWERING, LOWERED, FAULT
MAG_MASK: Hex bitmask of energized magnets (bits 0-4)
```

#### Diagnostics

```
Format:  D:STATE=N,LOOPS=N,CMDS=N,TIMEOUTS=N,CYCLES=N,LOOP_US=avg/max,FLT=flags,SRAM=N,UP=N\n
Rate:    Every 5 seconds
```

#### Status Message

```
Format:  S:message\n
Example: S:Minesweeper Motor Controller v2.0
```

#### Error Message

```
Format:  E:code:message\n
Example: E:3:IMU communication timeout
Codes:   1=Parser, 2=Safety, 3=IMU, 4=Lift, 5=Sensor
```
