import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CONF_CS_PIN = "cs_pin"
CONF_RETENTION_DAYS = "retention_days"
CONF_FLUSH_INTERVAL = "flush_interval"

sd_logger_ns = cg.esphome_ns.namespace("sd_logger")
SdLogger = sd_logger_ns.class_("SdLogger", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SdLogger),
        cv.Required(CONF_CS_PIN): cv.int_range(min=0, max=48),
        cv.Optional(CONF_RETENTION_DAYS, default=30): cv.int_range(min=1, max=365),
        cv.Optional(CONF_FLUSH_INTERVAL, default="60s"): cv.positive_time_period_seconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_cs_pin(config[CONF_CS_PIN]))
    cg.add(var.set_retention_days(config[CONF_RETENTION_DAYS]))
    cg.add(var.set_flush_interval(config[CONF_FLUSH_INTERVAL].total_milliseconds))
