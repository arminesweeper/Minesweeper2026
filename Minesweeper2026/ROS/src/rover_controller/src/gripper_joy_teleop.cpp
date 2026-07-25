#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joy.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float64_multi_array.hpp"

#include <algorithm>
#include <cmath>

using std::placeholders::_1;

class GripperJoyTeleop : public rclcpp::Node
{
public:
    GripperJoyTeleop() : Node("gripper_joy_teleop")
    {
        declare_parameter("open_button_index", 2);
        declare_parameter("close_button_index", 3);
        declare_parameter("watchdog_timeout_sec", 3.0);

        open_button_index_ = get_parameter("open_button_index").as_int();
        close_button_index_ = get_parameter("close_button_index").as_int();
        watchdog_timeout_sec_ = get_parameter("watchdog_timeout_sec").as_double();

        cmd_pub_ = create_publisher<std_msgs::msg::Float64MultiArray>(
            "/gripper_controller/commands", 10);

        joy_sub_ = create_subscription<sensor_msgs::msg::Joy>(
            "/joy", 10, std::bind(&GripperJoyTeleop::joyCallback, this, _1));

        joint_state_sub_ = create_subscription<sensor_msgs::msg::JointState>(
            "/joint_states", 10, std::bind(&GripperJoyTeleop::jointStateCallback, this, _1));

        RCLCPP_INFO(get_logger(),
                    "Gripper joy teleop ready. Open button: %ld, Close button: %ld",
                    open_button_index_, close_button_index_);
    }

private:
    void joyCallback(const sensor_msgs::msg::Joy &msg)
    {
        size_t max_needed = std::max(open_button_index_, close_button_index_);
        if (msg.buttons.size() <= max_needed)
        {
            return;  // controller doesn't have enough buttons, ignore
        }

        bool open_pressed = msg.buttons[open_button_index_] != 0;
        bool close_pressed = msg.buttons[close_button_index_] != 0;

        // Rising-edge detection: act only the instant the button goes from
        // not-pressed to pressed, not on every Joy message while held.
        if (open_pressed && !prev_open_pressed_)
        {
            sendCommand(1.0, "open");
        }
        else if (close_pressed && !prev_close_pressed_)
        {
            sendCommand(-1.0, "close");
        }

        prev_open_pressed_ = open_pressed;
        prev_close_pressed_ = close_pressed;
    }

    void sendCommand(double value, const std::string &label)
    {
        if (command_active_)
        {
            RCLCPP_WARN(get_logger(), "Ignoring '%s' request, gripper is still moving.", label.c_str());
            return;
        }

        std_msgs::msg::Float64MultiArray msg;
        msg.data.push_back(value);
        cmd_pub_->publish(msg);
        RCLCPP_INFO(get_logger(), "Gripper command sent: %s", label.c_str());

        command_active_ = true;
        startWatchdog();
    }

    void startWatchdog()
    {
        cancelWatchdog();
        watchdog_timer_ = create_wall_timer(
            std::chrono::duration<double>(watchdog_timeout_sec_),
            std::bind(&GripperJoyTeleop::watchdogTimeoutCallback, this));
    }

    void cancelWatchdog()
    {
        if (watchdog_timer_)
        {
            watchdog_timer_->cancel();
            watchdog_timer_.reset();
        }
    }

    void watchdogTimeoutCallback()
    {
        RCLCPP_WARN(get_logger(),
                    "Gripper watchdog timeout - limit switch never confirmed. Stopping motor from ROS side.");
        std_msgs::msg::Float64MultiArray stop_msg;
        stop_msg.data.push_back(0.0);
        cmd_pub_->publish(stop_msg);
        command_active_ = false;
        cancelWatchdog();
    }

    void jointStateCallback(const sensor_msgs::msg::JointState &msg)
    {
        if (!command_active_)
        {
            return;
        }

        auto it = std::find(msg.name.begin(), msg.name.end(), "gripper_joint");
        if (it == msg.name.end())
        {
            return;
        }

        size_t idx = std::distance(msg.name.begin(), it);
        double position = msg.position.at(idx);

        bool reached_open = std::abs(position - 1.0) < position_tolerance_;
        bool reached_closed = std::abs(position - 0.0) < position_tolerance_;

        if (reached_open || reached_closed)
        {
            RCLCPP_INFO(get_logger(), "Gripper reached end position.");
            command_active_ = false;
            cancelWatchdog();
        }
    }

    rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr cmd_pub_;
    rclcpp::Subscription<sensor_msgs::msg::Joy>::SharedPtr joy_sub_;
    rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_state_sub_;
    rclcpp::TimerBase::SharedPtr watchdog_timer_;

    int64_t open_button_index_;
    int64_t close_button_index_;
    double watchdog_timeout_sec_;
    const double position_tolerance_ = 0.05;

    bool prev_open_pressed_ = false;
    bool prev_close_pressed_ = false;
    bool command_active_ = false;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<GripperJoyTeleop>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}