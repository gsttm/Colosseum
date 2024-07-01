// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef rotor_actuator_hpp
#define rotor_actuator_hpp

#include <limits>
#include "common/Common.hpp"
#include "physics/Environment.hpp"
#include "common/FirstOrderFilter.hpp"
#include "physics/PhysicsBodyVertex.hpp"
#include "RotorParams.hpp"

namespace msr
{
namespace airlib
{

    //Rotor gets control signal as input (PWM or voltage represented from 0 to 1) which causes
    //change in rotation speed and turning direction and ultimately produces force and thrust as
    //output
    class RotorActuator : public PhysicsBodyVertex
    {
    public: //types
        struct Output
        {
            real_T thrust;
            real_T torque_scaler;
            real_T speed;
            RotorTurningDirection turning_direction;
            real_T control_signal_filtered;
            real_T control_signal_input;
        };

    public: //methods
        RotorActuator()
        {
            //allow default constructor with later call for initialize
        }
        RotorActuator(const Vector3r& position, const Vector3r& normal, RotorTurningDirection turning_direction,
                      const RotorParams& params, const Environment* environment, uint id = -1)
        {
            initialize(position, normal, turning_direction, params, environment, id);
        }
        void initialize(const Vector3r& position, const Vector3r& normal, RotorTurningDirection turning_direction,
                        const RotorParams& params, const Environment* environment, uint id = -1)
        {
            id_ = id;
            params_ = params;
            turning_direction_ = turning_direction;
            environment_ = environment;
            air_density_sea_level_ = EarthUtils::getAirDensity(0.0f);

            control_signal_filter_.initialize(params_.control_signal_filter_tc, 0, 0);

            PhysicsBodyVertex::initialize(position, normal); //call base initializer
        }

        //0 to 1 - will be scaled to 0 to max_speed
        void setControlSignal(real_T control_signal)
        {
            control_signal_filter_.setInput(Utils::clip(control_signal, 0.0f, 1.0f));
        }

        Output getOutput() const
        {
            return output_;
        }

        //*** Start: UpdatableState implementation ***//
        virtual void resetImplementation() override
        {
            PhysicsBodyVertex::resetImplementation();

            //update environmental factors before we call base
            updateEnvironmentalFactors();

            control_signal_filter_.reset();

            setOutput(output_, params_, control_signal_filter_, turning_direction_);
        }

        virtual void update() override
        {
            //update environmental factors before we call base
            updateEnvironmentalFactors();

            //this will in turn call setWrench
            PhysicsBodyVertex::update();

            //update our state
            setOutput(output_, params_, control_signal_filter_, turning_direction_);

            //update filter - this should be after so that first output is same as initial
            control_signal_filter_.update();
        }

        virtual void reportState(StateReporter& reporter) override
        {
            reporter.writeValue("Dir", static_cast<int>(turning_direction_));
            reporter.writeValue("Ctrl-in", output_.control_signal_input);
            reporter.writeValue("Ctrl-fl", output_.control_signal_filtered);
            reporter.writeValue("speed", output_.speed);
            reporter.writeValue("thrust", output_.thrust);
            reporter.writeValue("torque", output_.torque_scaler);
        }
        //*** End: UpdatableState implementation ***//

    protected:
        virtual void setWrench(Wrench& wrench) override
        {
            Vector3r normal = getNormal();
            //forces and torques are proportional to air density: http://physics.stackexchange.com/a/32013/14061
            wrench.force = normal * output_.thrust * air_density_ratio_;
            wrench.torque = normal * output_.torque_scaler * air_density_ratio_; //TODO: try using filtered control here
        }

    private: //methods
        static void setOutput(Output& output, const RotorParams& params, const FirstOrderFilter<real_T>& control_signal_filter, RotorTurningDirection turning_direction)
        {
            output.control_signal_input = (float)control_signal_filter.getInput();
            output.control_signal_filtered = (float)control_signal_filter.getOutput();
            //see relationship of rotation speed with thrust: http://physics.stackexchange.com/a/32013/14061
            
            float motor_speeds[51] = { 0, 0, 0, 0, 0, 0, 0, 0, 3753, 4601, 5322, 5995, 6752, 7466, 8103, 8779, 9529, 10250, 10872, 11580, 12155, 12798, 13470, 14088, 14744, 15305, 15771, 16240, 16690, 17102, 17591, 18090, 18598, 19236, 19660, 20172, 20612, 21192, 21536, 19462, 22656, 22908, 23386, 23785, 24120, 24498, 24890, 25223, 25545, 25853, 26024 };
            float motor_thrusts[51] = {0, 0, 0, 0, 0, 0, 0, 0.00584632270310872, 0.0140733537902564, 0.0206459651921767, 0.026399911495665, 0.0366524399829979, 0.0479390717525605, 0.0576198156547865, 0.069191757451541, 0.0840208916811748, 0.100955497458412, 0.115705443598866, 0.135160093861176, 0.156054104097401, 0.169619722492329, 0.192924311325644, 0.218814158364533, 0.237798338214658, 0.265252150014848, 0.287170947287517, 0.30380510413816, 0.326548389162877, 0.347096533774302, 0.366393972388977, 0.389839469439802, 0.415023610860414, 0.437347664824767, 0.466642599698386, 0.489689827241867, 0.517066780014324, 0.546315133659014, 0.572179361021992, 0.599647147190861, 0.630189294469061, 0.662712308507479, 0.68725129990742, 0.714393017473784, 0.734342593293468, 0.760111329137141, 0.794010818490419, 0.815931944824534, 0.838023092644242, 0.863345813220917, 0.883708797447349, 0.894279242822124 };
            float converted_signal = 1000 + output.control_signal_filtered * 1000;
            float rounded = ((converted_signal+ 20 - 1) / 20) * 20;
            int closest_idx = static_cast<int>((rounded - 1000) / 51);

            output.speed = motor_speeds[closest_idx];
            output.thrust = 9.8*motor_thrusts[closest_idx];
            //output.speed = sqrt(output.control_signal_filtered * params.max_speed_square);
            //output.thrust = output.control_signal_filtered * params.max_thrust;
            output.torque_scaler = (float) (output.thrust*12) * static_cast<int>(turning_direction);
            output.turning_direction = turning_direction;
        }

        void updateEnvironmentalFactors()
        {
            //update air density ration - this will affect generated force and torques by rotors
            air_density_ratio_ = environment_->getState().air_density / air_density_sea_level_;
        }

    private: //fields
        uint id_; //only used for debug messages
        RotorTurningDirection turning_direction_;
        RotorParams params_;
        FirstOrderFilter<real_T> control_signal_filter_;
        const Environment* environment_ = nullptr;
        real_T air_density_sea_level_, air_density_ratio_;
        Output output_;
    };
}
} //namespace
#endif
