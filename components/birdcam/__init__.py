"""LILYGO T-Camera S3 V1.6: PMU and volatile event JPEG.

Target: ESPHome 2026.9.1 / ESP-IDF. No custom_component mechanism.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32, esp32_camera, i2c, socket
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32_camera", "i2c", "esp32"]
ns = cg.esphome_ns.namespace("birdcam")
Birdcam = ns.class_("Birdcam", cg.Component, i2c.I2CDevice)
def consume_sockets(config):
    socket.consume_sockets(2, "birdcam")(config)
    socket.consume_sockets(1, "birdcam", socket.SocketType.TCP_LISTEN)(config)
    socket.consume_sockets(1, "birdcam", socket.SocketType.UDP)(config)
    return config

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(Birdcam),
    cv.Required("camera_id"): cv.use_id(esp32_camera.ESP32Camera),
    cv.Optional("photo_port", default=8082): cv.All(cv.port, cv.Range(min=1, max=55535)),
}).extend(i2c.i2c_device_schema(0x34)).extend(cv.COMPONENT_SCHEMA), cv.only_on_esp32, consume_sockets)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cam = await cg.get_variable(config["camera_id"])
    cg.add(var.set_camera(cam))
    cg.add(var.set_photo_port(config["photo_port"]))
    esp32.include_builtin_idf_component("esp_http_server")
