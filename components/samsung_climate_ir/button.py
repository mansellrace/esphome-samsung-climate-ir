import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import CONF_SAMSUNG_CLIMATE_IR_ID, SamsungClimateIR, samsung_climate_ir_ns

AUTO_LOAD = ["climate_ir"]

SamsungBeepButton = samsung_climate_ir_ns.class_(
    "SamsungBeepButton", button.Button, cg.Parented.template(SamsungClimateIR)
)

CONFIG_SCHEMA = button.button_schema(
    SamsungBeepButton,
    icon="mdi:volume-off",
    entity_category=ENTITY_CATEGORY_CONFIG,
).extend(
    {
        cv.GenerateID(CONF_SAMSUNG_CLIMATE_IR_ID): cv.use_id(SamsungClimateIR),
    }
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_parented(var, config[CONF_SAMSUNG_CLIMATE_IR_ID])
