#include "app_events.h"
#include "config/config_manager.h"
#include "hardware/display/display.h"
#include "hardware/uart/denon_avr.h"
#include "logging/logging_tags.h"
#include "nvs.h"
#include "sdkconfig.h"
#include "util/default_event.h"
#include "util/exceptions.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <stdio.h>

ESP_EVENT_DEFINE_BASE(APP_COMMON_EVENT);

extern "C" void app_main(void)
{
    ESP_LOGI(OPERATIONS_TAG, "Starting ....");
    esp_log_level_set("*", ESP_LOG_WARN);

    try
    {
        const auto err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            ESP_LOGW(OPERATIONS_TAG, "Erasing flash");
            CHECK_THROW_ESP(nvs_flash_erase());
            CHECK_THROW_ESP(nvs_flash_init());
        }

        CHECK_THROW_ESP(esp_event_loop_create_default());

        auto &config = config::create_instance();
        auto &denon_avr = denon_avr::create_instance();
        auto &display = display::create_instance(config, denon_avr);

        config.begin();
        display.begin();
        denon_avr.begin();

        CHECK_THROW_ESP(esp32::event_post(APP_COMMON_EVENT, APP_INIT_DONE));

        ESP_LOGI(OPERATIONS_TAG, "Main task is done");
    }
    catch (const std::exception &ex)
    {
        ESP_LOGI(OPERATIONS_TAG, "Init Failure:%s", ex.what());
        vTaskDelay(pdMS_TO_TICKS(3000));
        throw;
    }
}
