#include "sdkconfig.h"
#include "util/exceptions.h"
#include <esp_log.h>
#include "app_events.h"
#include <stdio.h>
#include "logging/logging_tags.h"
#include "util/default_event.h"

ESP_EVENT_DEFINE_BASE(APP_COMMON_EVENT);

extern "C" void app_main(void)
{
    ESP_LOGI(OPERATIONS_TAG, "Starting ....");
    esp_log_level_set("*", ESP_LOG_DEBUG);

    try
    {
        CHECK_THROW_ESP(esp_event_loop_create_default());

        CHECK_THROW_ESP(esp32::event_post(APP_COMMON_EVENT, APP_INIT_DONE));

        ESP_LOGI(OPERATIONS_TAG, "Main task is done");
    }
    catch (const std::exception &ex)
    {
        ESP_LOGI(OPERATIONS_TAG, "Init Failure:%s", ex.what());
        throw;
    }
}
