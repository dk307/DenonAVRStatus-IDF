#pragma once

#include "app_events.h"
#include "util/default_event.h"
#include "util/semaphore_lockable.h"
#include "util/singleton.h"
#include "util/task_wrapper.h"
#include "util/timer/timer.h"
#include <max7219.h>
#include <variant>

class display final : public esp32::singleton<display>
{
  public:
    void begin();

    void set_volume(uint8_t volume)
    {
        display_value.store(Volume{volume});
        xTaskNotify(lvgl_task_.handle(), set_display_changed_bit, eSetBits);
    }

    void clear()
    {
        display_value.store(None());
        xTaskNotify(lvgl_task_.handle(), set_display_changed_bit, eSetBits);
    }

  private:
    display() : lvgl_task_([this] { display::gui_task(); })
    {
    }

    friend class esp32::singleton<display>;

    esp32::task lvgl_task_;
    spi_bus_config_t cfg{};
    max7219_t handle{};

    typedef struct None
    {
        uint8_t value;
    } None;
    typedef struct Volume
    {
        uint8_t value;
    } Volume;

    // one of these state
    std::atomic<std::variant<None, Volume>> display_value{None()};

    std::unique_ptr<esp32::timer::timer> display_off_timer;
    std::unique_ptr<esp32::timer::timer> display_fade_timer;

    uint8_t default_brightness{8};
    uint8_t current_brightness{default_brightness};

    const std::chrono::seconds displayOffTimeout{5};
    const std::chrono::milliseconds fadeIntervalDelay{100};

    esp32::default_event_subscriber instance_app_common_event_{
        APP_COMMON_EVENT, ESP_EVENT_ANY_ID, [this](esp_event_base_t base, int32_t event, void *data) { app_event_handler(base, event, data); }};

    void gui_task();
    void app_event_handler(esp_event_base_t, int32_t, void *);
    void update_display();
    void restart_display_off_timer();
    void set_atleast_default_brightness();
    void set_brightness(uint8_t value);
    void set_display(const std::array<const void *, 4> &values);

    constexpr static uint32_t set_display_changed_bit = BIT(2);
    constexpr static uint32_t fade_display_bit = BIT(3);
};