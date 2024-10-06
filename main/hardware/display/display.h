#pragma once

#include "app_events.h"
#include "config/config_manager.h"
#include "hardware/uart/denon_avr.h"
#include "util/default_event.h"
#include "util/semaphore_lockable.h"
#include "util/singleton.h"
#include "util/task_wrapper.h"
#include "util/timer/timer.h"
#include <iot_button.h>
#include <max7219.h>
#include <variant>

class display final : public esp32::singleton<display>
{
  public:
    void begin();

    template <typename T> void set_display_value(T &&value)
    {
        display_value_.store(std::forward<T>(value));
        xTaskNotify(gui_task_.handle(), set_display_changed_bit, eSetBits);
    }

  private:
    display(config &config, denon_avr &denon_avr) : config_(config), denon_avr_(denon_avr), gui_task_([this] { display::gui_task(); })
    {
    }

    friend class esp32::singleton<display>;

    config &config_;
    denon_avr &denon_avr_;
    esp32::task gui_task_;
    max7219_t handle_{};

    typedef struct None
    {
        uint8_t value;
    } None;
    typedef struct FourChars
    {
        std::array<uint8_t, 4> value;
    } FourChars;
    typedef struct MuteOn
    {
    } MuteOn;
    typedef struct DynVol
    {
        uint8_t value;
    } DynVol;
    typedef struct PowerOff
    {
    } PowerOff;
    typedef struct ScreenBrightnessLevel
    {
        uint8_t value;
    } ScreenBrightnessLevel;

    // one of these state
    std::atomic<std::variant<None, FourChars, MuteOn, DynVol, PowerOff, ScreenBrightnessLevel>> display_value_{None()};

    std::unique_ptr<esp32::timer::timer> display_off_timer_;
    std::unique_ptr<esp32::timer::timer> display_fade_timer_;

    uint8_t current_brightness_{0};
    button_handle_t button_;

    const std::chrono::seconds display_off_timeout_{5};
    const std::chrono::milliseconds fade_interval_delay_{120};

    esp32::default_event_subscriber instance_app_common_event_{
        APP_COMMON_EVENT, ESP_EVENT_ANY_ID, [this](esp_event_base_t base, int32_t event, void *data) { app_event_handler(base, event, data); }};

    void gui_task();
    void app_event_handler(esp_event_base_t, int32_t, void *);
    void update_display_based_on_display_value();
    std::array<const void *, 4U> get_display_led_bits(const std::array<uint8_t, 4> &fourChars);
    const void *get_display_led_bits(uint8_t c);
    void restart_display_off_timer();
    void set_default_brightness();
    void set_max7219_brightness(uint8_t value);
    void set_max7219_display(const std::array<const void *, 4> &values);
    void start_display(const std::array<const void *, 4> &values, bool turn_off);
    void button_click();

    template <void (display::*ftn)()> static void button_event_callback(void *, void *usr_data)
    {
        auto p_this = reinterpret_cast<display *>(usr_data);
        (p_this->*ftn)();
    }

    constexpr static uint32_t set_display_changed_bit = BIT(2);
    constexpr static uint32_t fade_display_bit = BIT(3);
    constexpr static uint32_t button_clicked_display_bit = BIT(4);
};