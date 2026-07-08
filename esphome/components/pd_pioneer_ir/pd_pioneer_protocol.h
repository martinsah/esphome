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
  static const uint8_t FRAME_SIZE = 14;
  static const uint8_t DATA_LEN = 13;
  static const uint8_t OFFSET_CS = 13;

  PDPioneerData() { this->data_.fill(0); }
  PDPioneerData(std::initializer_list<uint8_t> data) {
    std::copy_n(data.begin(), std::min(data.size(), this->data_.size()), this->data_.begin());
  }
  PDPioneerData(const std::vector<uint8_t> &data) {
    std::copy_n(data.begin(), std::min(data.size(), this->data_.size()), this->data_.begin());
  }

  uint8_t *data() { return this->data_.data(); }
  const uint8_t *data() const { return this->data_.data(); }
  uint8_t size() const { return this->data_.size(); }

  bool is_odd_burst() const { return this->data_[3] == 0x02; }
  bool is_even_burst() const { return this->data_[3] == 0x01; }

  bool is_valid() const {
    if (this->is_odd_burst())
      return this->data_[OFFSET_CS] == static_cast<uint8_t>(this->calc_cs_() + 15);
    if (this->is_even_burst())
      return this->data_[OFFSET_CS] == this->calc_cs_();
    return false;
  }

  void finalize() {
    if (this->is_odd_burst()) {
      this->data_[OFFSET_CS] = this->calc_cs_() + 15;
    } else {
      this->data_[OFFSET_CS] = this->calc_cs_();
    }
  }

  std::string to_string() const { return format_hex_pretty(this->data_.data(), this->data_.size()); }

  uint8_t &operator[](size_t idx) { return this->data_[idx]; }
  const uint8_t &operator[](size_t idx) const { return this->data_[idx]; }

 protected:
  uint8_t calc_cs_() const {
    uint8_t cs = 0;
    for (uint8_t idx = 0; idx < OFFSET_CS; idx++)
      cs += this->data_[idx];
    return cs;
  }

  std::array<uint8_t, FRAME_SIZE> data_{};
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
    data.finalize();
    PDPioneerProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
