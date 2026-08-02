#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BUTTON

#include "esphome/components/button/button.h"
#include "esphome/core/helpers.h"

#include "samsung_climate_ir.h"

namespace esphome::samsung_climate_ir {

/** Flips the unit's beep setting.
 *
 * The protocol only carries a toggle, never the resulting state, so this is a
 * button and not a switch: we cannot know whether the unit currently beeps.
 */
class SamsungBeepButton final : public button::Button, public Parented<SamsungClimateIR> {
 protected:
  void press_action() override { this->parent_->toggle_beep(); }
};

}  // namespace esphome::samsung_climate_ir

#endif  // USE_BUTTON
