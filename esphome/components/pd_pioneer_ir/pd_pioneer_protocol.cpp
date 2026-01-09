#include "pd_pioneer_protocol.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.pd_pioneer";

static const int32_t TICK_US = 560;
static const int32_t HEADER_MARK_US = 3064;
static const int32_t HEADER_SPACE_US = 1670;
static const int32_t BIT_MARK_US = 408;
static const int32_t BIT_ONE_SPACE_US = 1131;
static const int32_t BIT_ZERO_SPACE_US = 355;
static const int32_t FOOTER_MARK_US = 408;
static const int32_t FOOTER_SPACE_US = 9988;

uint8_t PDPioneerData::calc_cs_() const {
  uint8_t cs = 0;
  for (uint8_t idx = 0; idx < OFFSET_CS; idx++)
    cs += this->data_[idx];
  return cs;
}

void PDPioneerProtocol::encode(RemoteTransmitData *dst, const PDPioneerData &src) {
  dst->set_carrier_frequency(38000);
  dst->reserve(2 + 48 * 2 + 2 + 2 + 48 * 2 + 1);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  for (unsigned idx = 0; idx < 6; idx++) {
    for (uint8_t mask = 1 << 7; mask; mask >>= 1)
      dst->item(BIT_MARK_US, (src[idx] & mask) ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
  }
  dst->item(FOOTER_MARK_US, FOOTER_SPACE_US);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  for (unsigned idx = 0; idx < 6; idx++) {
    for (uint8_t mask = 1 << 7; mask; mask >>= 1)
      dst->item(BIT_MARK_US, (src[idx] & mask) ? BIT_ZERO_SPACE_US : BIT_ONE_SPACE_US);
  }
  dst->mark(FOOTER_MARK_US);
}

static bool decode_data(RemoteReceiveData &src, PDPioneerData &dst) {
  for (unsigned idx = 0; idx < 14; idx++) {
    uint8_t data = 0;
    for (uint8_t mask = 1; mask; mask <<= 1) {
      int32_t mark = src.peek(0);
      src.advance(1);
      int32_t space = src.peek(0);
      src.advance(1);
      if (mark < -space)
        data |= mask;
    }
    dst[idx] = data;
  }
  return true;
}

optional<PDPioneerData> PDPioneerProtocol::decode(RemoteReceiveData src) {
  PDPioneerData out;

  // Check header mark and space
  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US)) {
    return {};
  }

  // Decode data
  if (!decode_data(src, out)) {
    return {};
  }

  // Check footer mark
  if (!src.peek_mark_at_least(FOOTER_MARK_US)) {
    ESP_LOGI(TAG, "RX PDPioneer decode footer mark error");
    return {};
  }
  src.advance(1);

  // Check footer space
  if (!src.peek_space_at_most(-FOOTER_SPACE_US)) {
    ESP_LOGI(TAG, "RX PDPioneer decode footer space error");
    return {};
  }
  src.advance(1);

  // Validate and return
  if (out.is_valid_odd()) {
    out.set_type(PDPioneerData::PDPIONEER_TYPE_ODD);
    ESP_LOGI(TAG, "RX PDPioneer (Odd): %s", out.to_string().c_str());
    return out;
  }

  if (out.is_valid_even()) {
    out.set_type(PDPioneerData::PDPIONEER_TYPE_EVEN);
    ESP_LOGI(TAG, "RX PDPioneer (Even): %s", out.to_string().c_str());
    return out;
  }

  return {};
}

void PDPioneerProtocol::dump(const PDPioneerData &data) {
  ESP_LOGI(TAG, "Received PDPioneer: %s", data.to_string().c_str());
}

}  // namespace remote_base
}  // namespace esphome
