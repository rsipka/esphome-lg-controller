#pragma once

#include "esphome.h"
#include "esphome/components/uart/uart.h"

// ESPHome 2026.11 stopped exposing format_hex_pretty through helpers.h.
#if __has_include("esphome/core/alloc_helpers.h")
#include "esphome/core/alloc_helpers.h"
#endif

static const char* const TAG = "lg-controller";

namespace esphome::lg_controller {

static constexpr size_t MIN_TEMP_SETPOINT = 16;
static constexpr size_t MAX_TEMP_SETPOINT = 30;

class LgSwitch final : public switch_::Switch {
    void write_state(bool value) override {
        publish_state(value);
    }

public:
    void restore_and_set_mode(switch_::SwitchRestoreMode mode) {
        set_restore_mode(mode);
        if (auto state = get_initial_state_with_restore_mode()) {
            write_state(*state);
        }
    }
};

class LgSelect final : public select::Select {
    void control(const std::string& value) override {
        if (this->current_option() != value) {
            this->publish_state(value); 
        }
    }
};

class LgNumber final : public number::Number {
    void control(float value) override {
        if (this->state != value) {
            this->publish_state(value); 
        }
    }
};

// The LG protocol always uses Celsius. The HA/ESPHome climate component internally
// converts between Fahrenheit and Celsius. Values from the Home Assistant room temperature sensor
// are not converted automatically so can be Celsius or Fahrenheit.
//
// Unfortunately LG uses their own Fahrenheit/Celsius mapping that's different from what you'd
// expect. For example, 78F is ~25.5C, but LG controllers will send 26C for 78F. A value of 25.5C
// would be interpreted by the AC as 77F.
//
// This class has some functions to convert between Fahrenheit, Celsius and "LG-Celsius" (values
// we send to or receive from the unit). This ensures Home Assistant and the LG unit always agree
// on the setpoint in Fahrenheit.
//
// These conversions are only used in Fahrenheit mode.
class TempConversion {
private:
    static constexpr int8_t FahToLGCel[] = {
        0  /* 32  */, 1  /* 33  */, 2  /* 34  */, 3  /* 35  */, 4  /* 36  */,
        5  /* 37  */, 6  /* 38  */, 7  /* 39  */, 8  /* 40  */, 10 /* 41  */,
        12 /* 42  */, 13 /* 43  */, 14 /* 44  */, 15 /* 45  */, 16 /* 46  */,
        17 /* 47  */, 18 /* 48  */, 19 /* 49  */, 20 /* 50  */, 21 /* 51  */,
        22 /* 52  */, 23 /* 53  */, 24 /* 54  */, 25 /* 55  */, 26 /* 56  */,
        27 /* 57  */, 28 /* 58  */, 30 /* 59  */, 32 /* 60  */, 33 /* 61  */,
        34 /* 62  */, 35 /* 63  */, 36 /* 64  */, 37 /* 65  */, 38 /* 66  */,
        39 /* 67  */, 40 /* 68  */, 41 /* 69  */, 42 /* 70  */, 43 /* 71  */,
        44 /* 72  */, 45 /* 73  */, 46 /* 74  */, 47 /* 75  */, 48 /* 76  */,
        50 /* 77  */, 52 /* 78  */, 53 /* 79  */, 54 /* 80  */, 55 /* 81  */,
        56 /* 82  */, 57 /* 83  */, 58 /* 84  */, 59 /* 85  */, 60 /* 86  */,
        61 /* 87  */, 62 /* 88  */, 63 /* 89  */, 64 /* 90  */, 65 /* 91  */,
        66 /* 92  */, 67 /* 93  */, 68 /* 94  */, 70 /* 95  */, 72 /* 96  */,
        73 /* 97  */, 74 /* 98  */, 75 /* 99  */, 76 /* 100 */, 77 /* 101 */,
        78 /* 102 */, 79 /* 103 */, 80 /* 104 */
    };
    static constexpr int8_t LGCelToCelAdjustment[] = {
         0 /* 0    */,  0 /* 0.5  */,  0 /* 1.0  */,  0 /* 1.5  */,  0 /* 2.0  */,
         1 /* 2.5  */,  1 /* 3.0  */,  1 /* 3.5  */,  1 /* 4.0  */,  0 /* 4.5  */,
         0 /* 5.0  */, -1 /* 5.5  */, -1 /* 6.0  */, -1 /* 6.5  */, -1 /* 7.0  */,
        -1 /* 7.5  */,  0 /* 8.0  */,  0 /* 8.5  */,  0 /* 9.0  */,  0 /* 9.5  */,
         0 /* 10.0 */,  0 /* 10.5 */,  0 /* 11.0 */,  0 /* 11.5 */,  0 /* 12.0 */,
         1 /* 12.5 */,  1 /* 13.0 */,  1 /* 13.5 */,  1 /* 14.0 */,  0 /* 14.5 */,
         0 /* 15.0 */, -1 /* 15.5 */, -1 /* 16.0 */, -1 /* 16.5 */, -1 /* 17.0 */,
        -1 /* 17.5 */,  0 /* 18.0 */,  0 /* 18.5 */,  0 /* 19.0 */,  0 /* 19.5 */,
         0 /* 20.0 */,  0 /* 20.5 */,  0 /* 21.0 */,  0 /* 21.5 */,  0 /* 22.0 */,
         1 /* 22.5 */,  1 /* 23.0 */,  1 /* 23.5 */,  1 /* 24.0 */,  0 /* 24.5 */,
         0 /* 25.0 */, -1 /* 25.5 */, -1 /* 26.0 */, -1 /* 26.5 */, -1 /* 27.0 */,
        -1 /* 27.5 */,  0 /* 28.0 */,  0 /* 28.5 */,  0 /* 29.0 */,  0 /* 29.5 */,
         0 /* 30.0 */,  0 /* 30.5 */,  0 /* 31.0 */,  0 /* 31.5 */,  0 /* 32.0 */,
         1 /* 32.5 */,  1 /* 33.0 */,  1 /* 33.5 */,  1 /* 34.0 */,  0 /* 34.5 */,
         0 /* 35.0 */, -1 /* 35.5 */, -1 /* 36.0 */, -1 /* 36.5 */, -1 /* 37.0 */,
        -1 /* 37.5 */,  0 /* 38.0 */,  0 /* 38.5 */,  0 /* 39.0 */,  0 /* 39.5 */,
         0 /* 40.0 */
    };

public:
    // Convert from Fahrenheit to LG-Celsius (using the LG-compatible conversion).
    static float fahrenheit_to_lgcelsius(float temp) {
        int temp_int = int(round(temp));
        if (temp_int < 32 || temp_int > 104) {
            return esphome::fahrenheit_to_celsius(temp);
        }
        int8_t val = FahToLGCel[temp_int - 32];
        return float(val / 2) + ((val & 1) ? 0.5f : 0.0f);
    }
    // Convert an LG-Celsius value to Celsius. This is done to ensure the LG unit and HA agree
    // on the value in Fahrenheit. For example, the unit sends 78F as 26C (LG Celsius), but HA
    // would convert this to 78.8F => 79F. To work around this, we adjust 26C to 25.5C because
    // this maps to 78F in HA.
    static float lgcelsius_to_celsius(float temp) {
        int index = int(temp * 2);
        if (index < 0 || index >= sizeof(LGCelToCelAdjustment)) {
            return temp;
        }
        int8_t adjustment = LGCelToCelAdjustment[index];
        if (adjustment == -1) {
            return temp - 0.5;
        }
        if (adjustment == 1) {
            return temp + 0.5;
        }
        return temp;
    }
    static float celsius_to_lgcelsius(float temp) {
        float fahrenheit = esphome::celsius_to_fahrenheit(temp);
        return fahrenheit_to_lgcelsius(fahrenheit);
    }
};
constexpr int8_t TempConversion::FahToLGCel[];
constexpr int8_t TempConversion::LGCelToCelAdjustment[];

class LgController final : public climate::Climate, public uart::UARTDevice, public Component {
    static constexpr size_t MsgLen = 13;

    climate::ClimateTraits supported_traits_{};

    InternalGPIOPin& rx_pin_;
    esphome::sensor::Sensor* temperature_sensor_;

    LgSelect& vane_select_1_;
    LgSelect& vane_select_2_;
    LgSelect& vane_select_3_;
    LgSelect& vane_select_4_;
    LgSelect& overheating_select_;

    LgNumber& fan_speed_slow_;
    LgNumber& fan_speed_low_;
    LgNumber& fan_speed_medium_;
    LgNumber& fan_speed_high_;

    LgNumber& sleep_timer_;

    esphome::sensor::Sensor& error_code_;
    esphome::sensor::Sensor& pipe_temp_in_;
    esphome::sensor::Sensor& pipe_temp_mid_;
    esphome::sensor::Sensor& pipe_temp_out_;
    esphome::sensor::Sensor* power_consumption_;
    esphome::sensor::Sensor* current_power_;
    esphome::sensor::Sensor* humidity_;
    esphome::sensor::Sensor* fan_operation_time_;
    esphome::sensor::Sensor* indoor_unit_operation_time_;
    esphome::sensor::Sensor* precise_room_temperature_;
    esphome::binary_sensor::BinarySensor& defrost_;
    esphome::binary_sensor::BinarySensor& preheat_;
    esphome::binary_sensor::BinarySensor& outdoor_;
    esphome::binary_sensor::BinarySensor& auto_dry_active_;
    optional<uint32_t> outdoor_off_since_{};

    LgSwitch& purifier_;
    LgSwitch& internal_thermistor_;
    LgSwitch& auto_dry_;

    uint8_t recv_buf_[MsgLen] = {};
    size_t recv_buf_len_ = 0;
    uint32_t last_recv_millis_ = 0;

    // Last received 0xC8 message.
    uint8_t last_recv_status_[MsgLen] = {};

    // Last received 0xCA message.
    uint8_t last_recv_type_a_settings_[MsgLen] = {};

    // Last received 0xCB message.
    uint8_t last_recv_type_b_settings_[MsgLen] = {};

    uint8_t send_buf_[MsgLen] = {};
    uint32_t last_sent_status_millis_ = 0;
    uint32_t last_sent_recv_type_b_millis_ = 0;

    enum class PendingSendKind : uint8_t { None, Status, TypeA, TypeB, TypeBRequest };
    PendingSendKind pending_send_ = PendingSendKind::None;

    bool pending_status_change_ = false;
    bool pending_type_a_settings_change_ = false;
    bool pending_type_b_settings_change_ = false;
    bool pending_type_b_request_ = false;
    bool overheating_change_pending_ = false;
    bool overheating_change_sent_ = false;
    // Type A field bits: vane positions 0–3, fan speeds 4–7, Auto Dry 8.
    static constexpr uint16_t TYPE_A_AUTO_DRY = 1U << 8;
    uint16_t pending_type_a_fields_ = 0;
    uint16_t sent_type_a_fields_ = 0;

    bool is_initializing_ = true;
    bool slave_status_synced_ = false;
    enum StartupStatusField : uint8_t {
        STARTUP_MODE = 1U << 0,
        STARTUP_TARGET = 1U << 1,
        STARTUP_FAN = 1U << 2,
        STARTUP_SWING = 1U << 3,
        STARTUP_PURIFIER = 1U << 4,
        STARTUP_TIMER = 1U << 5,
    };
    uint8_t startup_status_fields_ = 0;

    uint8_t vane_position_[4] = {0,0,0,0};
    uint8_t fan_speed_[4] = {0,0,0,0};
    uint8_t overheating_ = 0;

    optional<uint32_t> sleep_timer_target_millis_{};
    uint32_t last_reported_sleep_minutes_ = 0;
    bool active_reservation_ = false;
    bool ignore_sleep_timer_callback_ = false;
    bool ignore_switch_callbacks_ = false;

    uint32_t NVS_STORAGE_VERSION = 2843654U; // Change version if the NVSStorage struct changes
    struct NVSStorage {
        uint8_t capabilities_message[13] = {};
    };
    NVSStorage nvs_storage_;

    const bool fahrenheit_;

    // Whether a message came from the HVAC unit, a master controller, or a slave controller.
    enum class MessageSender : uint8_t { Unit, Master, Slave };
    // Set if this controller is configured as slave controller.
    const bool slave_;

    enum class LgCapability {
        FAN_AUTO,
        FAN_SLOW,
        FAN_LOW,
        FAN_LOW_MEDIUM,
        FAN_MEDIUM,
        FAN_MEDIUM_HIGH,
        FAN_HIGH,
        FAN_POWER,
        MODE_HEATING,
        MODE_FAN,
        MODE_AUTO,
        MODE_DEHUMIDIFY,
        VERTICAL_SWING,
        HORIZONTAL_SWING,
    };

    bool parse_capability(LgCapability capability) {
        switch (capability) {
            case LgCapability::FAN_AUTO:
                return (nvs_storage_.capabilities_message[3] & 0x01) != 0;
            case LgCapability::FAN_SLOW:
                return (nvs_storage_.capabilities_message[3] & 0x20) != 0;
            case LgCapability::FAN_LOW:
                return (nvs_storage_.capabilities_message[3] & 0x10) != 0;
            case LgCapability::FAN_LOW_MEDIUM:
                return (nvs_storage_.capabilities_message[6] & 0x08) != 0;
            case LgCapability::FAN_MEDIUM:
                return (nvs_storage_.capabilities_message[3] & 0x08) != 0;
            case LgCapability::FAN_MEDIUM_HIGH:
                return (nvs_storage_.capabilities_message[6] & 0x10) != 0;
            case LgCapability::FAN_HIGH:
                return true;
            case LgCapability::FAN_POWER:
                return (nvs_storage_.capabilities_message[3] & 0x02) != 0;
            case LgCapability::MODE_HEATING:
                return (nvs_storage_.capabilities_message[2] & 0x20) != 0;
            case LgCapability::MODE_FAN:
                return (nvs_storage_.capabilities_message[2] & 0x40) != 0;
            case LgCapability::MODE_AUTO:
                return (nvs_storage_.capabilities_message[2] & 0x08) != 0;
            case LgCapability::MODE_DEHUMIDIFY:
                return (nvs_storage_.capabilities_message[2] & 0x80) != 0;
            case LgCapability::VERTICAL_SWING:
                return (nvs_storage_.capabilities_message[1] & 0x80) != 0;
            case LgCapability::HORIZONTAL_SWING:
                return (nvs_storage_.capabilities_message[1] & 0x40) != 0;
        }
        return false;
    }

    void configure_capabilities() {
        // Default traits
        climate::ClimateModeMask device_modes;
        device_modes.insert(climate::CLIMATE_MODE_OFF);
        device_modes.insert(climate::CLIMATE_MODE_COOL);
        device_modes.insert(climate::CLIMATE_MODE_HEAT);
        device_modes.insert(climate::CLIMATE_MODE_DRY);
        device_modes.insert(climate::CLIMATE_MODE_FAN_ONLY);
        device_modes.insert(climate::CLIMATE_MODE_HEAT_COOL);
        
        climate::ClimateFanModeMask fan_modes;
        fan_modes.insert(climate::CLIMATE_FAN_LOW);
        fan_modes.insert(climate::CLIMATE_FAN_MEDIUM);
        fan_modes.insert(climate::CLIMATE_FAN_HIGH);
        fan_modes.insert(climate::CLIMATE_FAN_AUTO);
        fan_modes.insert(climate::CLIMATE_FAN_FOCUS);
        
        climate::ClimateSwingModeMask swing_modes;
        swing_modes.insert(climate::CLIMATE_SWING_OFF);
        swing_modes.insert(climate::CLIMATE_SWING_BOTH);
        swing_modes.insert(climate::CLIMATE_SWING_VERTICAL);
        swing_modes.insert(climate::CLIMATE_SWING_HORIZONTAL);

        supported_traits_.set_supported_modes(device_modes);
        supported_traits_.set_supported_fan_modes(fan_modes);
        supported_traits_.set_supported_swing_modes(swing_modes);
        supported_traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE |
                                            climate::CLIMATE_SUPPORTS_ACTION);
        supported_traits_.set_visual_min_temperature(MIN_TEMP_SETPOINT);
        supported_traits_.set_visual_max_temperature(MAX_TEMP_SETPOINT);
        supported_traits_.set_visual_current_temperature_step(fahrenheit_ ? 1 : 0.5);
        supported_traits_.set_visual_target_temperature_step(fahrenheit_ ? 1 : 0.5);

        // Only override defaults if the capabilities are known
        if (nvs_storage_.capabilities_message[0] != 0) {
            // Configure the climate traits
            climate::ClimateModeMask override_device_modes;
            override_device_modes.insert(climate::CLIMATE_MODE_OFF);
            override_device_modes.insert(climate::CLIMATE_MODE_COOL);
            if (parse_capability(LgCapability::MODE_HEATING))
                override_device_modes.insert(climate::CLIMATE_MODE_HEAT);
            if (parse_capability(LgCapability::MODE_FAN))
                override_device_modes.insert(climate::CLIMATE_MODE_FAN_ONLY);
            if (parse_capability(LgCapability::MODE_AUTO))
                override_device_modes.insert(climate::CLIMATE_MODE_HEAT_COOL);
            if (parse_capability(LgCapability::MODE_DEHUMIDIFY))
                override_device_modes.insert(climate::CLIMATE_MODE_DRY);
            supported_traits_.set_supported_modes(override_device_modes);

            climate::ClimateFanModeMask override_fan_modes;
            if (parse_capability(LgCapability::FAN_AUTO))
                override_fan_modes.insert(climate::CLIMATE_FAN_AUTO);
            if (parse_capability(LgCapability::FAN_SLOW))
                override_fan_modes.insert(climate::CLIMATE_FAN_QUIET);
            if (parse_capability(LgCapability::FAN_LOW))
                override_fan_modes.insert(climate::CLIMATE_FAN_LOW);
            if (parse_capability(LgCapability::FAN_MEDIUM))
                override_fan_modes.insert(climate::CLIMATE_FAN_MEDIUM);
            if (parse_capability(LgCapability::FAN_HIGH))
                override_fan_modes.insert(climate::CLIMATE_FAN_HIGH);
            if (parse_capability(LgCapability::FAN_POWER))
                override_fan_modes.insert(climate::CLIMATE_FAN_FOCUS);
            supported_traits_.set_supported_fan_modes(override_fan_modes);

            climate::ClimateSwingModeMask override_swing_modes;
            override_swing_modes.insert(climate::CLIMATE_SWING_OFF);
            if (parse_capability(LgCapability::VERTICAL_SWING) && parse_capability(LgCapability::HORIZONTAL_SWING))
                override_swing_modes.insert(climate::CLIMATE_SWING_BOTH);
            if (parse_capability(LgCapability::VERTICAL_SWING))
                override_swing_modes.insert(climate::CLIMATE_SWING_VERTICAL);
            if (parse_capability(LgCapability::HORIZONTAL_SWING))
                override_swing_modes.insert(climate::CLIMATE_SWING_HORIZONTAL);
            supported_traits_.set_supported_swing_modes(override_swing_modes);
        }
    }

public:
    LgController(InternalGPIOPin* rx_pin,
                 sensor::Sensor* temperature_sensor,
                 LgSelect* vane_select_1,
                 LgSelect* vane_select_2,
                 LgSelect* vane_select_3,
                 LgSelect* vane_select_4,
                 LgSelect* overheating_select,
                 LgNumber* fan_speed_slow,
                 LgNumber* fan_speed_low,
                 LgNumber* fan_speed_medium,
                 LgNumber* fan_speed_high,
                 LgNumber* sleep_timer,
                 sensor::Sensor* error_code,
                 sensor::Sensor* pipe_temp_in,
                 sensor::Sensor* pipe_temp_mid,
                 sensor::Sensor* pipe_temp_out,
                 sensor::Sensor* power_consumption,
                 sensor::Sensor* current_power,
                 sensor::Sensor* humidity,
                 sensor::Sensor* fan_operation_time,
                 sensor::Sensor* indoor_unit_operation_time,
                 sensor::Sensor* precise_room_temperature,
                 binary_sensor::BinarySensor* defrost,
                 binary_sensor::BinarySensor* preheat,
                 binary_sensor::BinarySensor* outdoor,
                 binary_sensor::BinarySensor* auto_dry_active,
                 LgSwitch* purifier,
                 LgSwitch* internal_thermistor,
                 LgSwitch* auto_dry,
                 bool fahrenheit, bool is_slave_controller)
      : rx_pin_(*rx_pin),
        temperature_sensor_(temperature_sensor),
        vane_select_1_(*vane_select_1),
        vane_select_2_(*vane_select_2),
        vane_select_3_(*vane_select_3),
        vane_select_4_(*vane_select_4),
        overheating_select_(*overheating_select),
        fan_speed_slow_(*fan_speed_slow),
        fan_speed_low_(*fan_speed_low),
        fan_speed_medium_(*fan_speed_medium),
        fan_speed_high_(*fan_speed_high),
        sleep_timer_(*sleep_timer),
        error_code_(*error_code),
        pipe_temp_in_(*pipe_temp_in),
        pipe_temp_mid_(*pipe_temp_mid),
        pipe_temp_out_(*pipe_temp_out),
        power_consumption_(power_consumption),
        current_power_(current_power),
        humidity_(humidity),
        fan_operation_time_(fan_operation_time),
        indoor_unit_operation_time_(indoor_unit_operation_time),
        precise_room_temperature_(precise_room_temperature),
        defrost_(*defrost),
        preheat_(*preheat),
        outdoor_(*outdoor),
        auto_dry_active_(*auto_dry_active),
        purifier_(*purifier),
        internal_thermistor_(*internal_thermistor),
        auto_dry_(*auto_dry),
        fahrenheit_(fahrenheit),
        slave_(is_slave_controller)
    {
        vane_select_1_.add_on_state_callback([this](size_t index) {
            set_vane_position(1, index);
        });
        vane_select_2_.add_on_state_callback([this](size_t index) {
            set_vane_position(2, index);
        });
        vane_select_3_.add_on_state_callback([this](size_t index) {
            set_vane_position(3, index);
        });
        vane_select_4_.add_on_state_callback([this](size_t index) {
            set_vane_position(4, index);
        });
        overheating_select_.add_on_state_callback([this](size_t index) {
            set_overheating(index);
        });

        fan_speed_slow_.add_on_state_callback([this](float v) {
            set_fan_speed(0, v);
        });
        fan_speed_low_.add_on_state_callback([this](float v) {
            set_fan_speed(1, v);
        });
        fan_speed_medium_.add_on_state_callback([this](float v) {
            set_fan_speed(2, v);
        });
        fan_speed_high_.add_on_state_callback([this](float v) {
            set_fan_speed(3, v);
        });
        sleep_timer_.add_on_state_callback([this](float v) {
            if (!ignore_sleep_timer_callback_ && set_sleep_timer(v)) {
                queue_status_change(STARTUP_TIMER);
            }
        });

        purifier_.add_on_state_callback([this](bool) {
            if (!ignore_switch_callbacks_) {
                queue_status_change(STARTUP_PURIFIER);
            }
        });
        internal_thermistor_.add_on_state_callback([this](bool) {
            if (!ignore_switch_callbacks_) {
                pending_status_change_ = true;
            }
        });
        auto_dry_.add_on_state_callback([this](bool) {
            if (!ignore_switch_callbacks_) {
                queue_type_a_change(TYPE_A_AUTO_DRY);
            }
        });
    }

    float get_setup_priority() const override {
        return esphome::setup_priority::BUS;
    }

    void setup() override {
        // Load our custom NVS storage to get the capabilities message
        ESPPreferenceObject pref = global_preferences->make_preference<NVSStorage>(this->get_object_id_hash() ^ NVS_STORAGE_VERSION);
        pref.load(&nvs_storage_);

        // ESPHome restores fan and swing modes only if the traits already advertise them.
        configure_capabilities();

        auto restore = this->restore_state_();
        if (restore.has_value()) {
            restore->apply(this);
        } else {
            this->mode = climate::CLIMATE_MODE_OFF;
            this->target_temperature = 20;
            this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
            this->swing_mode = climate::CLIMATE_SWING_OFF;
            this->publish_state();
        }

        ignore_switch_callbacks_ = true;
        internal_thermistor_.restore_and_set_mode(esphome::switch_::SWITCH_RESTORE_DEFAULT_OFF);
        ignore_switch_callbacks_ = false;
        publish_sleep_timer(0);

        while (UARTDevice::available() > 0) {
            uint8_t b;
            UARTDevice::read_byte(&b);
        }
        // The master sends its restored state and requests settings. The slave listens first.
        if (!slave_) {
            pending_status_change_ = true;
        }

        // Call `update` every 6 seconds, but first wait 10 seconds.
        set_timeout("initial_send", 10000, [this]() {
            set_interval("update", 6000, [this]() { update(); });
        });
    }

    // Process changes from HA.
    void control(const climate::ClimateCall &call) override {
        uint8_t fields = 0;
        if (call.get_mode().has_value()) {
            this->mode = *call.get_mode();
            fields |= STARTUP_MODE;
        }
        if (call.get_target_temperature().has_value()) {
            this->target_temperature = *call.get_target_temperature();
            fields |= STARTUP_TARGET;
        }
        if (call.get_fan_mode().has_value()) {
            this->fan_mode = *call.get_fan_mode();
            fields |= STARTUP_FAN;
        }
        if (call.get_swing_mode().has_value()) {
            set_swing_mode(*call.get_swing_mode());
            fields |= STARTUP_SWING;
        }
        queue_status_change(fields);
        this->publish_state();
    }

    climate::ClimateTraits traits() override {
        return supported_traits_;
    }

private:
    void queue_status_change(uint8_t fields) {
        if (slave_ && !slave_status_synced_) {
            startup_status_fields_ |= fields;
        }
        pending_status_change_ = true;
    }

    void queue_type_a_change(uint16_t field) {
        pending_type_a_fields_ |= field;
        pending_type_a_settings_change_ = true;
    }

    bool is_type_a_field_pending(uint16_t field) const {
        return ((pending_type_a_fields_ | sent_type_a_fields_) & field) != 0;
    }

    static optional<climate::ClimateMode> decode_status_mode(uint8_t status) {
        if ((status & 0x02) == 0) {
            return climate::CLIMATE_MODE_OFF;
        }
        switch ((status >> 2) & 0x07) {
            case 0: return climate::CLIMATE_MODE_COOL;
            case 1: return climate::CLIMATE_MODE_DRY;
            case 2: return climate::CLIMATE_MODE_FAN_ONLY;
            case 3: return climate::CLIMATE_MODE_HEAT_COOL;
            case 4: return climate::CLIMATE_MODE_HEAT;
            default: return {};
        }
    }

    static optional<climate::ClimateFanMode> decode_status_fan_mode(uint8_t speed) {
        switch (speed) {
            case 0: return climate::CLIMATE_FAN_LOW;
            case 1: return climate::CLIMATE_FAN_MEDIUM;
            case 2: return climate::CLIMATE_FAN_HIGH;
            case 3: return climate::CLIMATE_FAN_AUTO;
            case 4: return climate::CLIMATE_FAN_QUIET;
            case 7: return climate::CLIMATE_FAN_FOCUS;
            default: return {};
        }
    }

    static climate::ClimateAction action_from_status(climate::ClimateMode reported_mode,
                                                     const uint8_t* buffer) {
        if (reported_mode == climate::CLIMATE_MODE_OFF) {
            return climate::CLIMATE_ACTION_OFF;
        }
        if (buffer[3] & 0x04) {
            return climate::CLIMATE_ACTION_DEFROSTING;
        }
        if (reported_mode == climate::CLIMATE_MODE_FAN_ONLY) {
            return climate::CLIMATE_ACTION_FAN;
        }
        if (!(buffer[5] & 0x04) || (buffer[3] & 0x08)) {
            return climate::CLIMATE_ACTION_IDLE;
        }
        switch (reported_mode) {
            case climate::CLIMATE_MODE_COOL:
                return climate::CLIMATE_ACTION_COOLING;
            case climate::CLIMATE_MODE_DRY:
                return climate::CLIMATE_ACTION_DRYING;
            case climate::CLIMATE_MODE_HEAT:
                return climate::CLIMATE_ACTION_HEATING;
            case climate::CLIMATE_MODE_HEAT_COOL:
                if ((buffer[7] & 0xC0) == 0x40) {
                    return climate::CLIMATE_ACTION_COOLING;
                }
                if ((buffer[7] & 0xC0) == 0x80) {
                    return climate::CLIMATE_ACTION_HEATING;
                }
                return climate::CLIMATE_ACTION_IDLE;
            default:
                return climate::CLIMATE_ACTION_IDLE;
        }
    }

    // Sets position of vane index (1-4) to position (0-6).
    void set_vane_position(int index, int position) {
        if (index < 1 || index > 4) {
            ESP_LOGE(TAG, "Unexpected vane index: %d", index);
            return;
        }
        if (position < 0 || position > 6) {
            ESP_LOGE(TAG, "Unexpected vane position: %d", position);
            return;
        }
        if (vane_position_[index-1] == position) {
            return;
        }
        ESP_LOGD(TAG, "Setting vane %d position: %d", index, position);
        vane_position_[index-1] = position;
        if (!is_initializing_) {
            queue_type_a_change(1U << (index - 1));
        }
    }

    // Sets installer setting fan speed index (0-3 for slow-high) to value (0-255), with "0" being the factory default
    void set_fan_speed(int index, int value) {
        if (index < 0 || index > 3) {
            ESP_LOGE(TAG, "Unexpected fan speed index: %d", index);
            return;
        }
        if (value<0 || value>255) {
            ESP_LOGE(TAG, "Unexpected fan speed: %d", value);
            return;
        }

        if (fan_speed_[index] == value) {
            return;
        }

        fan_speed_[index] = value;
        if (!is_initializing_) {
            queue_type_a_change(1U << (index + 4));
        }
    }

    // Set overheating installer setting 15 to 0-4.
    void set_overheating(int value) {
        if (value < 0 || value > 4) {
            ESP_LOGE(TAG, "Unexpected overheating value: %d", value);
            return;
        }

        if (overheating_ == value) {
            return;
        }

        ESP_LOGD(TAG, "Setting overheating installer setting: %d", value);

        overheating_ = value;
        overheating_change_pending_ = true;
        pending_type_b_settings_change_ = true;
    }

    bool set_sleep_timer(int minutes) {
        // 0 clears the timer. Accept max 7 hours.
        if (minutes < 0 || minutes > 7 * 60) {
            ESP_LOGE(TAG, "Ignoring invalid sleep timer value: %d minutes", minutes);
            return false;
        }
        ESP_LOGD(TAG, "Setting sleep timer: %d minutes", minutes);
        last_reported_sleep_minutes_ = minutes;
        if (minutes > 0) {
            sleep_timer_target_millis_ = millis() + unsigned(minutes) * 60 * 1000;
            active_reservation_ = true;
        } else {
            sleep_timer_target_millis_.reset();
            active_reservation_ = false;
        }
        return true;
    }

    void publish_sleep_timer(uint32_t minutes) {
        ignore_sleep_timer_callback_ = true;
        sleep_timer_.publish_state(minutes);
        ignore_sleep_timer_callback_ = false;
    }

    void publish_switch_state(LgSwitch& entity, bool state) {
        ignore_switch_callbacks_ = true;
        entity.publish_state(state);
        ignore_switch_callbacks_ = false;
    }

    optional<float> get_room_temp() const {
        if (temperature_sensor_ == nullptr) {
            return {};
        }

        float temp = temperature_sensor_->get_state();
        if (std::isnan(temp) || temp == 0) {
            return {};
        }
        if (fahrenheit_) {
            temp = TempConversion::fahrenheit_to_lgcelsius(temp);
        }
        if (temp < 11) {
            return 11;
        }
        if (temp > 35) {
            return 35;
        }
        // Round to nearest 0.5 degrees.
        return round(temp * 2) / 2;
    }

    optional<uint32_t> get_sleep_timer_minutes() const {
        if (!sleep_timer_target_millis_.has_value()) {
            return {};
        }
        int32_t diff = int32_t(sleep_timer_target_millis_.value() - millis());
        if (diff <= 0) {
            return {};
        }
        return (uint32_t(diff) + 59999) / 60000;
    }

    void sync_sleep_timer(const uint8_t* buffer) {
        active_reservation_ = buffer[3] & 0x10;
        if (((buffer[8] >> 3) & 0x7) != 3) {
            if (!active_reservation_) {
                sleep_timer_target_millis_.reset();
                last_reported_sleep_minutes_ = 0;
                if (sleep_timer_.state > 0) {
                    publish_sleep_timer(0);
                }
            }
            return;
        }

        uint32_t minutes = (uint32_t(buffer[8] & 0x7) << 8) | buffer[9];
        if (minutes > 7 * 60) {
            return;
        }
        if (minutes == 0) {
            set_sleep_timer(0);
        } else if (!slave_) {
            // Equal reports are echoes. A lower report can shorten the countdown, and a
            // higher report is a new duration. Keep the last value after local expiry.
            if (minutes > last_reported_sleep_minutes_) {
                set_sleep_timer(minutes);
            } else if (sleep_timer_target_millis_.has_value()) {
                uint32_t now = millis();
                uint32_t duration = minutes * 60000;
                if (int32_t(*sleep_timer_target_millis_ - now) > int32_t(duration)) {
                    sleep_timer_target_millis_ = now + duration;
                }
            }
            last_reported_sleep_minutes_ = minutes;
            minutes = get_sleep_timer_minutes().value_or(0);
        }
        publish_sleep_timer(minutes);
    }

    static uint8_t calc_checksum(const uint8_t* buffer) {
        size_t result = 0;
        for (size_t i = 0; i < 12; i++) {
            result += buffer[i];
        }
        return (result & 0xff) ^ 0x55;
    }

    void set_swing_mode(climate::ClimateSwingMode mode) {
        if (this->swing_mode != mode) {
            // If vertical swing is off, send a 0xAA message to restore the vane position.
            if (mode == climate::CLIMATE_SWING_OFF || mode == climate::CLIMATE_SWING_HORIZONTAL) {
                pending_type_a_settings_change_ = true;
            }
        }
        this->swing_mode = mode;
    }

    void send_status_message() {
        // Byte 0: message type.
        send_buf_[0] = slave_ ? 0x28 : 0xA8;

        // Byte 1: changed flag (0x1), power on (0x2), mode (0x1C), fan speed (0xE0).
        uint8_t b = 0;
        if (pending_status_change_) {
            b |= 0x1;
        }
        switch (this->mode) {
            case climate::CLIMATE_MODE_COOL:
                b |= (0 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_DRY:
                b |= (1 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_FAN_ONLY:
                b |= (2 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_HEAT_COOL:
                b |= (3 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_HEAT:
                b |= (4 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_OFF:
                // Don't set power-on flag, but preserve previous operation mode.
                b |= (last_recv_status_[1] & 0x1C);
                break;
            default:
                ESP_LOGE(TAG, "unknown operation mode, turning off");
                b |= (2 << 2);
                break;
        }
        
        // Fix: Check if fan_mode has a value before dereferencing
        if (this->fan_mode.has_value()) {
            switch (this->fan_mode.value()) {
                case climate::CLIMATE_FAN_LOW:
                    b |= 0 << 5;
                    break;
                case climate::CLIMATE_FAN_MEDIUM:
                    b |= 1 << 5;
                    break;
                case climate::CLIMATE_FAN_HIGH:
                    b |= 2 << 5;
                    break;
                case climate::CLIMATE_FAN_AUTO:
                    b |= 3 << 5;
                    break;
                case climate::CLIMATE_FAN_QUIET:
                    b |= 4 << 5;
                    break;
                case climate::CLIMATE_FAN_FOCUS:
                    b |= 7 << 5;
                    break;
                default:
                    ESP_LOGE(TAG, "unknown fan mode, using Medium");
                    b |= 1 << 5;
                    break;
            }
        } else {
            // Default to Medium if no fan mode is set
            ESP_LOGD(TAG, "no fan mode set, using Medium as default");
            b |= 1 << 5;
        }
        send_buf_[1] = b;

        // Byte 2: swing mode and purifier/plasma setting. Preserve the other bits.
        b = last_recv_status_[2] & ~(0x4|0x40|0x80);
        if (purifier_.state) {
            b |= 0x4;
        }
        switch (this->swing_mode) {
            case climate::CLIMATE_SWING_OFF:
                break;
            case climate::CLIMATE_SWING_HORIZONTAL:
                b |= 0x40;
                break;
            case climate::CLIMATE_SWING_VERTICAL:
                b |= 0x80;
                break;
            case climate::CLIMATE_SWING_BOTH:
                b |= 0x40 | 0x80;
                break;
            default:
                ESP_LOGE(TAG, "unknown swing mode");
                break;
        }
        send_buf_[2] = b;

        // Byte 3.
        send_buf_[3] = last_recv_status_[3];
        if (active_reservation_) {
            send_buf_[3] |= 0x10;
        } else {
            send_buf_[3] &= ~0x10;
        }

        // Byte 4.
        send_buf_[4] = last_recv_status_[4];

        float target = this->target_temperature;
        if (fahrenheit_) {
            target = TempConversion::celsius_to_lgcelsius(target);
        }
        if (target < MIN_TEMP_SETPOINT) {
            target = MIN_TEMP_SETPOINT;
        } else if (target > MAX_TEMP_SETPOINT) {
            target = MAX_TEMP_SETPOINT;
        }

        // Byte 5. Unchanged except for the low bit which indicates the target temperature has a
        // 0.5 fractional part.
        send_buf_[5] = last_recv_status_[5] & ~0x1;
        if (target - uint8_t(target) == 0.5) {
            send_buf_[5] |= 0x1;
        }

        // Byte 6: thermistor setting and target temperature (fractional part in byte 5).
        // Byte 7: room temperature. Preserve the (unknown) upper two bits.
        enum ThermistorSetting { Unit = 0, Controller = 1, TwoTH = 2 };
        ThermistorSetting thermistor =
            internal_thermistor_.state ? ThermistorSetting::Unit : ThermistorSetting::Controller;
        float temp;
        if (auto maybe_temp = get_room_temp()) {
            temp = *maybe_temp;
        } else {
            // Room temperature isn't available. Use the unit's thermistor and send something
            // reasonable.
            thermistor = ThermistorSetting::Unit;
            temp = 20;
        }
        send_buf_[6] = (thermistor << 4) | ((uint8_t(target) - 15) & 0xf);
        send_buf_[7] = (last_recv_status_[7] & 0xC0) | uint8_t((temp - 10) * 2);

        // Bytes 8-10. Initialize bytes 8-9 to 0 to not echo back timer settings set by the AC.
        send_buf_[8] = 0;
        send_buf_[9] = 0;
        send_buf_[10] = last_recv_status_[10];

        if (is_initializing_) {
            // Request settings when controller turns on.
            send_buf_[8] |= 0x40;
            // Set bit 0x80 of byte 10 to use byte 9 for the Fahrenheit setting flag (0x40).
            if (fahrenheit_) {
                send_buf_[9] |= 0x40;
            }
            send_buf_[10] = 0x80;
        } else if (optional<uint32_t> minutes = get_sleep_timer_minutes()) {
            // Set sleep timer.
            // Byte 8 stores the kind (0x38) and high bits of number of minutes (0x7).
            // Byte 9 stores the low bits of the number of minutes.
            constexpr uint8_t timer_kind_sleep = 3;
            send_buf_[8] = timer_kind_sleep << 3;
            send_buf_[8] |= (*minutes >> 8) & 0b111;
            send_buf_[9] = *minutes & 0xff;
        }

        // Byte 11.
        send_buf_[11] = last_recv_status_[11];

        // Byte 12.
        send_buf_[12] = calc_checksum(send_buf_);

        ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);

        pending_status_change_ = false;
        pending_send_ = PendingSendKind::Status;
        last_sent_status_millis_ = millis();

        // If we sent an updated temperature to the AC, update temperature in HA too.
        // Slave controller temperature sensor is ignored.
        if (!slave_ && thermistor == ThermistorSetting::Controller) {
            float ha_temp = temp;
            if (fahrenheit_) {
                ha_temp = TempConversion::lgcelsius_to_celsius(ha_temp);
            }
            if (this->current_temperature != ha_temp) {
                this->current_temperature = ha_temp;
                publish_state();
            }
        }
    }

    void send_type_a_settings_message() {
        // Copy settings from the CA/AA message we received.
        memcpy(send_buf_, last_recv_type_a_settings_, MsgLen);
        send_buf_[0] = slave_ ? 0x2A : 0xAA;

        // Bytes 2-6 store the installer fan speeds
        send_buf_[2] = fan_speed_[0];
        send_buf_[3] = fan_speed_[1];
        send_buf_[4] = fan_speed_[2];
        send_buf_[5] = fan_speed_[3];

        // Bytes 7-8 store vane positions.
        send_buf_[7] = (send_buf_[7] & 0xf0) | (vane_position_[0] & 0x0f); // Set vane 1
        send_buf_[7] = (send_buf_[7] & 0x0f) | ((vane_position_[1] & 0x0f) << 4); // Set vane 2
        send_buf_[8] = (send_buf_[8] & 0xf0) | (vane_position_[2] & 0x0f); // Set vane 3
        send_buf_[8] = (send_buf_[8] & 0x0f) | ((vane_position_[3] & 0x0f) << 4); // Set vane 4

        // Set auto dry setting.
        uint8_t b = send_buf_[11] & ~0x8;
        if (auto_dry_.state) {
            b |= 0x8;
        }
        send_buf_[11] = b;

        send_buf_[12] = calc_checksum(send_buf_);

        ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);

        pending_type_a_settings_change_ = false;
        sent_type_a_fields_ = pending_type_a_fields_;
        pending_type_a_fields_ = 0;
        pending_send_ = PendingSendKind::TypeA;
    }

    void send_type_b_settings_message(bool timed) {
        if (timed) {
            ESP_LOGD(TAG, "sending timed AB message");
        }
        if (last_recv_type_b_settings_[0] != 0xCB && last_recv_type_b_settings_[0] != 0xAB) {
            ESP_LOGE(TAG, "Unexpected missing previous CB/AB message");
            // Don't try to send another message immediately after.
            last_sent_recv_type_b_millis_ = millis();
            return;
        }

        // Copy settings from the CB/AB message we received.
        memcpy(send_buf_, last_recv_type_b_settings_, MsgLen);
        send_buf_[0] = slave_ ? 0x2B : 0xAB;

        // Set the high bit of the second byte to request a CB message from the unit.
        if (timed) {
            send_buf_[1] |= 0x80;
        } else {
            send_buf_[1] &= ~0x80;
            // Byte 2 stores installer setting 15.
            send_buf_[2] = (send_buf_[2] & 0xC7) | (overheating_ << 3);
        }

        send_buf_[12] = calc_checksum(send_buf_);

        ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);

        if (timed) {
            pending_type_b_request_ = false;
            pending_send_ = PendingSendKind::TypeBRequest;
        } else {
            pending_type_b_settings_change_ = false;
            overheating_change_sent_ = overheating_change_pending_;
            overheating_change_pending_ = false;
            pending_send_ = PendingSendKind::TypeB;
        }
        last_sent_recv_type_b_millis_ = millis();
    }

    static bool is_message_start(uint8_t byte) {
        uint8_t sender = byte & 0xF8;
        return sender == 0xC8 || sender == 0xA8 || sender == 0x28;
    }

    void receive_byte(uint8_t byte) {
        // Ignore padding and noise until we see a possible frame start.
        if (recv_buf_len_ == 0 && !is_message_start(byte)) {
            return;
        }

        recv_buf_[recv_buf_len_++] = byte;
        if (recv_buf_len_ < MsgLen) {
            return;
        }

        if (calc_checksum(recv_buf_) == recv_buf_[MsgLen - 1]) {
            process_message(recv_buf_);
            recv_buf_len_ = 0;
            return;
        }

        ESP_LOGW(TAG, "invalid checksum while recovering frame alignment: %s",
                 format_hex_pretty(recv_buf_, MsgLen).c_str());
        // Keep the earliest possible start in the remaining bytes.
        size_t next_start = 1;
        while (next_start < MsgLen && !is_message_start(recv_buf_[next_start])) {
            ++next_start;
        }
        recv_buf_len_ = MsgLen - next_start;
        if (recv_buf_len_ > 0) {
            std::memmove(recv_buf_, recv_buf_ + next_start, recv_buf_len_);
        }
    }

    void read_received_messages() {
        if (recv_buf_len_ > 0 && millis() - last_recv_millis_ > 15 * 1000) {
            ESP_LOGE(TAG, "discarding incomplete data %s",
                     format_hex_pretty(recv_buf_, recv_buf_len_).c_str());
            recv_buf_len_ = 0;
        }

        while (UARTDevice::available() > 0) {
            uint8_t byte;
            if (!UARTDevice::read_byte(&byte)) {
                break;
            }
            last_recv_millis_ = millis();
            receive_byte(byte);
        }
    }

    void process_message(const uint8_t* buffer) {
        ESP_LOGD(TAG, "received %s", format_hex_pretty(buffer, MsgLen).c_str());

        if (pending_send_ != PendingSendKind::None && memcmp(send_buf_, buffer, MsgLen) == 0) {
            ESP_LOGD(TAG, "verified send");
            if (pending_send_ == PendingSendKind::TypeA) {
                sent_type_a_fields_ = 0;
            } else if (pending_send_ == PendingSendKind::TypeB) {
                overheating_change_sent_ = false;
            }
            pending_send_ = PendingSendKind::None;
            return;
        }

        // Determine message type.
        optional<MessageSender> sender;
        switch (buffer[0] & 0xf8) {
            case 0xC8:
                sender = MessageSender::Unit;
                break;
            case 0xA8:
                if (!slave_) {
                    // Ignore (our own?) master controller messages.
                    return;
                }
                sender = MessageSender::Master;
                break;
            case 0x28:
                if (slave_) {
                    // Ignore (our own?) slave controller messages.
                    return;
                }
                sender = MessageSender::Slave;
                break;
            default:
                return; // Unknown message sender. Ignore.
        }

        switch (buffer[0] & 0b111) {
            case 0: // 0xC8/A8/28
                process_status_message(*sender, buffer);
                break;
            case 1: // 0xC9
                process_capabilities_message(*sender, buffer);
                break;
            case 2: // 0xCA/AA/2A
                process_type_a_settings_message(*sender, buffer);
                break;
            case 3: // 0xCB/AB/2B
                process_type_b_settings_message(*sender, buffer);
                break;
            case 4: // 0xCC/AC/2C
                process_type_c_status_message(*sender, buffer);
                break;
            case 6: // 0xCE/AE/2E
                process_type_e_status_message(*sender, buffer);
                break;
            case 7: // 0xCF/AF/2F
                process_type_f_status_message(*sender, buffer);
                break;
            default:
                return;
        }
    }

    void process_status_message(MessageSender sender, const uint8_t* buffer) {
        bool syncing_slave = slave_ && !slave_status_synced_;

        // Handle simple input sensors first. These are safe to update even if we have a pending
        // change.

        defrost_.publish_state(buffer[3] & 0x4);
        preheat_.publish_state(buffer[3] & 0x8);

        if (sender == MessageSender::Unit) {
            error_code_.publish_state(buffer[11]);
        }

        // Ignore short OFF pulses while the outdoor unit is starting.
        bool outdoor_on = buffer[5] & 0x4;
        if (outdoor_on) {
            outdoor_off_since_.reset();
            outdoor_.publish_state(true);
        } else if (!outdoor_.state) {
            outdoor_.publish_state(false);
        } else if (!outdoor_off_since_.has_value()) {
            outdoor_off_since_ = millis();
        } else if (millis() - *outdoor_off_since_ >= 8000) {
            outdoor_.publish_state(false);
            outdoor_off_since_.reset();
        }

        if (sender == MessageSender::Unit && !auto_dry_.is_internal()) {
            bool unit_off = (buffer[1] & 0x2) == 0;
            bool drying = (buffer[10] & 0x20) && unit_off;
            auto_dry_active_.publish_state(drying);
        }

        bool read_temp = false;
        if (slave_) {
            // Let the slave controller report the temperature from the master.
            read_temp = (sender == MessageSender::Master);
        } else {
            // Report the unit's room temperature only if we're using the internal thermistor.
            // With an external temperature sensor, some units report the temperature we sent and
            // others always send the internal temperature.
            read_temp = sender == MessageSender::Unit &&
                        (internal_thermistor_.state || !get_room_temp().has_value());
        }
        bool climate_changed = false;
        if (read_temp) {
            float room_temp = float(buffer[7] & 0x3F) / 2 + 10;
            if (fahrenheit_) {
                room_temp = TempConversion::lgcelsius_to_celsius(room_temp);
            }
            if (this->current_temperature != room_temp) {
                this->current_temperature = room_temp;
                climate_changed = true;
            }
        }

        uint8_t b = buffer[1];
        auto reported_mode = decode_status_mode(b);
        // The unit reports its live action even while a local setting change is pending.
        if (sender == MessageSender::Unit && reported_mode.has_value()) {
            auto reported_action = action_from_status(*reported_mode, buffer);
            if (this->action != reported_action) {
                this->action = reported_action;
                climate_changed = true;
            }
        }

        // Don't update our settings if we have a pending change/send, because else we overwrite
        // changes we still have to send (or are sending) to the AC.
        if (!syncing_slave && (pending_status_change_ || pending_send_ == PendingSendKind::Status)) {
            ESP_LOGD(TAG, "ignoring status settings while a change is pending");
            if (climate_changed) {
                publish_state();
            }
            return;
        }

        if (!reported_mode.has_value()) {
            ESP_LOGE(TAG, "received invalid operation mode from AC (%u)", (b >> 2) & 0x07);
            if (climate_changed) {
                publish_state();
            }
            return;
        }
        uint8_t fan_val = b >> 5;
        auto reported_fan = decode_status_fan_mode(fan_val);
        if (!reported_fan.has_value()) {
            ESP_LOGE(TAG, "received unexpected fan mode from AC (%u)", fan_val);
            if (climate_changed) {
                publish_state();
            }
            return;
        }

        if (sender != MessageSender::Slave) {
            memcpy(last_recv_status_, buffer, MsgLen);
        }
        // On the first slave sync, preserve only the fields an early local command changed.
        if (!(startup_status_fields_ & STARTUP_MODE)) {
            this->mode = *reported_mode;
        }
        if (!(startup_status_fields_ & STARTUP_FAN)) {
            this->fan_mode = *reported_fan;
        }
        if (!(startup_status_fields_ & STARTUP_PURIFIER)) {
            publish_switch_state(purifier_, buffer[2] & 0x4);
        }

        bool horiz_swing = buffer[2] & 0x40;
        bool vert_swing = buffer[2] & 0x80;
        climate::ClimateSwingMode reported_swing = climate::CLIMATE_SWING_OFF;
        if (horiz_swing && vert_swing) {
            reported_swing = climate::CLIMATE_SWING_BOTH;
        } else if (horiz_swing) {
            reported_swing = climate::CLIMATE_SWING_HORIZONTAL;
        } else if (vert_swing) {
            reported_swing = climate::CLIMATE_SWING_VERTICAL;
        }
        if (!slave_) {
            set_swing_mode(reported_swing);
        } else if (!(startup_status_fields_ & STARTUP_SWING)) {
            this->swing_mode = reported_swing;
        }

        float target = float((buffer[6] & 0xf) + 15);
        if (buffer[5] & 0x1) {
            target += 0.5;
        }
        if (fahrenheit_) {
            target = TempConversion::lgcelsius_to_celsius(target);
        }
        if (!(startup_status_fields_ & STARTUP_TARGET)) {
            this->target_temperature = target;
        }
        if (!(startup_status_fields_ & STARTUP_TIMER)) {
            sync_sleep_timer(buffer);
        }
        if (syncing_slave) {
            slave_status_synced_ = true;
            is_initializing_ = false;
            startup_status_fields_ = 0;
        }

        publish_state();
    }

    void process_capabilities_message(MessageSender sender, const uint8_t* buffer) {
        // Capabilities message. The unit sends this with the other settings so we're now
        // initialized.

        if (sender != MessageSender::Unit) {
            ESP_LOGE(TAG, "ignoring capabilities message not from unit");
            return;
        }

        // Check if we need to update the capabilities message.
        if (nvs_storage_.capabilities_message[0] == 0 ||
            std::memcmp(nvs_storage_.capabilities_message, buffer, MsgLen - 1) != 0) {

            bool first_capabilities = nvs_storage_.capabilities_message[0] == 0;
            NVSStorage updated;
            std::memcpy(updated.capabilities_message, buffer, MsgLen);
            ESPPreferenceObject pref = global_preferences->make_preference<NVSStorage>(this->get_object_id_hash() ^ NVS_STORAGE_VERSION);
            if (pref.save(&updated) && global_preferences->sync()) {
                nvs_storage_ = updated;
                // Only reboot automatically for the first saved capabilities.
                if (first_capabilities) {
                    ESP_LOGD(TAG, "restarting to apply initial capabilities");
                    App.safe_reboot();
                } else {
                    ESP_LOGD(TAG, "saved updated device capabilities, manual restart required to take effect");
                }
            } else {
                ESP_LOGW(TAG, "unable to save device capabilities");
            }
        }

        is_initializing_ = false;
    }

    void process_type_a_settings_message(MessageSender sender, const uint8_t* buffer) {
        // The master sends settings after the first 0xCA message.
        if (sender != MessageSender::Slave) {
            bool first_time = last_recv_type_a_settings_[0] == 0;
            memcpy(last_recv_type_a_settings_, buffer, MsgLen);
            if (first_time && !slave_) {
                pending_type_a_settings_change_ = true;
            }
        }

        LgSelect* const vanes[] = {&vane_select_1_, &vane_select_2_, &vane_select_3_, &vane_select_4_};
        for (size_t i = 0; i < 4; ++i) {
            if (is_type_a_field_pending(1U << i)) {
                continue;
            }
            uint8_t position = (buffer[7 + i / 2] >> ((i % 2) * 4)) & 0x0F;
            if (position <= 6) {
                vane_position_[i] = position;
                vanes[i]->publish_state(*vanes[i]->at(position));
            } else {
                ESP_LOGE(TAG, "Unexpected vane %u position: %u", unsigned(i + 1), position);
            }
        }

        if (!is_type_a_field_pending(TYPE_A_AUTO_DRY)) {
            publish_switch_state(auto_dry_, buffer[11] & 0x8);
        }

        if (sender != MessageSender::Slave) {
            LgNumber* const speeds[] = {&fan_speed_slow_, &fan_speed_low_, &fan_speed_medium_, &fan_speed_high_};
            for (size_t i = 0; i < 4; ++i) {
                if (!is_type_a_field_pending(1U << (i + 4))) {
                    fan_speed_[i] = buffer[2 + i];
                    speeds[i]->publish_state(fan_speed_[i]);
                }
            }
        }
    }

    void process_type_b_settings_message(MessageSender sender, const uint8_t* buffer) {
        // Ignore this message from other controllers.
        if (sender != MessageSender::Unit) {
            return;
        }

        // The master sends installer settings after the first 0xCB message.
        bool first_time = last_recv_type_b_settings_[0] == 0;
        memcpy(last_recv_type_b_settings_, buffer, MsgLen);
        if (first_time && !slave_) {
            pending_type_b_settings_change_ = true;
        }

        last_sent_recv_type_b_millis_ = millis();
        pending_type_b_request_ = false;
        if (pending_send_ == PendingSendKind::TypeBRequest) {
            pending_send_ = PendingSendKind::None;
        }

        uint8_t overheating = (buffer[2] >> 3) & 0b111;
        if (overheating > 4) {
            ESP_LOGE(TAG, "Unexpected overheating value: %u", overheating);
        } else if (!overheating_change_pending_ && !overheating_change_sent_) {
            overheating_ = overheating;
            overheating_select_.publish_state(*overheating_select_.at(overheating));
        }

        // Table mapping a byte value to degrees Celsius based on values displayed by PREMTB100.
        // INT8_MIN indicates an invalid value.
        static constexpr int8_t PipeTempTable[] = {
            /* 0x00 */ INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN,
                       INT8_MIN, INT8_MIN, INT8_MIN, 108, 104, 101, 100, 98, 95,
            /* 0x10 */ 93, 91, 89, 87, 85, 84, 82, 81, 79, 78, 76, 75, 74, 73, 72, 71,
            /* 0x20 */ 70, 68, 68, 67, 66, 65, 64, 63, 62, 61, 60, 60, 59, 58, 57, 57,
            /* 0x30 */ 56, 55, 55, 54, 53, 53, 52, 52, 51, 50, 50, 49, 49, 48, 47, 47,
            /* 0x40 */ 46, 46, 45, 45, 44, 44, 43, 43, 42, 42, 41, 41, 40, 40, 39, 39,
            /* 0x50 */ 39, 38, 38, 37, 37, 36, 36, 36, 35, 35, 34, 34, 33, 33, 33, 32,
            /* 0x60 */ 32, 31, 31, 31, 30, 30, 30, 29, 29, 29, 28, 28, 27, 27, 27, 26,
            /* 0x70 */ 26, 26, 25, 25, 24, 24, 24, 23, 23, 23, 22, 22, 22, 21, 21, 21,
            /* 0x80 */ 20, 20, 20, 19, 19, 19, 18, 18, 18, 17, 17, 17, 16, 16, 16, 15,
            /* 0x90 */ 15, 15, 14, 14, 14, 13, 13, 13, 12, 12, 12, 11, 11, 11, 10, 10,
            /* 0xa0 */ 10, 9, 9, 9, 8, 8, 8, 7, 7, 6, 6, 6, 5, 5, 5, 4,
            /* 0xb0 */ 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 0, 0, 0, 0, 0, -1,
            /* 0xc0 */ -1, -2, -2, -2, -3, -3, -4, -4, -5, -5, -5, -6, -6, -7, -7, -8,
            /* 0xd0 */ -8, -9, -9, -9, -10, -10, -11, -11, -12, -12, -13, -14, -14, -15, -15, -16,
            /* 0xe0 */ -16, -17, -18, -18, -19, -20, -20, -21, -22, -22, -23, -24, -25, -26, -27,
                       -28,
            /* 0xf0 */ -29, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN,
                       INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN
        };
        static_assert(sizeof(PipeTempTable) == 256);
        static_assert(PipeTempTable[UINT8_MAX] == INT8_MIN);

        int8_t pipe_temp_in = PipeTempTable[buffer[3]];
        if (pipe_temp_in != INT8_MIN) {
            pipe_temp_in_.publish_state(pipe_temp_in);
        }

        int8_t pipe_temp_out = PipeTempTable[buffer[4]];
        if (pipe_temp_out != INT8_MIN) {
            pipe_temp_out_.publish_state(pipe_temp_out);
        }

        int8_t pipe_temp_mid = PipeTempTable[buffer[5]];
        if (pipe_temp_mid != INT8_MIN) {
            pipe_temp_mid_.publish_state(pipe_temp_mid);
        }
    }

    static uint32_t decode_uint24_be(const uint8_t* bytes) {
        return (uint32_t(bytes[0]) << 16) | (uint32_t(bytes[1]) << 8) | uint32_t(bytes[2]);
    }

    void process_type_c_status_message(MessageSender sender, const uint8_t* buffer) {
        if (sender == MessageSender::Unit && power_consumption_ != nullptr) {
            power_consumption_->publish_state(decode_uint24_be(buffer + 3) / 10.0f);
        }
    }

    void process_type_e_status_message(MessageSender sender, const uint8_t* buffer) {
        if (sender == MessageSender::Slave || buffer[1] != 0x80) {
            return;
        }

        if (humidity_ != nullptr && buffer[2] <= 100) {
            humidity_->publish_state(buffer[2]);
        }
        if (fan_operation_time_ != nullptr) {
            fan_operation_time_->publish_state(encode_uint16(buffer[3], buffer[4]));
        }
        if (indoor_unit_operation_time_ != nullptr) {
            indoor_unit_operation_time_->publish_state(encode_uint16(buffer[6], buffer[7]));
        }
        if (precise_room_temperature_ != nullptr && buffer[11] <= 9) {
            precise_room_temperature_->publish_state(buffer[10] + buffer[11] / 10.0f);
        }
    }

    void process_type_f_status_message(MessageSender sender, const uint8_t* buffer) {
        if (sender == MessageSender::Unit && current_power_ != nullptr) {
            current_power_->publish_state(decode_uint24_be(buffer + 2) / 1000.0f);
        }
    }

    void update() {
        ESP_LOGD(TAG, "update");

        read_received_messages();

        // If we did not receive the message we sent last time, try to send it again next time.
        // Ignore this when we're initializing because the unit then immediately responds by
        // sending a lot of messages and this introduces a delay.
        if (pending_send_ != PendingSendKind::None &&
            (!is_initializing_ || sent_type_a_fields_ != 0 || overheating_change_sent_)) {
            ESP_LOGE(TAG, "did not receive message we just sent");
            switch (pending_send_) {
                case PendingSendKind::Status:
                    pending_status_change_ = true;
                    break;
                case PendingSendKind::TypeA:
                    pending_type_a_fields_ |= sent_type_a_fields_;
                    sent_type_a_fields_ = 0;
                    pending_type_a_settings_change_ = true;
                    break;
                case PendingSendKind::TypeB:
                    overheating_change_pending_ |= overheating_change_sent_;
                    overheating_change_sent_ = false;
                    pending_type_b_settings_change_ = true;
                    break;
                case PendingSendKind::TypeBRequest:
                    pending_type_b_request_ = true;
                    break;
                case PendingSendKind::None:
                    ESP_LOGE(TAG, "unreachable");
                    break;
            }
            pending_send_ = PendingSendKind::None;
            return;
        }

        if (recv_buf_len_ > 0) {
            return;
        }

        uint32_t millis_now = millis();

        // Handle sleep timer.
        if (sleep_timer_target_millis_.has_value()) {
            int32_t diff = int32_t(sleep_timer_target_millis_.value() - millis_now);
            if (diff <= 0) {
                ESP_LOGD(TAG, "Turning off for sleep timer");
                sleep_timer_target_millis_.reset();
                active_reservation_ = false;
                publish_sleep_timer(0);
                this->mode = climate::CLIMATE_MODE_OFF;
                pending_status_change_ = true;
                publish_state();
            } else if (optional<uint32_t> minutes = get_sleep_timer_minutes()) {
                if (sleep_timer_.state != *minutes) {
                    publish_sleep_timer(*minutes);
                }
            }
        }

        if (slave_ && !slave_status_synced_) {
            ESP_LOGD(TAG, "Not sending, waiting for other controller or unit to send first");
            return;
        }

        // Make sure the RX pin is idle for at least 500 ms to avoid collisions on the bus as much
        // as possible. If there is still a collision, we'll likely both start sending at
        // approximately the same time and the message will hopefully be corrupt (and ignored)
        // anyway. Else the pending_send_/send_buf_ mechanism should catch it and we try again.
        //
        // Note: using digital_read is *much* better for this than using UARTDevice because that
        // interface has significant delays. It has to wait for a full byte to arrive and this
        // takes about 9-10 ms with our slow baud rate. There are also various buffers and
        // timeouts before incoming bytes reach us.
        //
        // 500 ms might be overkill, but the device usually sends the same message twice with a
        // short delay (about 200 ms?) between them so let's not send there either to avoid
        // collisions.
        auto check_can_send = [&]() -> bool {
            while (true) {
                if (UARTDevice::available() > 0 || !rx_pin_.digital_read()) {
                    ESP_LOGD(TAG, "line busy, not sending yet");
                    return false;
                }
                if (millis() - millis_now > 500) {
                    return true;
                }
                delay(5);
            }
        };

        if (pending_type_a_settings_change_ &&
            (last_recv_type_a_settings_[0] == 0xCA || last_recv_type_a_settings_[0] == 0xAA)) {
            if (check_can_send()) {
                send_type_a_settings_message();
            }
            return;
        }
        if (pending_type_b_settings_change_ && last_recv_type_b_settings_[0] != 0) {
            if (check_can_send()) {
                send_type_b_settings_message(/* timed = */ false);
            }
            return;
        }
        // Send a status message if there is a pending change.
        // Additionally, queue a Type A message after sending the status message because some
        // units set the vane position to the default setting after changing swing mode or
        // operation mode.
        if (pending_status_change_) {
            if (check_can_send()) {
                send_status_message();
                pending_type_a_settings_change_ = true;
            }
            return;
        }
        // Send a status message every 20 seconds.
        // Slave controllers only send a status message when settings are changed.
        if (!slave_ && millis_now - last_sent_status_millis_ > 20 * 1000) {
            if (check_can_send()) {
                send_status_message();
            }
            return;
        }
        // Send an AB message every 10 minutes to request pipe temperature values.
        if (!slave_ && (pending_type_b_request_ || millis_now - last_sent_recv_type_b_millis_ > 10 * 60 * 1000)) {
            if (check_can_send()) {
                send_type_b_settings_message(/* timed = */ true);
            }
        }
    }
};

} // namespace esphome::lg_controller
