#pragma once

// Samsung A/C infrared protocol: state layout, timings and checksum.
//
// The bit layout, the timing constants and the checksum algorithm are a port of
// the Samsung A/C support in IRremoteESP8266 (src/ir_Samsung.{h,cpp}) by
// David Conran and contributors, licensed LGPL-2.1.
// See https://github.com/crankyoldgit/IRremoteESP8266
//
// This header is deliberately free of any ESPHome dependency: it only describes
// the wire format.

#include <cstdint>

namespace esphome::samsung_climate_ir {

// Frame timings, in microseconds. These are the exact values IRremoteESP8266
// transmits; do not round them.
static constexpr uint32_t HEADER_MARK = 690;
static constexpr uint32_t HEADER_SPACE = 17844;
static constexpr uint32_t SECTION_MARK = 3086;
static constexpr uint32_t SECTION_SPACE = 8864;
static constexpr uint32_t SECTION_GAP = 2886;
static constexpr uint32_t BIT_MARK = 586;
static constexpr uint32_t ONE_SPACE = 1432;
static constexpr uint32_t ZERO_SPACE = 436;
static constexpr uint32_t CARRIER_FREQUENCY = 38000;

// A frame is made of 7-byte sections: 2 sections for a standard message,
// 3 for the extended one used on power transitions.
static constexpr uint8_t SECTION_LENGTH = 7;
static constexpr uint8_t STANDARD_LENGTH = 14;
static constexpr uint8_t EXTENDED_LENGTH = 21;

static constexpr uint8_t MIN_TEMP = 16;
static constexpr uint8_t MAX_TEMP = 30;

// _.mode
static constexpr uint8_t MODE_AUTO = 0;
static constexpr uint8_t MODE_COOL = 1;
static constexpr uint8_t MODE_DRY = 2;
static constexpr uint8_t MODE_FAN = 3;
static constexpr uint8_t MODE_HEAT = 4;

// _.fan
static constexpr uint8_t FAN_AUTO = 0;
static constexpr uint8_t FAN_LOW = 2;
static constexpr uint8_t FAN_MED = 4;
static constexpr uint8_t FAN_HIGH = 5;
static constexpr uint8_t FAN_AUTO2 = 6;  // only valid while mode is AUTO
static constexpr uint8_t FAN_TURBO = 7;

// _.swing
static constexpr uint8_t SWING_VERTICAL = 0b010;
static constexpr uint8_t SWING_HORIZONTAL = 0b011;
static constexpr uint8_t SWING_BOTH = 0b100;
static constexpr uint8_t SWING_OFF = 0b111;

// _.fan_special
static constexpr uint8_t FAN_SPECIAL_OFF = 0b000;
static constexpr uint8_t FAN_SPECIAL_POWERFUL = 0b011;
static constexpr uint8_t FAN_SPECIAL_BREEZE = 0b101;
static constexpr uint8_t FAN_SPECIAL_ECONO = 0b111;

// _.power1 / _.power2 / _.power3
static constexpr uint8_t POWER_ON = 0b11;
static constexpr uint8_t POWER_OFF = 0b00;

/// Blank state the remote starts from, and the base for every standard frame.
static constexpr uint8_t RESET_STATE[STANDARD_LENGTH] = {0x02, 0x92, 0x0F, 0x00, 0x00, 0x00, 0xF0,
                                                         0x01, 0x02, 0xAE, 0x71, 0x00, 0x15, 0xF0};

/// Fixed middle section inserted when converting a standard frame to an extended one.
static constexpr uint8_t EXTENDED_MIDDLE_SECTION[SECTION_LENGTH] = {0x01, 0xD2, 0x0F, 0x00, 0x00, 0x00, 0x00};

/// Verbatim "turn off" frame, as sent by IRremoteESP8266's IRSamsungAc::sendOff().
static constexpr uint8_t POWER_OFF_FRAME[EXTENDED_LENGTH] = {0x02, 0xB2, 0x0F, 0x00, 0x00, 0x00, 0xC0,
                                                             0x01, 0xD2, 0x0F, 0x00, 0x00, 0x00, 0x00,
                                                             0x01, 0x02, 0xFF, 0x71, 0x80, 0x11, 0xC0};

/** The 21 byte state, addressable either as raw bytes or as named bit fields.
 *
 * Bytes 0..13 are the standard message. In an extended message bytes 7..13 are
 * replaced by EXTENDED_MIDDLE_SECTION and the original section 2 is moved to
 * bytes 14..20, so the `*3` fields below mirror their section 2 counterparts.
 * Timer fields of the extended middle section are not modelled: the component
 * never sets a timer, and that section is always sent verbatim.
 */
union SamsungProtocol {
  uint8_t raw[EXTENDED_LENGTH];
  struct {
    // Byte 0
    uint8_t : 8;
    // Byte 1
    uint8_t : 4;
    uint8_t sum1_lower : 4;
    // Byte 2
    uint8_t sum1_upper : 4;
    uint8_t : 4;
    // Byte 3
    uint8_t : 8;
    // Byte 4
    uint8_t : 8;
    // Byte 5
    uint8_t : 4;
    uint8_t sleep : 1;
    uint8_t quiet : 1;
    uint8_t : 2;
    // Byte 6
    uint8_t : 4;
    uint8_t power1 : 2;
    uint8_t : 2;
    // Byte 7
    uint8_t : 8;
    // Byte 8
    uint8_t : 4;
    uint8_t sum2_lower : 4;
    // Byte 9
    uint8_t sum2_upper : 4;
    uint8_t swing : 3;
    uint8_t : 1;
    // Byte 10
    uint8_t : 1;
    uint8_t fan_special : 3;
    uint8_t display : 1;
    uint8_t : 2;
    uint8_t clean_toggle10 : 1;
    // Byte 11
    uint8_t ion : 1;
    uint8_t clean_toggle11 : 1;
    uint8_t : 2;
    uint8_t temp : 4;
    // Byte 12
    uint8_t : 1;
    uint8_t fan : 3;
    uint8_t mode : 3;
    uint8_t : 1;
    // Byte 13
    uint8_t : 2;
    uint8_t beep_toggle : 1;
    uint8_t : 1;
    uint8_t power2 : 2;
    uint8_t : 2;
    // Byte 14
    uint8_t : 8;
    // Byte 15
    uint8_t : 4;
    uint8_t sum3_lower : 4;
    // Byte 16
    uint8_t sum3_upper : 4;
    uint8_t swing3 : 3;
    uint8_t : 1;
    // Byte 17
    uint8_t : 1;
    uint8_t fan_special3 : 3;
    uint8_t : 4;
    // Byte 18
    uint8_t : 4;
    uint8_t temp3 : 4;
    // Byte 19
    uint8_t : 1;
    uint8_t fan3 : 3;
    uint8_t mode3 : 3;
    uint8_t : 1;
    // Byte 20
    uint8_t : 2;
    uint8_t beep_toggle3 : 1;
    uint8_t : 1;
    uint8_t power3 : 2;
    uint8_t : 2;
  } f;
};

static_assert(sizeof(SamsungProtocol) == EXTENDED_LENGTH, "SamsungProtocol must be packed into 21 bytes");

/// Number of bits set in a byte.
inline uint8_t count_bits(uint8_t data) {
  uint8_t count = 0;
  for (uint8_t remainder = data; remainder != 0; remainder >>= 1) {
    if ((remainder & 1) != 0)
      count++;
  }
  return count;
}

/// Number of bits set across `length` bytes starting at `start`.
inline uint16_t count_bits(const uint8_t *start, uint16_t length) {
  uint16_t count = 0;
  for (uint16_t offset = 0; offset < length; offset++)
    count += count_bits(start[offset]);
  return count;
}

/** Checksum of one 7-byte section.
 *
 * Counts the bits set in the whole first byte, the low nibble of the second,
 * the high nibble of the third and the following four bytes, then inverts it.
 * The two checksum nibbles themselves are excluded, which is why the second and
 * third bytes are only half counted.
 */
inline uint8_t calc_section_checksum(const uint8_t *section) {
  uint16_t sum = 0;
  sum += count_bits(section[0]);
  sum += count_bits(static_cast<uint8_t>(section[1] & 0x0F));
  sum += count_bits(static_cast<uint8_t>((section[2] >> 4) & 0x0F));
  sum += count_bits(section + 3, 4);
  return static_cast<uint8_t>(sum) ^ 0xFF;
}

/// Checksum currently stored in a 7-byte section (low nibble of byte 1, high of byte 2).
inline uint8_t get_section_checksum(const uint8_t *section) {
  return static_cast<uint8_t>(((section[2] & 0x0F) << 4) | ((section[1] >> 4) & 0x0F));
}

/// True when every section of a `length` byte frame carries a correct checksum.
inline bool valid_checksum(const uint8_t *state, uint16_t length) {
  if (length < SECTION_LENGTH)
    return false;
  const uint16_t max_length = length > EXTENDED_LENGTH ? EXTENDED_LENGTH : length;
  for (uint16_t offset = 0; offset + SECTION_LENGTH <= max_length; offset += SECTION_LENGTH) {
    if (get_section_checksum(state + offset) != calc_section_checksum(state + offset))
      return false;
  }
  return true;
}

/// Recompute and store the checksum of all three sections.
inline void update_checksums(SamsungProtocol &state) {
  uint8_t sum = calc_section_checksum(state.raw);
  state.f.sum1_lower = sum & 0x0F;
  state.f.sum1_upper = (sum >> 4) & 0x0F;
  sum = calc_section_checksum(state.raw + SECTION_LENGTH);
  state.f.sum2_lower = sum & 0x0F;
  state.f.sum2_upper = (sum >> 4) & 0x0F;
  sum = calc_section_checksum(state.raw + 2 * SECTION_LENGTH);
  state.f.sum3_lower = sum & 0x0F;
  state.f.sum3_upper = (sum >> 4) & 0x0F;
}

}  // namespace esphome::samsung_climate_ir
