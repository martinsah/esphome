#pragma once

#include "pd_pioneer_protocol.h"
#include "esphome/components/climate/climate_mode.h"

namespace esphome {
namespace pd_pioneer_ir {

using climate::ClimateMode;
using climate::ClimateFanMode;
using remote_base::PDPioneerData;

class ControlData : public PDPioneerData {
 public:
  // Default constructor (power: ON, mode: AUTO, fan: AUTO, temp: 25C)
  ControlData() : PDPioneerData({PDPioneerData::PDPIONEER_TYPE_CONTROL, 0x82, 0x48, 0xFF, 0xFF}) {}
  // Copy from Base
  ControlData(const PDPioneerData &data) : PDPioneerData(data) {}

  void set_temp(float temp);
  float get_temp() const;

  void set_mode(ClimateMode mode);
  ClimateMode get_mode() const;

  void set_fan_mode(ClimateFanMode mode);
  ClimateFanMode get_fan_mode() const;

  void set_sleep_preset(bool value) { this->set_mask_(1, value, 64); }
  bool get_sleep_preset() const { return this->get_value_(1, 64); }

  void set_fahrenheit(bool value) { this->set_mask_(2, value, 32); }
  bool get_fahrenheit() const { return this->get_value_(2, 32); }

  void fix();

 protected:
  enum Mode : uint8_t {
    MODE_COOL,
    MODE_DRY,
    MODE_AUTO,
    MODE_HEAT,
    MODE_FAN_ONLY,
  };
  enum FanMode : uint8_t {
    FAN_AUTO,
    FAN_LOW,
    FAN_MEDIUM,
    FAN_HIGH,
  };
  void set_fan_mode_(FanMode mode) { this->set_value_(1, mode, 3, 3); }
  FanMode get_fan_mode_() const { return static_cast<FanMode>(this->get_value_(1, 3, 3)); }
  void set_mode_(Mode mode) { this->set_value_(1, mode, 7); }
  Mode get_mode_() const { return static_cast<Mode>(this->get_value_(1, 7)); }
  void set_power_(bool value) { this->set_mask_(1, value, 128); }
  bool get_power_() const { return this->get_value_(1, 128); }
};

class FollowMeData : public PDPioneerData {
 public:
  // Default constructor (temp: 30C, beeper: off)
  FollowMeData() : PDPioneerData({PDPioneerData::PDPIONEER_TYPE_FOLLOW_ME, 0x82, 0x48, 0x7F, 0x1F}) {}
  // Copy from Base
  FollowMeData(const PDPioneerData &data) : PDPioneerData(data) {}
  // Direct from temperature and beeper values
  FollowMeData(uint8_t temp, bool beeper = false) : FollowMeData() {
    this->set_temp(temp);
    this->set_beeper(beeper);
  }

  /* TEMPERATURE */
  uint8_t temp() const { return this->get_value_(4) - 1; }
  void set_temp(uint8_t val) { this->set_value_(4, std::min(MAX_TEMP, val) + 1); }

  /* BEEPER */
  bool beeper() const { return this->get_value_(3, 128); }
  void set_beeper(bool value) { this->set_mask_(3, value, 128); }

 protected:
  static const uint8_t MAX_TEMP = 37;
};

class SpecialData : public PDPioneerData {
 public:
  SpecialData(uint8_t code) : PDPioneerData({PDPioneerData::PDPIONEER_TYPE_SPECIAL, code, 0xFF, 0xFF, 0xFF}) {}
  static const uint8_t VSWING_STEP = 1;
  static const uint8_t VSWING_TOGGLE = 2;
  static const uint8_t TURBO_TOGGLE = 9;
};

}  // namespace pd_pioneer_ir
}  // namespace esphome
