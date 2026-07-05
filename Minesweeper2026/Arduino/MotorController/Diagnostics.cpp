/**
 * ============================================================================
 * DIAGNOSTICS.CPP - Diagnostics Implementation
 * ============================================================================
 */

#include "Diagnostics.h"

Diagnostics::Diagnostics()
    : loop_start_time_us_(0), last_status_time_ms_(0),
      loop_time_accumulator_(0), loop_time_samples_(0) {
  resetStats();
}

void Diagnostics::begin() {
  resetStats();
  last_status_time_ms_ = millis();
}

void Diagnostics::loopStart() { loop_start_time_us_ = micros(); }

void Diagnostics::loopEnd() {
  unsigned long now = micros();
  uint32_t loop_time = now - loop_start_time_us_;

  stats_.last_loop_time_us = loop_time;
  stats_.loop_count++;

  updateTimingStats(loop_time);
}

void Diagnostics::updateTimingStats(uint32_t loop_time_us) {
  // Update min/max
  if (loop_time_us > stats_.max_loop_time_us) {
    stats_.max_loop_time_us = loop_time_us;
  }
  if (loop_time_us < stats_.min_loop_time_us || stats_.min_loop_time_us == 0) {
    stats_.min_loop_time_us = loop_time_us;
  }

  // Accumulate for average calculation
  loop_time_accumulator_ += loop_time_us;
  loop_time_samples_++;

  // Calculate running average every 100 samples
  if (loop_time_samples_ >= 100) {
    stats_.avg_loop_time_us = loop_time_accumulator_ / loop_time_samples_;
    loop_time_accumulator_ = 0;
    loop_time_samples_ = 0;
  }
}

void Diagnostics::sendStatusReport(SystemState state, FaultFlags faults) {
#if ENABLE_DIAGNOSTICS
  unsigned long now = millis();

  if (now - last_status_time_ms_ >= DiagConfig::STATUS_INTERVAL_MS) {
    last_status_time_ms_ = now;

    Serial.print(F("D:STATE="));
    Serial.print(static_cast<uint8_t>(state));

    Serial.print(F(",LOOPS="));
    Serial.print(stats_.loop_count);

    Serial.print(F(",CMDS="));
    Serial.print(stats_.command_count);

    Serial.print(F(",TIMEOUTS="));
    Serial.print(stats_.timeout_count);

    Serial.print(F(",CYCLES="));
    Serial.print(stats_.control_cycles);

    Serial.print(F(",LOOP_US="));
    Serial.print(stats_.avg_loop_time_us);
    Serial.print('/');
    Serial.print(stats_.max_loop_time_us);

    Serial.print(F(",FLT="));
    Serial.print(faults.command_timeout ? 'T' : 't');
    Serial.print(faults.encoder_right ? 'R' : 'r');
    Serial.print(faults.encoder_left ? 'L' : 'l');
    Serial.print(faults.battery_low ? 'B' : 'b');
    Serial.print(faults.velocity_limit ? 'V' : 'v');

    Serial.println();
  }
#endif
}

void Diagnostics::resetStats() {
  stats_.loop_count = 0;
  stats_.command_count = 0;
  stats_.timeout_count = 0;
  stats_.control_cycles = 0;
  stats_.max_loop_time_us = 0;
  stats_.min_loop_time_us = 0;
  stats_.avg_loop_time_us = 0;
  stats_.last_loop_time_us = 0;
  loop_time_accumulator_ = 0;
  loop_time_samples_ = 0;
}