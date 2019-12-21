#ifndef LAYERED_HARDWARE_JOINT_LIMITS_LAYER_HPP
#define LAYERED_HARDWARE_JOINT_LIMITS_LAYER_HPP

#include <list>
#include <string>

#include <hardware_interface/controller_info.h>
#include <hardware_interface/internal/demangle_symbol.h>
#include <hardware_interface/joint_command_interface.h>
#include <hardware_interface/posvel_command_interface.h>
#include <hardware_interface/robot_hw.h>
#include <joint_limits_interface/joint_limits_interface.h>
#include <joint_limits_interface/joint_limits_urdf.h>
#include <layered_hardware/common_namespaces.hpp>
#include <layered_hardware/layer_base.hpp>
#include <ros/console.h>
#include <ros/duration.h>
#include <ros/node_handle.h>
#include <ros/time.h>
#include <urdf/model.h>

#include <boost/foreach.hpp>

namespace layered_hardware {

// TODO: support soft limits

class JointLimitsLayer : public LayerBase {
public:
  virtual bool init(hi::RobotHW &hw, ros::NodeHandle &param_nh, const std::string &urdf_str) {
    // we do NOT register joint limit interfaces to the hardware
    // to prevent other layers from updating the interfaces
    // because the interfaces are stateful
    /*
    hw.registerInterface(&pos_iface_);
    ...
    */

    // extract the robot model from the given URDF, which contains joint limits info
    urdf::Model urdf_model;
    if (!urdf_model.initString(urdf_str)) {
      ROS_ERROR("JointLimitsLayer::init(): Failed to parse URDF");
      return false;
    }

    // associate joints already registered in the joint interface of the hardware
    // and joint limits loaded from the URDF.
    // associated pairs will be stored in the limits interface.
    tieJointsAndLimits< hi::PositionJointInterface, jli::PositionJointSaturationHandle >(
        hw, urdf_model, pos_iface_);
    tieJointsAndLimits< hi::VelocityJointInterface, jli::VelocityJointSaturationHandle >(
        hw, urdf_model, vel_iface_);
    tieJointsAndLimits< hi::EffortJointInterface, jli::EffortJointSaturationHandle >(hw, urdf_model,
                                                                                     eff_iface_);

    return true;
  }

  virtual void doSwitch(const std::list< hi::ControllerInfo > &start_list,
                        const std::list< hi::ControllerInfo > &stop_list) {
    // nothing to do
  }

  virtual void read(const ros::Time &time, const ros::Duration &period) {
    // nothing to do
  }

  virtual void write(const ros::Time &time, const ros::Duration &period) {
    // saturate joint commands
    pos_iface_.enforceLimits(period);
    vel_iface_.enforceLimits(period);
    eff_iface_.enforceLimits(period);
  }

private:
  template < typename CommandInterface, typename SaturationHandle, typename SaturationInterface >
  void tieJointsAndLimits(hi::RobotHW &hw, const urdf::Model &urdf_model,
                          SaturationInterface &sat_iface) {
    // find joint command interface that joints have been registered
    CommandInterface *const cmd_iface(hw.get< CommandInterface >());
    if (!cmd_iface) {
      return;
    }

    // associate joints already registered and limits in the given URDF
    const std::vector< std::string > hw_jnt_names(cmd_iface->getNames());
    BOOST_FOREACH (const std::string &hw_jnt_name, hw_jnt_names) {
      // find limits of a joint
      const urdf::JointConstSharedPtr urdf_jnt(urdf_model.getJoint(hw_jnt_name));
      if (!urdf_jnt) {
        continue;
      }
      jli::JointLimits limits;
      if (!jli::getJointLimits(urdf_jnt, limits)) {
        continue;
      }
      // register new associated pair
      ROS_INFO_STREAM("JointLimitsLayer::init(): Initialized the joint limits ("
                      << hi::internal::demangledTypeName< SaturationHandle >() << ") for the joint '"
                      << hw_jnt_name << "'");
      sat_iface.registerHandle(SaturationHandle(cmd_iface->getHandle(hw_jnt_name), limits));
    }
  }

private:
  jli::PositionJointSaturationInterface pos_iface_;
  jli::VelocityJointSaturationInterface vel_iface_;
  jli::EffortJointSaturationInterface eff_iface_;
};
} // namespace layered_hardware

#endif