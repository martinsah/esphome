#include "pd_pioneer_ir.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pd_pioneer_ir {

static const char *const TAG = "pd_pioneer_ir.climate";

void PDPioneerIR::setup() {
  climate_ir::ClimateIR::setup();
  this->publish_state();
}

void PDPioneerIR::control(const climate::ClimateCall &call) {
  if (call.get_mode() == climate::CLIMATE_MODE_OFF) {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->preset = climate::CLIMATE_PRESET_NONE;
  } else if (call.get_swing_mode().has_value()) {
    const auto swing = *call.get_swing_mode();
    if ((swing == climate::CLIMATE_SWING_OFF && this->swing_mode == climate::CLIMATE_SWING_VERTICAL) ||
        (swing == climate::CLIMATE_SWING_VERTICAL && this->swing_mode == climate::CLIMATE_SWING_OFF)) {
      this->swing_ = true;
    }
  } else if (call.get_preset().has_value()) {
    const auto preset = *call.get_preset();
    if ((preset == climate::CLIMATE_PRESET_NONE && this->preset == climate::CLIMATE_PRESET_ECO) ||
        (preset == climate::CLIMATE_PRESET_ECO && this->preset == climate::CLIMATE_PRESET_NONE)) {
      this->eco_ = true;
    }
  }
  climate_ir::ClimateIR::control(call);
}

void PDPioneerIR::transmit_pair_(const ControlData &data) {
  ControlData frame = data;
  frame.finalize();

  ESP_LOGI(TAG, "TX odd:  %s", frame.odd().to_string().c_str());
  ESP_LOGI(TAG, "TX even: %s", frame.even().to_string().c_str());

  auto transmit = this->transmitter_->transmit();
  remote_base::PDPioneerProtocol protocol;
  protocol.encode(transmit.get_data(), frame.odd());
  protocol.encode(transmit.get_data(), frame.even());
  transmit.perform();
}

void PDPioneerIR::transmit_state() {
  ControlData data;
  data.set_mode(this->mode);

  if (this->swing_) {
    const bool enable = this->swing_mode != climate::CLIMATE_SWING_VERTICAL;
    if (this->mode != climate::CLIMATE_MODE_OFF && this->mode != climate::CLIMATE_MODE_FAN_ONLY)
      data.set_temp(this->target_temperature, this->fahrenheit_);
    data.set_fan_mode(this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO));
    data.set_swing_vertical(enable);
    data.set_eco(this->preset == climate::CLIMATE_PRESET_ECO);
    this->swing_mode = enable ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
    this->swing_ = false;
    this->transmit_pair_(data);
    this->publish_state();
    return;
  }

  if (this->eco_) {
    const bool enable = this->preset != climate::CLIMATE_PRESET_ECO;
    if (this->mode != climate::CLIMATE_MODE_OFF && this->mode != climate::CLIMATE_MODE_FAN_ONLY)
      data.set_temp(this->target_temperature, this->fahrenheit_);
    data.set_fan_mode(this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO));
    data.set_swing_vertical(this->swing_mode == climate::CLIMATE_SWING_VERTICAL);
    data.set_eco(enable);
    this->preset = enable ? climate::CLIMATE_PRESET_ECO : climate::CLIMATE_PRESET_NONE;
    this->eco_ = false;
    this->transmit_pair_(data);
    this->publish_state();
    return;
  }

  if (this->mode != climate::CLIMATE_MODE_FAN_ONLY)
    data.set_temp(this->target_temperature, this->fahrenheit_);
  data.set_fan_mode(this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO));
  data.set_swing_vertical(this->swing_mode == climate::CLIMATE_SWING_VERTICAL);
  data.set_eco(this->preset == climate::CLIMATE_PRESET_ECO);

  this->transmit_pair_(data);
}

bool PDPioneerIR::apply_frame_(const remote_base::PDPioneerData &frame) {
  if (frame.is_odd_burst()) {
    this->rx_state_.apply_odd(frame);
    this->rx_odd_pending_ = true;
    return false;
  }

  if (frame.is_even_burst()) {
    this->rx_state_.apply_even(frame);
    if (!this->rx_odd_pending_) {
      ESP_LOGD(TAG, "even burst without preceding odd burst");
    }
    this->rx_odd_pending_ = false;

    if (this->rx_state_.get_mode() != climate::CLIMATE_MODE_FAN_ONLY)
      this->target_temperature = this->rx_state_.get_temp(this->fahrenheit_);
    this->mode = this->rx_state_.get_mode();
    this->fan_mode = this->rx_state_.get_fan_mode();
    this->swing_mode =
        this->rx_state_.get_swing_vertical() ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
    this->preset = this->rx_state_.get_eco() ? climate::CLIMATE_PRESET_ECO : climate::CLIMATE_PRESET_NONE;
    this->publish_state();
    return true;
  }

  return false;
}

bool PDPioneerIR::on_receive(remote_base::RemoteReceiveData data) {
  auto decoded = remote_base::PDPioneerProtocol().decode(data);
  if (!decoded.has_value())
    return false;
  return this->apply_frame_(*decoded);
}

}  // namespace pd_pioneer_ir
}  // namespace esphome
