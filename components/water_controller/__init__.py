import esphome.codegen as cg
from esphome.components import sensor, output
import esphome.config_validation as cv
from esphome.const import CONF_ID

CONF_WATER_LEVEL_SENSOR = "water_level_sensor"
CONF_PUMP_OUTPUT = "pump_output"
CONF_TARGET_LEVEL = "target_level"
CONF_CRITICAL_LOW_LEVEL = "critical_low_level"
CONF_FILL_TIMEOUT = "fill_timeout"
CONF_COOLDOWN_DURATION = "cooldown_duration"

water_controller_ns = cg.esphome_ns.namespace("water_controller")
WaterController = water_controller_ns.class_("WaterController", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WaterController),
        cv.Required(CONF_WATER_LEVEL_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_PUMP_OUTPUT): cv.use_id(output.BinaryOutput),
        cv.Optional(CONF_TARGET_LEVEL, default=80.0): cv.float_range(min=0, max=100),
        cv.Optional(CONF_CRITICAL_LOW_LEVEL, default=15.0): cv.float_range(min=0, max=100),
        cv.Optional(CONF_FILL_TIMEOUT, default="120s"): cv.positive_time_period_seconds,
        cv.Optional(CONF_COOLDOWN_DURATION, default="300s"): cv.positive_time_period_seconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    level_sensor = await cg.get_variable(config[CONF_WATER_LEVEL_SENSOR])
    cg.add(var.set_water_level_sensor(level_sensor))

    pump = await cg.get_variable(config[CONF_PUMP_OUTPUT])
    cg.add(var.set_pump_output(pump))

    cg.add(var.set_target_level(config[CONF_TARGET_LEVEL]))
    cg.add(var.set_critical_low_level(config[CONF_CRITICAL_LOW_LEVEL]))
    cg.add(var.set_fill_timeout(config[CONF_FILL_TIMEOUT].total_milliseconds))
    cg.add(var.set_cooldown_duration(config[CONF_COOLDOWN_DURATION].total_milliseconds))
