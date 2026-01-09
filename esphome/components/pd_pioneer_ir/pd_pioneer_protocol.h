#pragma once

#include <array>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace remote_base {

class PDPioneerData {
 public:
  // Make default
  PDPioneerData() {}
  // Make from initializer_list
  PDPioneerData(std::initializer_list<uint8_t> data) {
    std::copy_n(data.begin(), std::min(data.size(), this->data_.size()), this->data_.begin());
  }
  // Make from vector
  PDPioneerData(const std::vector<uint8_t> &data) {
    std::copy_n(data.begin(), std::min(data.size(), this->data_.size()), this->data_.begin());
  }

  uint8_t *data() { return this->data_.data(); }
  const uint8_t *data() const { return this->data_.data(); }
  uint8_t size() const { return this->data_.size(); }
  bool is_valid_odd() const { return this->data_[OFFSET_CS] == this->calc_cs_(); }
  bool is_valid_even() const { return this->data_[OFFSET_CS] == this->calc_cs_() + 15; }
  void finalize_odd() { this->data_[OFFSET_CS] = this->calc_cs_(); }
  void finalize_even() { this->data_[OFFSET_CS] = this->calc_cs_() + 15; }

  std::string to_string() const { return format_hex_pretty(this->data_.data(), this->data_.size()); }
  // compare only 40-bits
  bool operator==(const PDPioneerData &rhs) const {
    return std::equal(this->data_.begin(), this->data_.begin() + OFFSET_CS, rhs.data_.begin());
  }
  enum PDPioneerDataType : uint8_t {
    PDPIONEER_TYPE_CONTROL = 0xA1,
    PDPIONEER_TYPE_SPECIAL = 0xA2,
    PDPIONEER_TYPE_FOLLOW_ME = 0xA4,
    PDPIONEER_TYPE_ODD = 0x23,
    PDPIONEER_TYPE_EVEN = 0x24,
  };
  PDPioneerDataType type() const { return static_cast<PDPioneerDataType>(this->data_[0]); }
  void set_type(PDPioneerDataType type) { this->data_[0] = static_cast<uint8_t>(type); }
  template<typename T> T to() const { return T(*this); }
  uint8_t &operator[](size_t idx) { return this->data_[idx]; }
  const uint8_t &operator[](size_t idx) const { return this->data_[idx]; }

 protected:
  uint8_t get_value_(uint8_t idx, uint8_t mask = 255, uint8_t shift = 0) const {
    return (this->data_[idx] >> shift) & mask;
  }
  void set_value_(uint8_t idx, uint8_t value, uint8_t mask = 255, uint8_t shift = 0) {
    this->data_[idx] &= ~(mask << shift);
    this->data_[idx] |= (value << shift);
  }
  float get_temperature_celsius() { return this->get_value_(4) - 1; }
  void set_mask_(uint8_t idx, bool state, uint8_t mask = 255) { this->set_value_(idx, state ? mask : 0, mask); }
  static const uint8_t OFFSET_CS = 13;
  // 104-bits data + 8 bits checksum
  std::array<uint8_t, 14> data_;
  // Calculate checksum
  uint8_t calc_cs_() const;
};

class PDPioneerProtocol : public RemoteProtocol<PDPioneerData> {
 public:
  void encode(RemoteTransmitData *dst, const PDPioneerData &src) override;
  optional<PDPioneerData> decode(RemoteReceiveData src) override;
  void dump(const PDPioneerData &data) override;
};

DECLARE_REMOTE_PROTOCOL(PDPioneer)

template<typename... Ts> class PDPioneerAction : public RemoteTransmitterActionBase<Ts...> {
  TEMPLATABLE_VALUE(std::vector<uint8_t>, code)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    PDPioneerData data(this->code_.value(x...));
    data.finalize_odd();
    PDPioneerProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
