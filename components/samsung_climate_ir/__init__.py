import esphome.codegen as cg
from esphome.components import climate, climate_ir

CODEOWNERS = ["@mansellrace"]
AUTO_LOAD = ["climate_ir"]

samsung_climate_ir_ns = cg.esphome_ns.namespace("samsung_climate_ir")
SamsungClimateIR = samsung_climate_ir_ns.class_("SamsungClimateIR", climate_ir.ClimateIR)
SamsungFanSpeed = samsung_climate_ir_ns.enum("SamsungFanSpeed")

CONF_SAMSUNG_CLIMATE_IR_ID = "samsung_climate_ir_id"

# Renameable fan speeds: YAML key -> (C++ enumerator, matching standard fan mode).
# Turbo has no standard equivalent, so it is always exposed as a custom fan mode.
FAN_SPEEDS = {
    "quiet": (
        SamsungFanSpeed.SAMSUNG_FAN_QUIET,
        climate.ClimateFanMode.CLIMATE_FAN_QUIET,
    ),
    "low": (
        SamsungFanSpeed.SAMSUNG_FAN_LOW,
        climate.ClimateFanMode.CLIMATE_FAN_LOW,
    ),
    "medium": (
        SamsungFanSpeed.SAMSUNG_FAN_MEDIUM,
        climate.ClimateFanMode.CLIMATE_FAN_MEDIUM,
    ),
    "high": (
        SamsungFanSpeed.SAMSUNG_FAN_HIGH,
        climate.ClimateFanMode.CLIMATE_FAN_HIGH,
    ),
    "turbo": (SamsungFanSpeed.SAMSUNG_FAN_TURBO, None),
}
