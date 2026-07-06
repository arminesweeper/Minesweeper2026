/**
 * ============================================================================
 * DIAGNOSTICS.H - System Diagnostics & Statistics
 * ============================================================================
 * Tracks runtime performance metrics:
 *   - Loop iteration count and timing
 *   - PID control cycle count
 *   - Serial command statistics
 *   - SRAM usage estimation
 *   - Uptime
 *
 * @file   Diagnostics.h
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "Config.h"
#include "Safety.h" // For SystemState and FaultFlags
#include <Arduino.h>

/**
 * @brief Runtime performance statistics
 */
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

/**
 * @brief Diagnostics Manager
 */
class Diagnostics {
public:
    Diagnostics();

    void begin();

    /** @brief Call at the very beginning of loop() */
    void loopStart();

    /** @brief Call at the very end of loop() */
    void loopEnd();

    void incrementCommandCount() { stats_.command_count++; }
    void incrementTimeoutCount() { stats_.timeout_count++; }
    void incrementControlCycleCount() { stats_.control_cycles++; }

    /**
     * @brief Output periodic diagnostic report via serial.
     * @param state Current system state
     * @param faults Current fault flags
     */
    void sendStatusReport(SystemState state, FaultFlags faults);

    const SystemStats& getStats() const { return stats_; }
    void resetStats();

    /**
     * @brief Estimate free SRAM.
     * @return Number of free bytes between heap and stack.
     */
    static uint16_t getFreeSRAM();

    /**
     * @brief Get system uptime.
     * @return Uptime in seconds
     */
    static uint32_t getUptimeSeconds() { return millis() / 1000; }

private:
    SystemStats stats_;
    unsigned long loop_start_us_;
    uint32_t loop_time_accumulator_;
    uint32_t loop_accumulator_count_;
    unsigned long last_report_ms_;

    /** @brief Format fault flags as a short string for telemetry */
    void formatFaultFlags(char* buf, FaultFlags faults) const;
};

#endif // DIAGNOSTICS_H