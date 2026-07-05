/**
 * ============================================================================
 * ODOMETRY.H - Position Tracking & Odometry
 * ============================================================================
 */

#ifndef ODOMETRY_H
#define ODOMETRY_H

#include "Config.h"
#include <Arduino.h>


/**
 * @brief Robot pose (position and orientation)
 */
struct Pose {
  double x;     // X position in meters
  double y;     // Y position in meters
  double theta; // Heading in radians
};

/**
 * @brief Odometry Tracker
 *
 * Computes robot position using differential drive kinematics:
 * - Integrates wheel velocities to estimate pose
 * - Uses standard unicycle model
 * - Provides position reset capability
 */
class Odometry {
public:
  /**
   * @brief Construct a new Odometry tracker
   */
  Odometry();

  /**
   * @brief Initialize odometry
   */
  void begin();

  /**
   * @brief Update position estimate
   * @param right_vel Right wheel velocity (rad/s)
   * @param left_vel Left wheel velocity (rad/s)
   * @param dt_sec Time step in seconds
   */
  void update(double right_vel, double left_vel, double dt_sec);

  /**
   * @brief Get current pose
   * @return Const reference to current pose
   */
  const Pose &getPose() const { return pose_; }

  /**
   * @brief Get X position
   * @return X in meters
   */
  double getX() const { return pose_.x; }

  /**
   * @brief Get Y position
   * @return Y in meters
   */
  double getY() const { return pose_.y; }

  /**
   * @brief Get heading angle
   * @return Theta in radians
   */
  double getTheta() const { return pose_.theta; }

  /**
   * @brief Get heading in degrees
   * @return Theta in degrees (-180 to 180)
   */
  double getThetaDegrees() const;

  /**
   * @brief Reset position to origin
   */
  void reset();

  /**
   * @brief Set position to specific values
   * @param x X position in meters
   * @param y Y position in meters
   * @param theta Heading in radians
   */
  void setPosition(double x, double y, double theta);

  /**
   * @brief Get total distance traveled
   * @return Distance in meters
   */
  double getDistanceTraveled() const { return distance_traveled_; }

private:
  Pose pose_;
  double distance_traveled_;

  // Normalize angle to [-PI, PI]
  static double normalizeAngle(double angle);
};

#endif // ODOMETRY_H