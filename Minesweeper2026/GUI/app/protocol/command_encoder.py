"""
Serial Command Encoder for Minesweeper Robot.
Encodes verified movement, lift, servo, and system commands for the Arduino Mega 2560.
Strictly adheres to SerialProtocol.cpp and SystemManager.cpp.
"""

from typing import Optional


class CommandEncoder:
    """Encodes robot commands into verified protocol byte sequences."""

    DEFAULT_MAX_VELOCITY = 10.0  # rad/s (from Config.h RobotParameters)

    def __init__(self, max_velocity: float = DEFAULT_MAX_VELOCITY):
        self.max_velocity = max_velocity

    def clamp_velocity(self, val: float) -> float:
        """Clamp velocity to [-max_velocity, +max_velocity]."""
        return max(-self.max_velocity, min(self.max_velocity, float(val)))

    def format_motor_field(self, prefix: str, velocity: float) -> str:
        """
        Format a single motor velocity field.
        Example: prefix='r', val=2.50 -> 'rp02.50,'
                 prefix='l', val=-4.20 -> 'ln04.20,'
        """
        clamped = self.clamp_velocity(velocity)
        sign = "p" if clamped >= 0 else "n"
        abs_val = abs(clamped)
        # Format as 5 characters with 2 decimals (e.g. 05.00)
        return f"{prefix}{sign}{abs_val:05.2f},"

    def encode_drive(
        self,
        right_vel: float,
        left_vel: float,
        gripper_vel: Optional[float] = None,
        servo_angle: Optional[float] = None,
    ) -> bytes:
        """
        Encode drive velocity packet.
        Syntax: r<sign><XX.XX>,l<sign><XX.XX>,[g<sign><XX.XX>,][s<sign><XX.XX>,]\n
        """
        r_field = self.format_motor_field("r", right_vel)
        l_field = self.format_motor_field("l", left_vel)
        parts = [r_field, l_field]

        if gripper_vel is not None:
            g_clamped = max(-1.0, min(1.0, float(gripper_vel)))
            g_sign = "p" if g_clamped >= 0 else "n"
            parts.append(f"g{g_sign}{abs(g_clamped):05.2f},")

        if servo_angle is not None and 0.0 <= servo_angle <= 180.0:
            parts.append(f"sp{servo_angle:05.2f},")

        packet_str = "".join(parts) + "\n"
        return packet_str.encode("ascii")

    def encode_stop(self) -> bytes:
        """Neutral zero-velocity stop packet: rp00.00,lp00.00,\n"""
        return b"rp00.00,lp00.00,\n"

    def encode_estop(self) -> bytes:
        """Immediate software emergency stop command: CESTOP\n"""
        return b"CESTOP\n"

    def encode_clear_faults(self) -> bytes:
        """Clear faults and reset ESTOP state: CCLEAR\n"""
        return b"CCLEAR\n"

    def encode_reset(self) -> bytes:
        """Reset odometry, IMU yaw, and diagnostics: CRESET\n"""
        return b"CRESET\n"

    def encode_pid(self, enable: bool) -> bytes:
        """Enable or disable closed-loop PID velocity control."""
        return b"CPID:ON\n" if enable else b"CPID:OFF\n"

    def encode_lift_cycle(self) -> bytes:
        """Trigger automated mine pickup and drop cycle: CLIFT:CYCLE\n"""
        return b"CLIFT:CYCLE\n"

    def encode_lift_lower(self) -> bytes:
        """Lower lift mechanism to bottom limit switch: CLIFT:DN\n"""
        return b"CLIFT:DN\n"

    def encode_lift_stop(self) -> bytes:
        """Halt lift motor immediately: CLIFT:STOP\n"""
        return b"CLIFT:STOP\n"

    def encode_magnets(self, energize: bool) -> bytes:
        """Energize or de-energize all electromagnets."""
        return b"CMAGNET:ON\n" if energize else b"CMAGNET:OFF\n"

    def encode_siren(self, activate: bool) -> bytes:
        """Activate or silence the 12V alert siren."""
        return b"CBUZZ:ALERT\n" if activate else b"CBUZZ:SILENT\n"

    def encode_servo(self, angle_deg: int) -> bytes:
        """Set camera pan servo angle (0 to 180 degrees)."""
        clamped_angle = max(0, min(180, int(angle_deg)))
        return f"CSERVO:{clamped_angle}\n".encode("ascii")
