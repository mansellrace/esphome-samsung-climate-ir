#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/core/hal.h"

#include "samsung_protocol.h"

namespace esphome::samsung_climate_ir {

/** The fan speeds the Samsung remote can select, other than automatic.
 *
 * Each one is exposed either as a standard ESPHome fan mode or, when the user
 * gives it a label through `fan_mode_names`, as a custom fan mode. TURBO has no
 * standard equivalent, so it is always a custom fan mode ("Turbo" by default).
 */
enum SamsungFanSpeed : uint8_t {
  SAMSUNG_FAN_QUIET = 0,
  SAMSUNG_FAN_LOW,
  SAMSUNG_FAN_MEDIUM,
  SAMSUNG_FAN_HIGH,
  SAMSUNG_FAN_TURBO,
  SAMSUNG_FAN_SPEED_COUNT,
  /// Automatic: always a standard fan mode, never renameable.
  SAMSUNG_FAN_AUTO = SAMSUNG_FAN_SPEED_COUNT,
};

class SamsungClimateIR : public climate_ir::ClimateIR {
 public:
  SamsungClimateIR()
      : climate_ir::ClimateIR(MIN_TEMP, MAX_TEMP, 1.0f, /* supports_dry= */ true, /* supports_fan_only= */ true,
                              {climate::CLIMATE_FAN_AUTO},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL},
                              {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_BOOST}) {}

  void setup() override;
  void dump_config() override;

  void set_fan_mode_name(SamsungFanSpeed speed, const char *name) { this->fan_mode_names_[speed] = name; }
  void add_std_fan_mode(climate::ClimateFanMode mode) { this->fan_modes_.insert(mode); }
  void set_transmit_on_boot(bool transmit_on_boot) { this->transmit_on_boot_ = transmit_on_boot; }
  void set_boost_timeout(uint32_t boost_timeout) { this->boost_timeout_ = boost_timeout; }

  /// Send the current state once with the beep toggle bit set, flipping the unit's beep setting.
  void toggle_beep();

 protected:
  void control(const climate::ClimateCall &call) override;
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  /// Write the current entity state into `state_` as a standard 14 byte frame.
  void build_state_();
  /// Checksum `frame`, encode it and hand it to the transmitter.
  void transmit_frame_(SamsungProtocol frame, uint8_t length, bool recalculate_checksum = true);
  /// The Samsung fan speed currently selected on the entity.
  SamsungFanSpeed current_fan_speed_() const;
  /// Publish `speed` on the entity, as a custom fan mode when it has been renamed.
  void apply_fan_speed_(SamsungFanSpeed speed);
  /// Arm or cancel the Fast auto-off timer.
  void update_boost_timeout_();

  bool boost_active_() const {
    return this->preset.has_value() && *this->preset == climate::CLIMATE_PRESET_BOOST;
  }

  SamsungProtocol state_{};
  const char *fan_mode_names_[SAMSUNG_FAN_SPEED_COUNT]{};
  uint32_t boost_timeout_{1800000};
  uint32_t last_transmit_{0};
  bool transmit_on_boot_{false};
  bool last_sent_power_state_{false};
  /// The very first frame after boot is always extended, like IRremoteESP8266 does.
  bool force_extended_{true};
};

}  // namespace esphome::samsung_climate_ir
