#include "dx4600_leds.h"

int dx4600_leds_t::start(const char *dev_path, uint8_t dev_addr) {
    return _i2c.start(dev_path, dev_addr);
}

static int compute_checksum(const std::vector<uint8_t>& data, int size) {
    if (size < 2 || size > (int)data.size()) 
        return 0;

    int sum = 0;
    for (int i = 0; i < size; ++i)
        sum += (int)data[i];

    return sum;
}

static bool verify_checksum(const std::vector<uint8_t>& data) {
    int size = data.size();
    if (size < 2) return false;
    int sum = compute_checksum(data, size - 2);
    return sum != 0 && sum == (data[size - 1] | (((int)data[size - 2]) << 8));
}

static void append_checksum(std::vector<uint8_t>& data) {
    int size = data.size();
    int sum = compute_checksum(data, size);
    data.push_back((sum >> 8) & 0xff);
    data.push_back(sum & 0xff);
}

dx4600_leds_t::led_data_t dx4600_leds_t::get_status(led_type_t id) {
    led_data_t data { };

    auto raw_data = _i2c.read_block_data(0x81 + (uint8_t)id, 0xb);
    if (raw_data.size() != 0xb || !verify_checksum(raw_data)) 
        return data;

    switch (raw_data[0]) {
        case 0: data.op_mode = op_mode_t::off; break;
        case 1: data.op_mode = op_mode_t::on; break;
        case 2: data.op_mode = op_mode_t::blink; break;
        case 3: data.op_mode = op_mode_t::breath; break;
        default: return data;
    };


    data.brightness = raw_data[1];
    data.color_r = raw_data[2];
    data.color_g = raw_data[3];
    data.color_b = raw_data[4];
    int t_hight = (((int)raw_data[5]) << 8) | raw_data[6];
    int t_low = (((int)raw_data[7]) << 8) | raw_data[8];
    data.t_on = t_low;
    data.t_off = t_hight - t_low;

    return data;
}

int dx4600_leds_t::_change_status(led_type_t id, uint8_t command, std::array<std::optional<uint8_t>, 4> params) {
    std::vector<uint8_t> data {
    //   3c    3b    3a
        0x00, 0xa0, 0x01,
    //     39        38         37
        0x00, 0x00, command, 
    //     36 - 33
        params[0].value_or(0x00), 
        params[1].value_or(0x00), 
        params[2].value_or(0x00), 
        params[3].value_or(0x00), 
    };

    append_checksum(data);
    data[0] = (uint8_t)id;
    return _i2c.write_block_data((uint8_t)id, data);
}

int dx4600_leds_t::set_onoff(led_type_t id, uint8_t status) {
    if (status >= 2) return -1;
    return _change_status(id, 0x03, { status } );
}

int dx4600_leds_t::set_blink(led_type_t id, uint16_t t_on, uint16_t t_off) {
    uint16_t t_hight = t_on + t_off;
    uint16_t t_low = t_on;
    return _change_status(id, 0x04, { 
        (uint8_t)(t_hight >> 8), 
        (uint8_t)(t_hight & 0xff), 
        (uint8_t)(t_low >> 8),
        (uint8_t)(t_low & 0xff),
    } );
}

int dx4600_leds_t::set_rgb(led_type_t id, uint8_t r, uint8_t g, uint8_t b) {
    return _change_status(id, 0x02, { r, g, b } );
}

int dx4600_leds_t::set_brightness(led_type_t id, uint8_t brightness) {
    return _change_status(id, 0x01, { brightness } );
}