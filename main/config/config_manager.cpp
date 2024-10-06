#include "config_manager.h"
#include "app_events.h"
#include "logging/logging_tags.h"
#include "util/default_event.h"
#include "util/helper.h"
#include <esp_log.h>
#include <filesystem>

constexpr std::string_view screen_brightness_key{"scrn_brightness"};

void config::begin()
{
    ESP_LOGD(CONFIG_TAG, "Loading Configuration");

    nvs_storage_.begin("nvs", "config");
    ESP_LOGI(CONFIG_TAG, "Screen brightness:%d", get_screen_brightness());
}

void config::save()
{
    ESP_LOGI(CONFIG_TAG, "config save");
    nvs_storage_.commit();
    CHECK_THROW_ESP(esp32::event_post(APP_COMMON_EVENT, CONFIG_CHANGE));
}

uint8_t config::get_screen_brightness()
{
    std::lock_guard<esp32::semaphore> lock(data_mutex_);
    const auto value = nvs_storage_.get(screen_brightness_key, static_cast<uint8_t>(8));
    return value;
}

void config::set_screen_brightness(uint8_t screen_brightness)
{
    std::lock_guard<esp32::semaphore> lock(data_mutex_);
    nvs_storage_.save(screen_brightness_key, screen_brightness);
}
