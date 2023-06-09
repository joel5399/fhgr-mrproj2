#include <ros/ros.h>
#include <std_msgs/Int32.h>
#include <cmath>

#include <pigpiod_if2.h>

#include "smt_gun_controller/gun_controller.hpp"



namespace smt {
    namespace gun_controller {
        enum class Pin {
            GUN_TRIGGER = 18, //!< gun trigger (Pyhsical 12)
            PWM_SERVO = 12,   //!< pwm motor right (Physical 32)
        };

        const int defaultPipeAngle = 0;
        const int minimalPipeAngle = -15;
        const int maximalPipeAngle = 50;
        const uint positivePulseWidthLimitation = 2050;
        const uint negativePulseWidthLimitation = 1250;

        /**
         * This function calculates the pulse width based on the given pipe angle, with limitations on the
         * angle range.
         *
         * @param pipeAngle The angle of a pipe in degrees.
         *
         * @return an integer value which represents the pulseWidth calculated based on the input pipe angle.
         */
        uint getPulseWidthFromPipeAngle(int const pipeAngle) {
            if (pipeAngle < minimalPipeAngle) {
                ROS_WARN("reached minimum angle limitation (%i deg), setting angle to %i deg", minimalPipeAngle, minimalPipeAngle);
                return negativePulseWidthLimitation;
            }

            if (pipeAngle > maximalPipeAngle) {
                ROS_WARN("reached maximum angle limitation (%i deg), setting angle to %i deg", maximalPipeAngle, maximalPipeAngle);
                return positivePulseWidthLimitation;
            }

            return (pipeAngle - minimalPipeAngle) *
                (positivePulseWidthLimitation - negativePulseWidthLimitation) /
                (maximalPipeAngle - minimalPipeAngle) +
                negativePulseWidthLimitation;
        }

        GunController::GunController(ros::NodeHandle& nodeHandle) {
            std::string gunControllerService;

            if (!nodeHandle.getParam("gun_controller_service", gunControllerService)) {
                ROS_ERROR("failed to load the `gun_controller_service` parameter!");
                ros::requestShutdown();
            }

            this->service = nodeHandle.advertiseService(gunControllerService, &GunController::gunCommandCallback, this);
            ROS_INFO_STREAM("starting service " << gunControllerService);

            this->initPi();

            ROS_INFO("init done");
        }

        GunController::~GunController() {
            pigpio_stop(this->pi);
        }

        void GunController::initPi() {
            this->pi = pigpio_start(NULL, NULL);
            if (this->pi < 0) {
                throw std::runtime_error("error while trying to initialize pigpio!");
                return;
            }

            // GPIOs
            set_mode(this->pi, static_cast<uint>(Pin::GUN_TRIGGER), PI_OUTPUT);
            gpio_write(this->pi, static_cast<uint>(Pin::GUN_TRIGGER), 1);

            // PWMs
            set_mode(this->pi, static_cast<uint>(Pin::PWM_SERVO), PI_OUTPUT);
            this->moveGunToAngle(defaultPipeAngle);
        }

        bool GunController::gunCommandCallback(smt_svcs::FireGun::Request& request, smt_svcs::FireGun::Response& response) {
            this->moveGunToAngle(request.angle);

            // use a small delay to make it look smoother
            ros::Duration(0.5).sleep();
            fireShot();
            ros::Duration(0.5).sleep();

            this->moveGunToAngle(defaultPipeAngle);

            response.success = true;
            return true;
        }

        void GunController::fireShot() const {
            gpio_write(this->pi, static_cast<uint>(Pin::GUN_TRIGGER), 0);
            ros::Duration(0.1).sleep();
            gpio_write(this->pi, static_cast<uint>(Pin::GUN_TRIGGER), 1);
        }

        void GunController::moveGunToAngle(int const targetAngle) const {
            auto const targetPulseWidth = getPulseWidthFromPipeAngle(targetAngle);

            set_servo_pulsewidth(this->pi, static_cast<uint>(Pin::PWM_SERVO), targetPulseWidth);
            ros::Duration(0.5).sleep();
        }

    } // namespace movement_controller
} // namespace smt
