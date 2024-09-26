#include "display.h"
#include "logging/logging_tags.h"
#include "util/cores.h"
#include "util/default_event.h"
#include "util/exceptions.h"
#include <driver/spi_master.h>
#include <esp_log.h>

#define MOSI_PIN 21
#define CLK_PIN 18
const gpio_num_t CS_PIN = GPIO_NUM_19;
#define HOST SPI3_HOST
#define CASCADE_SIZE 4

void display::begin()
{
    instance_app_common_event_.subscribe();

    ESP_LOGI(DISPLAY_TAG, "Initializing SPI BUS");
    spi_bus_config_t cfg = {.mosi_io_num = MOSI_PIN,
                            .miso_io_num = -1,
                            .sclk_io_num = CLK_PIN,
                            .quadwp_io_num = -1,
                            .quadhd_io_num = -1,
                            .data4_io_num = -1,
                            .data5_io_num = -1,
                            .data6_io_num = -1,
                            .data7_io_num = -1,
                            .max_transfer_sz = 0,
                            .flags = 0,
                            .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
                            .intr_flags = 0};
    CHECK_THROW_ESP(spi_bus_initialize(HOST, &cfg, SPI_DMA_CH1));

    ESP_LOGI(DISPLAY_TAG, "SPI Bus for display initialized");

    handle.cascade_size = CASCADE_SIZE;
    handle.mirrored = true;

    ESP_LOGI(DISPLAY_TAG, "Setting up display");

    CHECK_THROW_ESP(max7219_init_desc(&handle, HOST, MAX7219_MAX_CLOCK_SPEED_HZ, CS_PIN));
    CHECK_THROW_ESP(max7219_init(&handle));
    CHECK_THROW_ESP(lvgl_task_.spawn_pinned("gui", 1024 * 6, esp32::task::default_priority, esp32::display_core));
    ESP_LOGI(DISPLAY_TAG, "Display setup done");
}

// https://xantorohara.github.io/led-matrix-editor/
constexpr static std::array<uint64_t, 10> digits_led_bits = {
    0x3c6666666666663c, // 0
    0x7e181818181e1c18, // 1
    0x7e66063c6066667c, // 2
    0x3c6666303066663c, // 3
    0x3030307e36363636, // 4
    0x3c6666603e06667e, // 5
    0x3c66663e0666663c, // 6
    0x0c0c18383060667e, // 7
    0x3c66663c3c66663c, // 8
    0x3c6666607e66663c, // 9
};

void display::update_display()
{
    const auto current_display_value = display_value.load();
    if (std::holds_alternative<None>(current_display_value))
    {
        // start a fade timer
        display_off_timer.reset();
        ESP_LOGI(DISPLAY_TAG, "Clearing display with fading");
        display_fade_timer =
            std::make_unique<esp32::timer::timer>([this] { xTaskNotify(lvgl_task_.handle(), fade_display_bit, eSetBits); }, "display_fade_timer");
        display_fade_timer->start_periodic(fadeIntervalDelay);
    }
    else if (std::holds_alternative<Volume>(current_display_value))
    {
        display_fade_timer.reset();
        set_atleast_default_brightness();

        auto &&volume = std::get<Volume>(current_display_value);
        ESP_LOGI(DISPLAY_TAG, "Setting display to volume:%u", volume.value);

        const auto first_digit = volume.value / 10;
        const auto second_digit = volume.value % 10;

        const uint64_t zero = 0;
        const std::array<const void *, 4> display_values({&zero, &digits_led_bits[first_digit], &digits_led_bits[second_digit], &zero});
        set_display(display_values);
        restart_display_off_timer();
    }
}

void display::set_display(const std::array<const void *, 4> &values)
{
    CHECK_THROW_ESP(max7219_draw_image_8x8(&handle, 0, values[0]));
    CHECK_THROW_ESP(max7219_draw_image_8x8(&handle, 8, values[1]));
    CHECK_THROW_ESP(max7219_draw_image_8x8(&handle, 16, values[2]));
    CHECK_THROW_ESP(max7219_draw_image_8x8(&handle, 24, values[3]));
}

void display::restart_display_off_timer()
{
    if (display_off_timer)
    {
        display_off_timer->restart(displayOffTimeout);
    }
    else
    {
        display_off_timer = std::make_unique<esp32::timer::timer>([this] { display::clear(); }, "display_off_timer");
        display_off_timer->start_one_shot(displayOffTimeout);
    }
}

void display::set_atleast_default_brightness()
{
    if (current_brightness < default_brightness)
    {
        set_brightness(default_brightness);
    }
}

void display::set_brightness(uint8_t value)
{
    CHECK_THROW_ESP(max7219_set_brightness(&handle, value));
    current_brightness = value;
}

void display::gui_task()
{
    ESP_LOGI(DISPLAY_TAG, "Start to run Display Task on core:%d", xPortGetCoreID());

    try
    {
        set_brightness(default_brightness);
        CHECK_THROW_ESP(max7219_clear(&handle));

        do
        {
            uint32_t notification_value = 0;
            xTaskNotifyWait(pdFALSE,             /* Don't clear bits on entry. */
                            ULONG_MAX,           /* Clear all bits on exit. */
                            &notification_value, /* Stores the notified value. */
                            portMAX_DELAY);

            if (notification_value & set_display_changed_bit)
            {
                update_display();
            }
            else if (notification_value & fade_display_bit)
            {
                if (current_brightness <= 1)
                {
                    display_fade_timer.reset();
                    ESP_LOGI(DISPLAY_TAG, "Display off");
                    CHECK_THROW_ESP(max7219_clear(&handle));
                }
                else
                {
                    current_brightness--;
                    set_brightness(current_brightness);
                }
            }

        } while (true);
    }
    catch (const std::exception &ex)
    {
        ESP_LOGE(OPERATIONS_TAG, "UI Task Failure:%s", ex.what());
        vTaskDelay(pdMS_TO_TICKS(3000));
        throw;
    }

    vTaskDelete(NULL);
}

void display::app_event_handler(esp_event_base_t, int32_t event, void *data)
{
    switch (event)
    {
    case APP_INIT_DONE:
        // xTaskNotify(lvgl_task_.handle(), set_main_screen_changed_bit, eSetBits);
        break;
    }
}
