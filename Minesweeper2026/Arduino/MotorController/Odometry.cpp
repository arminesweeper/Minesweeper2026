/**
 * ============================================================================
 * ODOMETRY.CPP - Odometry Implementation
 * ============================================================================
 */

#include "Odometry.h"

Odometry::Odometry() : distance_traveled_(0.0) {
  pose_.x = 0.0;
  pose_.y = 0.0;
  pose_.theta = 0.0;
}

void Odometry::begin() { reset(); }

void Odometry::update(double right_vel, double left_vel, double dt_sec) {
#if ENABLE_ODOMETRY
  if (dt_sec <= 0.0)
    return;

  // Convert angular velocities to linear velocities (m/s)
  // v = omega * r
  double v_right = right_vel * RobotParams::WHEEL_RADIUS_M;
  double v_left = left_vel * RobotParams::WHEEL_RADIUS_M;

  // Calculate robot velocities (unicycle model)
  // v = (v_right + v_left) / 2
  // omega = (v_right - v_left) / L
  double v_robot = (v_right + v_left) / 2.0;
  double omega_robot = (v_right - v_left) / RobotParams::WHEEL_BASE_M;

  // Integrate position using exact integration for constant velocity
  if (abs(omega_robot) < 1e-6) {
    // Straight line motion
    pose_.x += v_robot * dt_sec * cos(pose_.theta);
    pose_.y += v_robot * dt_sec * sin(pose_.theta);
  } else {
    // Arc motion
    double R = v_robot / omega_robot;
    pose_.x += R * (sin(pose_.theta + omega_robot * dt_sec) - sin(pose_.theta));
    pose_.y +=
        R * (-cos(pose_.theta + omega_robot * dt_sec) + cos(pose_.theta));
  }

  // Update heading
  pose_.theta += omega_robot * dt_sec;
  pose_.theta = normalizeAngle(pose_.theta);

  // Accumulate distance
  distance_traveled_ += abs(v_robot) * dt_sec;
#endif
}

double Odometry::getThetaDegrees() const {
  return pose_.theta * 180.0 / 3.14159265359;
}

void Odometry::reset() {
  pose_.x = 0.0;
  pose_.y = 0.0;
  pose_.theta = 0.0;
  distance_traveled_ = 0.0;
}

void Odometry::setPosition(double x, double y, double theta) {
  pose_.x = x;
  pose_.y = y;
  pose_.theta = normalizeAngle(theta);
}

double Odometry::normalizeAngle(double angle) {
  while (angle > 3.14159265359) {
    angle -= 2.0 * 3.14159265359;
  }
  while (angle < -3.14159265359) {
    angle += 2.0 * 3.14159265359;
  }
  return angle;
}