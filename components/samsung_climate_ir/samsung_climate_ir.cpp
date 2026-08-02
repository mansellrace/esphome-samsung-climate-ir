#include "samsung_climate_ir.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cmath>
#include <cstring>
#include <vector>

namespace esphome::samsung_climate_ir {

static const char *const TAG = "samsung_climate_ir";

/// The transmitter is wired onto the receiver's own line, so we hear ourselves.
static const uint32_t RECEIVE_MUTE_MS = 500;

void SamsungClimateIR::setup() {
  // Turbo has no standard fan mode, so it is always exposed as a custom one.
  if (this->fan_mode_names_[SAMSUNG_FAN_TURBO] == nullptr)
    this->fan_mode_names_[SAMSUNG_FAN_TURBO] = "Turbo";

  std::vector<const char *> custom_fan_modes;
  for (uint8_t i = 0; i < SAMSUNG_FAN_SPEED_COUNT; i++) {
    if (this->fan_mode_names_[i] != nullptr)
      custom_fan_modes.push_back(this->fan_mode_names_[i]);
  }
  this->set_supported_custom_fan_modes(custom_fan_modes);

  std::memcpy(this->state_.raw, RESET_STATE, STANDARD_LENGTH);

  // Restores mode, temperature, fan and preset; needs the custom fan modes above
  // to already be declared, because they are restored by index.
  climate_ir::ClimateIR::setup();

  this->update_boost_timeout_();

  if (this->transmit_on_boot_) {
    this->transmit_state();
    this->publish_state();
  }
}

void SamsungClimateIR::dump_config() {
  climate_ir::ClimateIR::dump_config();
  ESP_LOGCONFIG(TAG,
                "  Transmit on boot: %s\n"
                "  Fast timeout: %" PRIu32 " s",
                YESNO(this->transmit_on_boot_), this->boost_timeout_ / 1000);
  for (uint8_t i = 0; i < SAMSUNG_FAN_SPEED_COUNT; i++) {
    if (this->fan_mode_names_[i] != nullptr)
      ESP_LOGCONFIG(TAG, "  Fan speed %u named '%s'", i, this->fan_mode_names_[i]);
  }
}

void SamsungClimateIR::control(const climate::ClimateCall &call) {
  const bool touches_settings = call.get_mode().has_value() || call.get_target_temperature().has_value() ||
                                call.get_fan_mode().has_value() || call.has_custom_fan_mode() ||
                                call.get_swing_mode().has_value();

  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();
  if (call.get_fan_mode().has_value()) {
    this->set_fan_mode_(*call.get_fan_mode());
  } else if (call.has_custom_fan_mode()) {
    // ClimateIR::control() does not know about custom fan modes, so handle them here.
    this->set_custom_fan_mode_(call.get_custom_fan_mode());
  }
  if (call.get_swing_mode().has_value())
    this->swing_mode = *call.get_swing_mode();

  if (call.get_preset().has_value()) {
    this->preset = *call.get_preset();
  } else if (touches_settings) {
    // Any other command cancels Fast, like the template switch used to do.
    this->preset = climate::CLIMATE_PRESET_NONE;
  }

  // The remote only offers Fast in cool and heat.
  if (this->boost_active_() && this->mode != climate::CLIMATE_MODE_COOL && this->mode != climate::CLIMATE_MODE_HEAT) {
    ESP_LOGD(TAG, "Fast is only available in cool and heat, ignoring it");
    this->preset = climate::CLIMATE_PRESET_NONE;
  }
  // Dry and auto always drive the fan automatically.
  if (this->mode == climate::CLIMATE_MODE_DRY || this->mode == climate::CLIMATE_MODE_HEAT_COOL)
    this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);

  this->update_boost_timeout_();
  this->transmit_state();
  this->publish_state();
}

void SamsungClimateIR::build_state_() {
  const bool power_on = this->mode != climate::CLIMATE_MODE_OFF;

  this->state_.f.power1 = power_on ? POWER_ON : POWER_OFF;
  this->state_.f.power2 = power_on ? POWER_ON : POWER_OFF;
  this->state_.f.beep_toggle = 0;

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      this->state_.f.mode = MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      this->state_.f.mode = MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_DRY:
      this->state_.f.mode = MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      this->state_.f.mode = MODE_FAN;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      this->state_.f.mode = MODE_AUTO;
      break;
    default:
      // Off keeps whatever mode was last selected, exactly like the remote.
      break;
  }

  const float target = clamp(this->target_temperature, static_cast<float>(MIN_TEMP), static_cast<float>(MAX_TEMP));
  this->state_.f.temp = static_cast<uint8_t>(lroundf(target)) - MIN_TEMP;

  this->state_.f.swing = this->swing_mode == climate::CLIMATE_SWING_VERTICAL ? SWING_VERTICAL : SWING_OFF;

  // Fan speed, Quiet and Powerful are mutually exclusive on this protocol.
  this->state_.f.quiet = 0;
  this->state_.f.fan_special = FAN_SPECIAL_OFF;

  if (this->boost_active_()) {
    // Powerful pins the fan to turbo and rules Quiet out.
    this->state_.f.fan_special = FAN_SPECIAL_POWERFUL;
    this->state_.f.fan = FAN_TURBO;
  } else {
    switch (this->current_fan_speed_()) {
      case SAMSUNG_FAN_QUIET:
        this->state_.f.quiet = 1;
        this->state_.f.fan = FAN_AUTO;
        break;
      case SAMSUNG_FAN_LOW:
        this->state_.f.fan = FAN_LOW;
        break;
      case SAMSUNG_FAN_MEDIUM:
        this->state_.f.fan = FAN_MED;
        break;
      case SAMSUNG_FAN_HIGH:
        this->state_.f.fan = FAN_HIGH;
        break;
      case SAMSUNG_FAN_TURBO:
        this->state_.f.fan = FAN_TURBO;
        break;
      default:
        this->state_.f.fan = FAN_AUTO;
        break;
    }
  }

  // Auto mode has a fan value reserved for it, and it wins over every other one.
  if (this->state_.f.mode == MODE_AUTO)
    this->state_.f.fan = FAN_AUTO2;
}

void SamsungClimateIR::transmit_state() {
  const bool power_on = this->mode != climate::CLIMATE_MODE_OFF;
  this->build_state_();

  if (!power_on) {
    // Switching off uses a fixed, already checksummed frame of its own.
    SamsungProtocol off_frame{};
    std::memcpy(off_frame.raw, POWER_OFF_FRAME, EXTENDED_LENGTH);
    this->last_sent_power_state_ = false;
    this->force_extended_ = false;
    this->transmit_frame_(off_frame, EXTENDED_LENGTH, /* recalculate_checksum= */ false);
    return;
  }

  const bool extended = this->force_extended_ || power_on != this->last_sent_power_state_;
  this->last_sent_power_state_ = power_on;
  this->force_extended_ = false;

  if (!extended) {
    this->transmit_frame_(this->state_, STANDARD_LENGTH);
    return;
  }

  // On a power transition section 2 moves to section 3 and a fixed middle
  // section takes its place. `state_` itself stays a standard frame.
  SamsungProtocol frame = this->state_;
  std::memcpy(frame.raw + 2 * SECTION_LENGTH, this->state_.raw + SECTION_LENGTH, SECTION_LENGTH);
  std::memcpy(frame.raw + SECTION_LENGTH, EXTENDED_MIDDLE_SECTION, SECTION_LENGTH);
  this->transmit_frame_(frame, EXTENDED_LENGTH);
}

void SamsungClimateIR::toggle_beep() {
  this->build_state_();
  SamsungProtocol frame = this->state_;
  frame.f.beep_toggle = 1;
  ESP_LOGD(TAG, "Toggling the unit's beep setting");
  this->transmit_frame_(frame, STANDARD_LENGTH);
}

void SamsungClimateIR::transmit_frame_(SamsungProtocol frame, uint8_t length, bool recalculate_checksum) {
  if (recalculate_checksum)
    update_checksums(frame);

  ESP_LOGD(TAG, "Sending %u byte frame: %s", length, format_hex_pretty(frame.raw, length).c_str());

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(CARRIER_FREQUENCY);
  // Header, then per section: section item, 56 bits and the trailing item.
  data->reserve(2 + (length / SECTION_LENGTH) * (2 + 2 * 8 * SECTION_LENGTH + 2));

  data->item(HEADER_MARK, HEADER_SPACE);
  for (uint8_t offset = 0; offset < length; offset += SECTION_LENGTH) {
    data->item(SECTION_MARK, SECTION_SPACE);
    for (uint8_t pos = 0; pos < SECTION_LENGTH; pos++) {
      const uint8_t byte = frame.raw[offset + pos];
      for (uint8_t mask = 1; mask != 0; mask <<= 1)  // least significant bit first
        data->item(BIT_MARK, (byte & mask) != 0 ? ONE_SPACE : ZERO_SPACE);
    }
    data->item(BIT_MARK, SECTION_GAP);
  }

  transmit.perform();
  this->last_transmit_ = millis();
}

bool SamsungClimateIR::on_receive(remote_base::RemoteReceiveData data) {
  // Transmitter and receiver share the same wire, so skip our own frames.
  if (millis() - this->last_transmit_ < RECEIVE_MUTE_MS)
    return false;

  if (!data.expect_item(HEADER_MARK, HEADER_SPACE))
    return false;

  SamsungProtocol frame{};
  uint8_t sections = 0;
  while (sections < EXTENDED_LENGTH / SECTION_LENGTH && data.peek_item(SECTION_MARK, SECTION_SPACE)) {
    data.advance(2);
    for (uint8_t pos = 0; pos < SECTION_LENGTH; pos++) {
      uint8_t byte = 0;
      for (uint8_t bit = 0; bit < 8; bit++) {  // least significant bit first
        if (data.expect_item(BIT_MARK, ONE_SPACE)) {
          byte |= 1 << bit;
        } else if (!data.expect_item(BIT_MARK, ZERO_SPACE)) {
          return false;
        }
      }
      frame.raw[sections * SECTION_LENGTH + pos] = byte;
    }
    if (!data.expect_mark(BIT_MARK))
      return false;
    sections++;
    // The gap is missing on the last section, where the capture simply ends.
    data.expect_space(SECTION_GAP);
  }

  const uint8_t length = sections * SECTION_LENGTH;
  if (length != STANDARD_LENGTH && length != EXTENDED_LENGTH)
    return false;
  if (!valid_checksum(frame.raw, length)) {
    ESP_LOGW(TAG, "Dropping %u byte frame with a bad checksum: %s", length,
             format_hex_pretty(frame.raw, length).c_str());
    return false;
  }
  ESP_LOGD(TAG, "Received %u byte frame: %s", length, format_hex_pretty(frame.raw, length).c_str());

  const bool extended = length == EXTENDED_LENGTH;
  const bool power_on =
      extended ? frame.f.power3 == POWER_ON : frame.f.power1 == POWER_ON && frame.f.power2 == POWER_ON;

  // Keep the frame we build the next command on as a standard message.
  this->state_ = frame;
  if (extended)
    std::memcpy(this->state_.raw + SECTION_LENGTH, frame.raw + 2 * SECTION_LENGTH, SECTION_LENGTH);
  this->last_sent_power_state_ = power_on;
  this->force_extended_ = false;

  if (!power_on) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->preset = climate::CLIMATE_PRESET_NONE;
    this->update_boost_timeout_();
    this->publish_state();
    return true;
  }

  const uint8_t mode = extended ? frame.f.mode3 : frame.f.mode;
  const uint8_t fan = extended ? frame.f.fan3 : frame.f.fan;
  const uint8_t fan_special = extended ? frame.f.fan_special3 : frame.f.fan_special;
  const uint8_t swing = extended ? frame.f.swing3 : frame.f.swing;
  const uint8_t temp = extended ? frame.f.temp3 : frame.f.temp;

  switch (mode) {
    case MODE_COOL:
      this->mode = climate::CLIMATE_MODE_COOL;
      break;
    case MODE_DRY:
      this->mode = climate::CLIMATE_MODE_DRY;
      break;
    case MODE_FAN:
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      break;
    case MODE_HEAT:
      this->mode = climate::CLIMATE_MODE_HEAT;
      break;
    case MODE_AUTO:
      this->mode = climate::CLIMATE_MODE_HEAT_COOL;
      break;
    default:
      break;
  }

  const bool powerful = fan_special == FAN_SPECIAL_POWERFUL && fan == FAN_TURBO;
  this->preset = powerful ? climate::CLIMATE_PRESET_BOOST : climate::CLIMATE_PRESET_NONE;

  if (frame.f.quiet != 0) {
    this->apply_fan_speed_(SAMSUNG_FAN_QUIET);
  } else {
    switch (fan) {
      case FAN_LOW:
        this->apply_fan_speed_(SAMSUNG_FAN_LOW);
        break;
      case FAN_MED:
        this->apply_fan_speed_(SAMSUNG_FAN_MEDIUM);
        break;
      case FAN_HIGH:
        this->apply_fan_speed_(SAMSUNG_FAN_HIGH);
        break;
      case FAN_TURBO:
        this->apply_fan_speed_(SAMSUNG_FAN_TURBO);
        break;
      default:
        this->apply_fan_speed_(SAMSUNG_FAN_AUTO);
        break;
    }
  }

  this->swing_mode = (swing == SWING_VERTICAL || swing == SWING_BOTH) ? climate::CLIMATE_SWING_VERTICAL
                                                                     : climate::CLIMATE_SWING_OFF;
  this->target_temperature = temp + MIN_TEMP;

  this->update_boost_timeout_();
  this->publish_state();
  return true;
}

SamsungFanSpeed SamsungClimateIR::current_fan_speed_() const {
  if (this->has_custom_fan_mode()) {
    const StringRef name = this->get_custom_fan_mode();
    for (uint8_t i = 0; i < SAMSUNG_FAN_SPEED_COUNT; i++) {
      if (this->fan_mode_names_[i] != nullptr && strcmp(name.c_str(), this->fan_mode_names_[i]) == 0)
        return static_cast<SamsungFanSpeed>(i);
    }
    return SAMSUNG_FAN_AUTO;
  }

  if (this->fan_mode.has_value()) {
    switch (*this->fan_mode) {
      case climate::CLIMATE_FAN_QUIET:
        return SAMSUNG_FAN_QUIET;
      case climate::CLIMATE_FAN_LOW:
        return SAMSUNG_FAN_LOW;
      case climate::CLIMATE_FAN_MEDIUM:
        return SAMSUNG_FAN_MEDIUM;
      case climate::CLIMATE_FAN_HIGH:
        return SAMSUNG_FAN_HIGH;
      default:
        break;
    }
  }
  return SAMSUNG_FAN_AUTO;
}

void SamsungClimateIR::apply_fan_speed_(SamsungFanSpeed speed) {
  if (speed != SAMSUNG_FAN_AUTO && this->fan_mode_names_[speed] != nullptr) {
    this->set_custom_fan_mode_(this->fan_mode_names_[speed]);
    return;
  }
  switch (speed) {
    case SAMSUNG_FAN_QUIET:
      this->set_fan_mode_(climate::CLIMATE_FAN_QUIET);
      break;
    case SAMSUNG_FAN_LOW:
      this->set_fan_mode_(climate::CLIMATE_FAN_LOW);
      break;
    case SAMSUNG_FAN_MEDIUM:
      this->set_fan_mode_(climate::CLIMATE_FAN_MEDIUM);
      break;
    case SAMSUNG_FAN_HIGH:
      this->set_fan_mode_(climate::CLIMATE_FAN_HIGH);
      break;
    default:
      this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);
      break;
  }
}

void SamsungClimateIR::update_boost_timeout_() {
  this->cancel_timeout("boost");
  if (this->boost_timeout_ == 0 || !this->boost_active_())
    return;

  this->set_timeout("boost", this->boost_timeout_, [this]() {
    // The unit drops out of Fast on its own, so there is nothing to transmit.
    ESP_LOGD(TAG, "Fast expired");
    this->preset = climate::CLIMATE_PRESET_NONE;
    this->state_.f.fan_special = FAN_SPECIAL_OFF;
    this->publish_state();
  });
}

}  // namespace esphome::samsung_climate_ir
