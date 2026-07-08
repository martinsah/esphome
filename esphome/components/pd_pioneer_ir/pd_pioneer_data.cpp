#include "pd_pioneer_data.h"
#include "esphome/core/helpers.h"
#include <cmath>

namespace esphome {
namespace pd_pioneer_ir {

static const uint8_t DEFAULT_ODD[] = {0x23, 0xCB, 0x26, 0x02, 0x00, 0x40, 0xC0,
                                      0x00, 0xC3, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t DEFAULT_EVEN[] = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x24, 0x03,
                                       0x0B, 0x05, 0x00, 0x00, 0x00, 0x80, 0x00};

ControlData::ControlData() {
  std::copy_n(DEFAULT_ODD, PDPioneerData::FRAME_SIZE, this->odd_.data());
  std::copy_n(DEFAULT_EVEN, PDPioneerData::FRAME_SIZE, this->even_.data());
}

void ControlData::finalize() {
  this->odd_.finalize();
  this->even_.finalize();
}

void ControlData::set_power_(bool on) {
  this->powered_ = on;
  this->even_[5] = on ? PWR_ON : PWR_OFF;
}

bool ControlData::get_power_() const { return this->even_[5] != PWR_OFF; }

void ControlData::set_temp(float temp_c, bool fahrenheit) {
  (void) fahrenheit;
  float temp_f = celsius_to_fahrenheit(temp_c);
  temp_f = clamp(temp_f, static_cast<float>(PDPIONEER_TEMPF_MIN), static_cast<float>(PDPIONEER_TEMPF_MAX));

  int temp_whole = static_cast<int>(std::floor(temp_f + 0.001f));
  this->even_[7] = static_cast<uint8_t>(0x07 + (76 - temp_whole) / 2);
  this->even_[12] = static_cast<uint8_t>(0x80 | ((temp_whole % 2 == 0) ? 0x04 : 0x00));
}

float ControlData::get_temp(bool fahrenheit) const {
  (void) fahrenheit;
  if (!this->get_power_())
    return fahrenheit_to_celsius(72.0f);

  int pair_top = 76 - 2 * (static_cast<int>(this->even_[7]) - 0x07);
  float temp_f = static_cast<float>((this->even_[12] & 0x04) ? pair_top : pair_top - 1);
  return fahrenheit_to_celsius(temp_f);
}

void ControlData::set_mode(ClimateMode mode) {
  switch (mode) {
    case ClimateMode::CLIMATE_MODE_OFF:
      this->set_power_(false);
      return;
    case ClimateMode::CLIMATE_MODE_COOL:
      this->even_[6] = MODE_COOL;
      break;
    case ClimateMode::CLIMATE_MODE_DRY:
      this->even_[6] = MODE_DRY;
      break;
    case ClimateMode::CLIMATE_MODE_FAN_ONLY:
      this->even_[6] = MODE_FAN_ONLY;
      break;
    case ClimateMode::CLIMATE_MODE_HEAT:
      this->even_[6] = MODE_HEAT;
      break;
    default:
      this->even_[6] = MODE_AUTO;
      break;
  }
  this->set_power_(true);
}

ClimateMode ControlData::get_mode() const {
  if (!this->get_power_())
    return ClimateMode::CLIMATE_MODE_OFF;
  switch (this->even_[6]) {
    case MODE_COOL:
      return ClimateMode::CLIMATE_MODE_COOL;
    case MODE_DRY:
      return ClimateMode::CLIMATE_MODE_DRY;
    case MODE_FAN_ONLY:
      return ClimateMode::CLIMATE_MODE_FAN_ONLY;
    case MODE_HEAT:
      return ClimateMode::CLIMATE_MODE_HEAT;
    case MODE_AUTO:
      return ClimateMode::CLIMATE_MODE_HEAT_COOL;
    default:
      return ClimateMode::CLIMATE_MODE_COOL;
  }
}

void ControlData::set_fan_from_odd_(uint8_t byte5, uint8_t byte6) {
  this->odd_[5] = byte5;
  this->odd_[6] = byte6;
}

void ControlData::set_fan_from_even_(uint8_t byte8) { this->even_[8] = byte8; }

void ControlData::sync_even_fan_byte_() {
  const uint8_t b6 = this->odd_[6];
  if (b6 == 0x20) {
    this->even_[8] = 0x00;
  } else if (b6 <= 0x40) {
    this->even_[8] = 0x02;
  } else if (b6 <= 0x80) {
    this->even_[8] = 0x03;
  } else {
    this->even_[8] = 0x05;
  }
}

void ControlData::set_fan_mode(ClimateFanMode mode) {
  switch (mode) {
    case ClimateFanMode::CLIMATE_FAN_LOW:
      this->set_fan_from_odd_(0x60, 0x40);
      break;
    case ClimateFanMode::CLIMATE_FAN_MEDIUM:
      this->set_fan_from_odd_(0x40, 0x60);
      break;
    case ClimateFanMode::CLIMATE_FAN_HIGH:
      this->set_fan_from_odd_(0x40, 0xC0);
      break;
    default:
      this->set_fan_from_odd_(0x40, 0xC0);
      break;
  }
  this->sync_even_fan_byte_();
}

ClimateFanMode ControlData::get_fan_mode() const {
  const uint8_t b6 = this->odd_[6];
  if (b6 == 0x20)
    return ClimateFanMode::CLIMATE_FAN_AUTO;
  if (b6 <= 0x40)
    return ClimateFanMode::CLIMATE_FAN_LOW;
  if (b6 <= 0x80)
    return ClimateFanMode::CLIMATE_FAN_MEDIUM;
  return ClimateFanMode::CLIMATE_FAN_HIGH;
}

void ControlData::set_swing_vertical(bool enabled) {
  if (enabled) {
    this->odd_[7] = 0x08;
  } else if (this->odd_[7] == 0x08) {
    this->odd_[7] = 0x00;
  }
}

bool ControlData::get_swing_vertical() const { return this->odd_[7] == 0x08; }

void ControlData::set_eco(bool enabled) {
  if (enabled) {
    this->even_[5] = 0x25;
  } else if (this->even_[5] == 0x25) {
    this->even_[5] = PWR_ON;
  }
}

bool ControlData::get_eco() const { return this->even_[5] == 0x25; }

void ControlData::apply_odd(const PDPioneerData &data) {
  for (uint8_t i = 0; i < PDPioneerData::DATA_LEN; i++)
    this->odd_[i] = data[i];
}

void ControlData::apply_even(const PDPioneerData &data) {
  for (uint8_t i = 0; i < PDPioneerData::DATA_LEN; i++)
    this->even_[i] = data[i];
  this->powered_ = this->get_power_();
}

}  // namespace pd_pioneer_ir
}  // namespace esphome
