/**
 * ============================================================================
 * DIAGNOSTICS.CPP - System Diagnostics Implementation
 * ============================================================================
 * @file   Diagnostics.cpp
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#include "Diagnostics.h"
#include <stdio.h>

Diagnostics::Diagnostics()
    : loop_start_us_(0),
      loop_time_accumulator_(0),
      loop_accumulator_count_(0),
      last_report_ms_(0) {
    resetStats();
}

void Diagnostics::begin() {
    resetStats();
    last_report_ms_ = millis();
}

void Diagnostics::resetStats() {
    stats_.loop_count = 0;
    stats_.command_count = 0;
    stats_.timeout_count = 0;
    stats_.control_cycles = 0;
    stats_.max_loop_time_us = 0;
    stats_.min_loop_time_us = 0xFFFFFFFF;
    stats_.avg_loop_time_us = 0;
    stats_.last_loop_time_us = 0;

    loop_time_accumulator_ = 0;
    loop_accumulator_count_ = 0;
}

void Diagnostics::loopStart() {
    loop_start_us_ = micros();
}

void Diagnostics::loopEnd() {
    unsigned long now_us = micros();
    unsigned long loop_time = 0;

    /* Handle micros() overflow (~70 mins) */
    if (now_us >= loop_start_us_) {
        loop_time = now_us - loop_start_us_;
    } else {
        loop_time = (0xFFFFFFFF - loop_start_us_) + now_us + 1;
    }

    stats_.last_loop_time_us = loop_time;
    stats_.loop_count++;

    if (loop_time > stats_.max_loop_time_us) {
        stats_.max_loop_time_us = loop_time;
    }
    if (loop_time < stats_.min_loop_time_us) {
        stats_.min_loop_time_us = loop_time;
    }

    loop_time_accumulator_ += loop_time;
    loop_accumulator_count_++;

    /* Calculate average every 100 loops */
    if (loop_accumulator_count_ >= 100) {
        stats_.avg_loop_time_us = loop_time_accumulator_ / 100;
        loop_time_accumulator_ = 0;
        loop_accumulator_count_ = 0;
    }
}

void Diagnostics::formatFaultFlags(char* buf, FaultFlags faults) const {
    /* Format: trlbv (lowercase = OK, uppercase = FAULT) */
    buf[0] = faults.command_timeout ? 'T' : 't';
    buf[1] = faults.encoder_right   ? 'R' : 'r';
    buf[2] = faults.encoder_left    ? 'L' : 'l';
    buf[3] = (faults.battery_low || faults.battery_high) ? 'B' : 'b';
    buf[4] = faults.velocity_limit  ? 'V' : 'v';
    buf[5] = faults.imu_fault       ? 'I' : 'i';
    buf[6] = '\0';
}

void Diagnostics::sendStatusReport(SystemState state, FaultFlags faults) {
#if ENABLE_DIAGNOSTICS
    unsigned long now = millis();
    if (now - last_report_ms_ >= DiagConfig::STATUS_INTERVAL_MS) {
        last_report_ms_ = now;

        char flt_str[8];
        formatFaultFlags(flt_str, faults);

        char buffer[SerialConfig::TX_BUFFER_SIZE * 2]; // Needs more space for long report
        snprintf(buffer, sizeof(buffer),
                 "D:STATE=%u,LOOPS=%lu,CMDS=%lu,TIMEOUTS=%lu,CYCLES=%lu,"
                 "LOOP_US=%lu/%lu,FLT=%s,SRAM=%u,UP=%lu",
                 static_cast<uint8_t>(state),
                 stats_.loop_count,
                 stats_.command_count,
                 stats_.timeout_count,
                 stats_.control_cycles,
                 stats_.avg_loop_time_us,
                 stats_.max_loop_time_us,
                 flt_str,
                 getFreeSRAM(),
                 getUptimeSeconds());

        Serial.println(buffer);

        /* Reset min/max after reporting so we catch new spikes */
        stats_.max_loop_time_us = 0;
        stats_.min_loop_time_us = 0xFFFFFFFF;
    }
#endif
}

/**
 * @brief Estimate free SRAM.
 * By taking the address of a local variable and subtracting the
 * value of the __brkval pointer (or __heap_start if __brkval is 0).
 */
uint16_t Diagnostics::getFreeSRAM() {
    extern int __heap_start, *__brkval;
    int v;
    return (uint16_t)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}