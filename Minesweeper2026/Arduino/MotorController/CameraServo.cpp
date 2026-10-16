/**
 * ============================================================================
 * CAMERASERVO.CPP - Hardware Timer 3 Servo Controller Implementation
 * ============================================================================
 */

#include "CameraServo.h"
#include <avr/interrupt.h>
#include <util/atomic.h>

static volatile uint8_t s_servo_pin = 4;
static volatile uint16_t s_pulse_ticks = 3000; // Default 1500 us (90 deg)
static volatile int s_current_angle = 90;
static volatile bool s_attached = false;

// Direct port register access for fast ISR
static volatile uint8_t* s_port_reg = nullptr;
static uint8_t s_pin_mask = 0;

void CameraServo::attach(uint8_t pin) {
    s_servo_pin = pin;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);

    s_port_reg = portOutputRegister(digitalPinToPort(pin));
    s_pin_mask = digitalPinToBitMask(pin);

    // Stop Timer 3 during configuration
    TCCR3A = 0;
    TCCR3B = 0;
    TCNT3 = 0;

    // Fast PWM mode 14 (TOP = ICR3)
    // Prescaler 8: 16 MHz / 8 = 2 MHz (0.5 us per tick)
    // TOP = 39999 -> 40000 ticks = 20,000 us = 20 ms (50 Hz frame rate)
    ICR3 = 39999;

    // Set 90 degrees default (1500 us -> 3000 ticks)
    write(90);

    // Mode 14: WGM33:WGM32:WGM31:WGM30 = 1:1:1:0
    TCCR3A = (1 << WGM31);
    TCCR3B = (1 << WGM33) | (1 << WGM32) | (1 << CS31); // Prescaler = 8

    // Enable Overflow (start of 20ms pulse) and Compare Match A (end of pulse)
    TIMSK3 = (1 << TOIE3) | (1 << OCIE3A);
    s_attached = true;
}

void CameraServo::write(int angle_deg) {
    if (angle_deg < 0) angle_deg = 0;
    if (angle_deg > 180) angle_deg = 180;
    s_current_angle = angle_deg;

    // Standard servo pulse: 600 us (0 deg) to 2400 us (180 deg)
    // At 2 MHz, 1 us = 2 ticks. 600 us = 1200 ticks, 2400 us = 4800 ticks.
    uint32_t pulse_us = 600 + ((uint32_t)angle_deg * 1800) / 180;
    uint16_t ticks = static_cast<uint16_t>(pulse_us * 2);

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        s_pulse_ticks = ticks;
        OCR3A = ticks;
    }
}

int CameraServo::read() {
    return s_current_angle;
}

void CameraServo::detach() {
    TIMSK3 = 0;
    TCCR3A = 0;
    TCCR3B = 0;
    if (s_port_reg != nullptr) {
        *s_port_reg &= ~s_pin_mask;
    }
    s_attached = false;
}

// Start of 20 ms cycle: set pin HIGH
ISR(TIMER3_OVF_vect) {
    if (s_port_reg != nullptr && s_attached) {
        *s_port_reg |= s_pin_mask;
    }
}

// End of pulse duration (OCR3A match): set pin LOW
ISR(TIMER3_COMPA_vect) {
    if (s_port_reg != nullptr && s_attached) {
        *s_port_reg &= ~s_pin_mask;
    }
}
