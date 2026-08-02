import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv

from . import FAN_SPEEDS, SamsungClimateIR

AUTO_LOAD = ["climate_ir"]

CONF_FAN_MODE_NAMES = "fan_mode_names"
CONF_TRANSMIT_ON_BOOT = "transmit_on_boot"
CONF_BOOST_TIMEOUT = "boost_timeout"

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(SamsungClimateIR).extend(
    {
        # Give a speed a label here and it becomes a custom fan mode with that
        # name instead of the standard ESPHome one.
        cv.Optional(CONF_FAN_MODE_NAMES): cv.Schema(
            {cv.Optional(key): cv.string_strict for key in FAN_SPEEDS}
        ),
        cv.Optional(CONF_TRANSMIT_ON_BOOT, default=False): cv.boolean,
        # The unit drops out of Fast by itself; 0s keeps the preset until changed.
        cv.Optional(
            CONF_BOOST_TIMEOUT, default="30min"
        ): cv.positive_time_period_milliseconds,
    }
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)

    names = config.get(CONF_FAN_MODE_NAMES, {})
    for key, (speed, standard_mode) in FAN_SPEEDS.items():
        if key in names:
            cg.add(var.set_fan_mode_name(speed, names[key]))
        elif standard_mode is not None:
            cg.add(var.add_std_fan_mode(standard_mode))

    cg.add(var.set_transmit_on_boot(config[CONF_TRANSMIT_ON_BOOT]))
    cg.add(var.set_boost_timeout(config[CONF_BOOST_TIMEOUT]))
