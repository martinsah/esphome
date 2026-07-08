#include "pd_pioneer_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.pd_pioneer";

static const int32_t HEADER_MARK_US = 3143;
static const int32_t HEADER_SPACE_US = 1591;
static const int32_t BIT_PULSE_US = 513;
static const int32_t BIT_ONE_SPACE_US = 1065;
static const int32_t BIT_ZERO_SPACE_US = 302;
static const int32_t FOOTER_MARK_US = 513;
static const int32_t FOOTER_SPACE_US = 10126;

static void encode_bit_(RemoteTransmitData *dst, bool bit) {
  dst->item(BIT_PULSE_US, bit ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
}

static bool decode_bit_(RemoteReceiveData &src, bool *bit) {
  if (!src.is_valid(1))
    return false;
  int32_t mark = src.peek(0);
  int32_t space = src.peek(1);
  src.advance(2);
  *bit = mark < -space;
  return true;
}

void PDPioneerProtocol::encode(RemoteTransmitData *dst, const PDPioneerData &src) {
  ESP_LOGD(TAG, "encode %s", src.to_string().c_str());
  dst->set_carrier_frequency(38000);
  // header + start bit + 14 bytes + stop bit + footer
  dst->reserve(2 + 2 + PDPioneerData::DATA_LEN * 8 * 2 + 2 + 2);

  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  encode_bit_(dst, true);  // start bit

  for (uint8_t idx = 0; idx < PDPioneerData::DATA_LEN; idx++) {
    for (uint8_t mask = 1; mask; mask <<= 1)
      encode_bit_(dst, (src[idx] & mask) != 0);
  }

  encode_bit_(dst, true);  // stop bit
  dst->item(FOOTER_MARK_US, FOOTER_SPACE_US);
}

static bool decode_frame_(RemoteReceiveData &src, PDPioneerData &dst) {
  bool bit;

  if (!decode_bit_(src, &bit) || !bit)
    return false;

  for (uint8_t idx = 0; idx < PDPioneerData::DATA_LEN; idx++) {
    uint8_t data = 0;
    for (uint8_t mask = 1; mask; mask <<= 1) {
      if (!decode_bit_(src, &bit))
        return false;
      if (bit)
        data |= mask;
    }
    dst[idx] = data;
  }

  if (!decode_bit_(src, &bit) || !bit)
    return false;

  return true;
}

optional<PDPioneerData> PDPioneerProtocol::decode(RemoteReceiveData src) {
  PDPioneerData out;

  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US))
    return {};

  if (!decode_frame_(src, out))
    return {};

  if (!src.peek_mark_at_least(FOOTER_MARK_US))
    return {};
  src.advance(1);

  if (!src.peek_space_at_most(-FOOTER_SPACE_US))
    return {};
  src.advance(1);

  if (!out.is_valid()) {
    ESP_LOGD(TAG, "checksum fail %s", out.to_string().c_str());
    return {};
  }

  ESP_LOGI(TAG, "RX %s burst: %s", out.is_odd_burst() ? "odd" : "even", out.to_string().c_str());
  return out;
}

void PDPioneerProtocol::dump(const PDPioneerData &data) {
  ESP_LOGI(TAG, "Received PD-Pioneer: %s", data.to_string().c_str());
}

}  // namespace remote_base
}  // namespace esphome
