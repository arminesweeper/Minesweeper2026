/**
 * ============================================================================
 * CAMERASERVO.H - Hardware Timer 3 Servo Controller for ATmega2560
 * ============================================================================
 * Generates precise 50 Hz PWM servo pulses on Pin 4 using Timer 3.
 *
 * CRITICAL ARCHITECTURAL DESIGN:
 * Standard Arduino <Servo.h> on ATmega2560 unconditionally allocates Timer 5
 * for its first servo instance. Pins 44 and 46 (MOTOR_R_PWM and MOTOR_L_PWM)
 * are driven by Timer 5 (OC5C and OC5A). Using <Servo.h> therefore corrupts
 * hardware PWM generation on the main drive wheels.
 *
 * CameraServo uses Timer 3 (which is completely free and unmapped on the robot),
 * protecting Timer 5, Timer 1, Timer 2, and Timer 0 completely.
 *
 * @file   CameraServo.h
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#ifndef CAMERASERVO_H
#define CAMERASERVO_H

#include <Arduino.h>

class CameraServo {
public:
    /**
     * @brief Initialize Timer 3 and configure servo output pin.
     * @param pin Output pin number (default: Pins::SERVO = 4)
     */
    static void attach(uint8_t pin);

    /**
     * @brief Set servo angle in degrees (0 to 180).
     * @param angle_deg Commanded angle (clamped 0 to 180)
     */
    static void write(int angle_deg);

    /**
     * @brief Get current commanded angle.
     * @return Angle in degrees [0-180]
     */
    static int read();

    /**
     * @brief Detach servo and disable Timer 3 interrupts.
     */
    static void detach();
};

#endif // CAMERASERVO_H
