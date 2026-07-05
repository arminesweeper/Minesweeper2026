/**
 * ============================================================================
 * SAFETY.H - Safety Systems & Fault Detection
 * ============================================================================
 */

#ifndef SAFETY_H
#define SAFETY_H

#include "Config.h"
#include <Arduino.h>


/**
 * @brief System state enumeration
 */
enum class SystemState : uint8_t {
  BOOTING,       // System initializing
  ACTIVE,        // Normal operation
  ESTOP_TIMEOUT, // Command timeout triggered
  ESTOP_MANUAL,  // Manual emergency stop
  FAULT_ENCODER, // Encoder error detected
  FAULT_BATTERY, // Battery voltage out of range
  FAULT_UNKNOWN  // Unknown fault
};

/**
 * @brief Fault flags structure
 */
struct FaultFlags {
  bool command_timeout : 1;
  bool encoder_right : 1;
  bool encoder_left : 1;
  bool battery_low : 1;
  bool battery_high : 1;
  bool velocity_limit : 1;
  bool watchdog_reset : 1;
  uint8_t reserved : 1;
};

/**
 * @brief Safety Monitor
 *
 * Provides:
 * - Hardware watchdog timer management
 * - Command timeout detection
 * - Battery voltage monitoring
 * - Fault flag management
 * - State machine for system states
 */
class SafetyMonitor {
public:
  /**
   * @brief Construct a new Safety Monitor
   */
  SafetyMonitor();

  /**
   * @brief Initialize safety systems
   */
  void begin();

  /**
   * @brief Update safety checks (call in main loop)
   * @param last_command_time Time of last received command
   * @param right_vel Current right wheel velocity
   * @param left_vel Current left wheel velocity
   * @return Current system state
   */
  SystemState update(unsigned long last_command_time, double right_vel,
                     double left_vel);

  /**
   * @brief Trigger manual emergency stop
   */
  void triggerEStop();

  /**
   * @brief Clear emergency stop condition
   */
  void clearEStop();

  /**
   * @brief Get current system state
   * @return Current state
   */
  SystemState getState() const { return state_; }

  /**
   * @brief Check if system is in active (safe) state
   * @return true if system is active
   */
  bool isActive() const { return state_ == SystemState::ACTIVE; }

  /**
   * @brief Get fault flags
   * @return Current fault flags
   */
  FaultFlags getFaults() const { return faults_; }

  /**
   * @brief Check if any fault is active
   * @return true if any fault flag is set
   */
  bool hasFault() const;

  /**
   * @brief Set encoder fault flag
   * @param is_right true for right encoder, false for left
   * @param fault true to set fault, false to clear
   */
  void setEncoderFault(bool is_right, bool fault);

  /**
   * @brief Read battery voltage
   * @return Battery voltage in millivolts
   */
  uint16_t readBatteryVoltage() const;

  /**
   * @brief Reset watchdog timer (call frequently)
   */
  void resetWatchdog();

  /**
   * @brief Get state as string
   * @return Human-readable state string
   */
  const char *getStateString() const;

private:
  SystemState state_;
  FaultFlags faults_;
  unsigned long estop_trigger_time_;

  void setState(SystemState new_state);
  void checkBatteryVoltage();
};

#endif // SAFETY_H