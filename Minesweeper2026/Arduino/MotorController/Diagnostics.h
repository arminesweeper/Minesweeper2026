/**
 * ============================================================================
 * DIAGNOSTICS.H - System Diagnostics & Telemetry
 * ============================================================================
 */

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "Config.h"
#include "Safety.h"
#include <Arduino.h>


/**
 * @brief System statistics
 */
struct SystemStats {
  uint32_t loop_count;
  uint32_t command_count;
  uint32_t timeout_count;
  uint32_t control_cycles;
  uint32_t max_loop_time_us;
  uint32_t min_loop_time_us;
  uint32_t avg_loop_time_us;
  uint32_t last_loop_time_us;
};

/**
 * @brief Diagnostics Manager
 *
 * Provides:
 * - Loop timing statistics
 * - Command statistics
 * - Periodic status reporting
 * - Debug output control
 */
class Diagnostics {
public:
  /**
   * @brief Construct a new Diagnostics manager
   */
  Diagnostics();

  /**
   * @brief Initialize diagnostics
   */
  void begin();

  /**
   * @brief Mark start of loop iteration
   */
  void loopStart();

  /**
   * @brief Mark end of loop iteration and update stats
   */
  void loopEnd();

  /**
   * @brief Increment command counter
   */
  void incrementCommandCount() { stats_.command_count++; }

  /**
   * @brief Increment timeout counter
   */
  void incrementTimeoutCount() { stats_.timeout_count++; }

  /**
   * @brief Increment control cycle counter
   */
  void incrementControlCycleCount() { stats_.control_cycles++; }

  /**
   * @brief Send periodic status report
   * @param state Current system state
   * @param faults Current fault flags
   */
  void sendStatusReport(SystemState state, FaultFlags faults);

  /**
   * @brief Get current statistics
   * @return Const reference to statistics
   */
  const SystemStats &getStats() const { return stats_; }

  /**
   * @brief Reset statistics
   */
  void resetStats();

private:
  SystemStats stats_;
  unsigned long loop_start_time_us_;
  unsigned long last_status_time_ms_;
  uint32_t loop_time_accumulator_;
  uint32_t loop_time_samples_;

  void updateTimingStats(uint32_t loop_time_us);
};

#endif // DIAGNOSTICS_H