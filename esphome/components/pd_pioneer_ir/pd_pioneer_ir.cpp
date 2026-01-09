#include "pd_pioneer_ir.h"
#include "pd_pioneer_data.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pd_pioneer_ir {

static const char *const TAG = "pd_pioneer_ir.climate";

void ControlData::set_temp(float temp) {
  uint8_t min;
  if (this->get_fahrenheit()) {
    min = PDPIONEER_TEMPF_MIN;
    temp = esphome::clamp<float>(celsius_to_fahrenheit(temp), PDPIONEER_TEMPF_MIN, PDPIONEER_TEMPF_MAX);
  } else {
    min = PDPIONEER_TEMPC_MIN;
    temp = esphome::clamp<float>(temp, PDPIONEER_TEMPC_MIN, PDPIONEER_TEMPC_MAX);
  }
  this->set_value_(2, lroundf(temp) - min, 31);
}

float ControlData::get_temp() const {
  const uint8_t temp = this->get_value_(2, 31);
  if (this->get_fahrenheit())
    return fahrenheit_to_celsius(static_cast<float>(temp + PDPIONEER_TEMPF_MIN));
  return static_cast<float>(temp + PDPIONEER_TEMPC_MIN);
}

void ControlData::fix() {
  // In FAN_AUTO, modes COOL, HEAT and FAN_ONLY bit #5 in byte #1 must be set
  const uint8_t value = this->get_value_(1, 31);
  if (value == 0 || value == 3 || value == 4)
    this->set_mask_(1, true, 32);
  // In FAN_ONLY mode we need to set all temperature bits
  if (this->get_mode_() == MODE_FAN_ONLY)
    this->set_mask_(2, true, 31);
}

void ControlData::set_mode(ClimateMode mode) {
  switch (mode) {
    case ClimateMode::CLIMATE_MODE_OFF:
      this->set_power_(false);
      return;
    case ClimateMode::CLIMATE_MODE_COOL:
      this->set_mode_(MODE_COOL);
      break;
    case ClimateMode::CLIMATE_MODE_DRY:
      this->set_mode_(MODE_DRY);
      break;
    case ClimateMode::CLIMATE_MODE_FAN_ONLY:
      this->set_mode_(MODE_FAN_ONLY);
      break;
    case ClimateMode::CLIMATE_MODE_HEAT:
      this->set_mode_(MODE_HEAT);
      break;
    default:
      this->set_mode_(MODE_AUTO);
      break;
  }
  this->set_power_(true);
}

ClimateMode ControlData::get_mode() const {
  if (!this->get_power_())
    return ClimateMode::CLIMATE_MODE_OFF;
  switch (this->get_mode_()) {
    case MODE_COOL:
      return ClimateMode::CLIMATE_MODE_COOL;
    case MODE_DRY:
      return ClimateMode::CLIMATE_MODE_DRY;
    case MODE_FAN_ONLY:
      return ClimateMode::CLIMATE_MODE_FAN_ONLY;
    case MODE_HEAT:
      return ClimateMode::CLIMATE_MODE_HEAT;
    default:
      return ClimateMode::CLIMATE_MODE_HEAT_COOL;
  }
}

void ControlData::set_fan_mode(ClimateFanMode mode) {
  switch (mode) {
    case ClimateFanMode::CLIMATE_FAN_LOW:
      this->set_fan_mode_(FAN_LOW);
      break;
    case ClimateFanMode::CLIMATE_FAN_MEDIUM:
      this->set_fan_mode_(FAN_MEDIUM);
      break;
    case ClimateFanMode::CLIMATE_FAN_HIGH:
      this->set_fan_mode_(FAN_HIGH);
      break;
    default:
      this->set_fan_mode_(FAN_AUTO);
      break;
  }
}

ClimateFanMode ControlData::get_fan_mode() const {
  switch (this->get_fan_mode_()) {
    case FAN_LOW:
      return ClimateFanMode::CLIMATE_FAN_LOW;
    case FAN_MEDIUM:
      return ClimateFanMode::CLIMATE_FAN_MEDIUM;
    case FAN_HIGH:
      return ClimateFanMode::CLIMATE_FAN_HIGH;
    default:
      return ClimateFanMode::CLIMATE_FAN_AUTO;
  }
}

void PDPioneerIR::control(const climate::ClimateCall &call) {
  // swing and preset resets after unit powered off
  if (call.get_mode() == climate::CLIMATE_MODE_OFF) {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->preset = climate::CLIMATE_PRESET_NONE;
  } else if (call.get_swing_mode().has_value() && ((*call.get_swing_mode() == climate::CLIMATE_SWING_OFF &&
                                                    this->swing_mode == climate::CLIMATE_SWING_VERTICAL) ||
                                                   (*call.get_swing_mode() == climate::CLIMATE_SWING_VERTICAL &&
                                                    this->swing_mode == climate::CLIMATE_SWING_OFF))) {
    this->swing_ = true;
  } else if (call.get_preset().has_value() &&
             ((*call.get_preset() == climate::CLIMATE_PRESET_NONE && this->preset == climate::CLIMATE_PRESET_BOOST) ||
              (*call.get_preset() == climate::CLIMATE_PRESET_BOOST && this->preset == climate::CLIMATE_PRESET_NONE))) {
    this->boost_ = true;
  }
  climate_ir::ClimateIR::control(call);
}

void PDPioneerIR::transmit_(PDPioneerData &data) {
  data.finalize_odd();
  auto transmit = this->transmitter_->transmit();
  remote_base::PDPioneerProtocol().encode(transmit.get_data(), data);
  transmit.perform();
}

void PDPioneerIR::transmit_state() {
  if (this->swing_) {
    SpecialData data(SpecialData::VSWING_TOGGLE);
    this->transmit_(data);
    this->swing_ = false;
    return;
  }
  if (this->boost_) {
    SpecialData data(SpecialData::TURBO_TOGGLE);
    this->transmit_(data);
    this->boost_ = false;
    return;
  }
  ControlData data;
  data.set_fahrenheit(this->fahrenheit_);
  data.set_temp(this->target_temperature);
  data.set_mode(this->mode);
  data.set_fan_mode(this->fan_mode.value_or(ClimateFanMode::CLIMATE_FAN_AUTO));
  data.set_sleep_preset(this->preset == climate::CLIMATE_PRESET_SLEEP);
  data.fix();
  this->transmit_(data);
}

bool PDPioneerIR::on_receive(remote_base::RemoteReceiveData data) {
  ESP_LOGD(TAG, "PDPioneerIR::on_receive");
  auto pdpioneer = remote_base::PDPioneerProtocol().decode(data);
  ESP_LOGD(TAG, "PDPioneerIR::on_receive - decode returned, has_value=%d", pdpioneer.has_value());
  if (pdpioneer.has_value()) {
    ESP_LOGI(TAG, "PDPioneerIR::on_receive - decode successful, calling on_pdpioneer_");
    bool result = this->on_pdpioneer_(*pdpioneer);
    ESP_LOGI(TAG, "PDPioneerIR::on_receive - on_pdpioneer_ returned %d", result);
    return result;
  } else {
    ESP_LOGD(TAG, "PDPioneerIR::on_receive - decode failed, no data");
  }
  return false;
}

bool PDPioneerIR::on_pdpioneer_(const PDPioneerData &data) {
  ESP_LOGI(TAG, "on_pdpioneer_ called with data: %s", data.to_string().c_str());
  uint8_t data_type = data.type();
  ESP_LOGI(TAG, "Data type: 0x%02X (expected: CONTROL=0x%02X, SPECIAL=0x%02X, FOLLOW_ME=0x%02X)", data_type,
           PDPioneerData::PDPIONEER_TYPE_CONTROL, PDPioneerData::PDPIONEER_TYPE_SPECIAL,
           PDPioneerData::PDPIONEER_TYPE_FOLLOW_ME);

  if (data_type == PDPioneerData::PDPIONEER_TYPE_CONTROL) {
    ESP_LOGI(TAG, "Processing CONTROL type");
    const ControlData status = data;
    if (status.get_mode() != climate::CLIMATE_MODE_FAN_ONLY)
      this->target_temperature = status.get_temp();
    this->mode = status.get_mode();
    this->fan_mode = status.get_fan_mode();
    if (status.get_sleep_preset()) {
      this->preset = climate::CLIMATE_PRESET_SLEEP;
    } else if (this->preset == climate::CLIMATE_PRESET_SLEEP) {
      this->preset = climate::CLIMATE_PRESET_NONE;
    }
    this->publish_state();
    return true;
  }

  if (data_type == PDPioneerData::PDPIONEER_TYPE_SPECIAL) {
    ESP_LOGI(TAG, "Processing SPECIAL type, command byte: 0x%02X", data[1]);
    switch (data[1]) {
      case SpecialData::VSWING_TOGGLE:
        this->swing_mode = this->swing_mode == climate::CLIMATE_SWING_VERTICAL ? climate::CLIMATE_SWING_OFF
                                                                               : climate::CLIMATE_SWING_VERTICAL;
        break;
      case SpecialData::TURBO_TOGGLE:
        this->preset = this->preset == climate::CLIMATE_PRESET_BOOST ? climate::CLIMATE_PRESET_NONE
                                                                     : climate::CLIMATE_PRESET_BOOST;
        break;
      default:
        ESP_LOGW(TAG, "Unknown SPECIAL command: 0x%02X", data[1]);
        return false;
    }
    this->publish_state();
    return true;
  }

  if (data_type == PDPioneerData::PDPIONEER_TYPE_FOLLOW_ME) {
    ESP_LOGI(TAG, "Processing FOLLOW_ME type");
    // Handle follow-me data if needed
    return true;
  }

  if (data_type == PDPioneerData::PDPIONEER_TYPE_ODD || data_type == PDPioneerData::PDPIONEER_TYPE_EVEN) {
    ESP_LOGI(TAG, "Processing %s type (decoded from remote)",
             data_type == PDPioneerData::PDPIONEER_TYPE_ODD ? "ODD" : "EVEN");
    // Treat ODD/EVEN as CONTROL data for climate control
    const ControlData status = data;
    if (status.get_mode() != climate::CLIMATE_MODE_FAN_ONLY)
      this->target_temperature = status.get_temp();
    this->mode = status.get_mode();
    this->fan_mode = status.get_fan_mode();
    if (status.get_sleep_preset()) {
      this->preset = climate::CLIMATE_PRESET_SLEEP;
    } else if (this->preset == climate::CLIMATE_PRESET_SLEEP) {
      this->preset = climate::CLIMATE_PRESET_NONE;
    }
    this->publish_state();
    return true;
  }

  ESP_LOGW(TAG, "Unknown data type: 0x%02X, not processing", data_type);
  return false;
}

}  // namespace pd_pioneer_ir
}  // namespace esphome
