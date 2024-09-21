#pragma once

#include <esp_event.h>

ESP_EVENT_DECLARE_BASE(APP_COMMON_EVENT);

typedef enum
{
    /** App init done*/
    APP_INIT_DONE,


} esp_app_common_event_t;
