#include "display.h"
#include "logging/logging_tags.h"
#include "util/cores.h"
#include "util/default_event.h"
#include "util/exceptions.h"
#include <driver/spi_master.h>
#include <esp_log.h>
#include <util/helper.h>

constexpr static int MOSI_PIN = 21;
constexpr static int CLK_PIN = 18;
constexpr static gpio_num_t CS_PIN = GPIO_NUM_19;
constexpr static spi_host_device_t HOST = SPI3_HOST;
constexpr static int CASCADE_SIZE = 4;

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

    handle_.cascade_size = CASCADE_SIZE;
    handle_.mirrored = true;

    ESP_LOGI(DISPLAY_TAG, "Setting up display");

    CHECK_THROW_ESP(max7219_init_desc(&handle_, HOST, MAX7219_MAX_CLOCK_SPEED_HZ, CS_PIN));
    CHECK_THROW_ESP(max7219_init(&handle_));
    CHECK_THROW_ESP(gui_task_.spawn_pinned("gui", 1024 * 6, esp32::task::default_priority, esp32::display_core));
    ESP_LOGI(DISPLAY_TAG, "Display setup done");

    set_display_value(FourChars{'B', 'o', 'o', 't'});

    button_config_t gpio_btn_cfg{};

    gpio_btn_cfg.type = BUTTON_TYPE_GPIO;
    gpio_btn_cfg.long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS;
    gpio_btn_cfg.short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS;
    gpio_btn_cfg.gpio_button_config.gpio_num = 23;
    gpio_btn_cfg.gpio_button_config.active_level = 0;
    button_ = iot_button_create(&gpio_btn_cfg);

    CHECK_THROW_ESP(iot_button_register_cb(button_, BUTTON_SINGLE_CLICK, button_event_callback<&display::button_click>, this));
}

void display::start_display(const std::array<const void *, 4> &values, bool turn_off)
{
    set_default_brightness();
    set_max7219_display(values);
    if (turn_off)
    {
        restart_display_off_timer();
    }
}

void display::update_display_based_on_display_value()
{
    const auto current_display_value = display_value_.load();
    display_fade_timer_.reset();
    if (std::holds_alternative<None>(current_display_value))
    {
        // start a fade timer
        display_off_timer_.reset();
        ESP_LOGI(DISPLAY_TAG, "Clearing display with fading");
        display_fade_timer_ =
            std::make_unique<esp32::timer::timer>([this] { xTaskNotify(gui_task_.handle(), fade_display_bit, eSetBits); }, "display_fade_timer");
        display_fade_timer_->start_periodic(fade_interval_delay_);
    }
    else if (std::holds_alternative<ScreenBrightnessLevel>(current_display_value))
    {
        constexpr static uint64_t all_on = 0xffffffffffffffff;
        constexpr std::array<const void *, 4> display_values{
            &all_on,
            &all_on,
            &all_on,
            &all_on,
        };

        start_display(display_values, true);
    }
    else if (std::holds_alternative<FourChars>(current_display_value))
    {
        auto &&four_chars = std::get<FourChars>(current_display_value);
        ESP_LOGI(DISPLAY_TAG, "Setting display to %s", reinterpret_cast<const char *>(four_chars.value.data()));
        const auto display_values = get_display_led_bits(four_chars.value);
        start_display(display_values, true);
    }
    else if (std::holds_alternative<MuteOn>(current_display_value))
    {
        ESP_LOGI(DISPLAY_TAG, "Setting Mute On");

        constexpr static uint64_t muteon_led_bits[] = {
            0x636363636b7f7763,
            0x3f333333b3000000,
            0xc646c646df060606,
            0xfbc8cbfac3c0c0c0,
        };
        constexpr std::array<const void *, 4> display_values{
            &muteon_led_bits[0],
            &muteon_led_bits[1],
            &muteon_led_bits[2],
            &muteon_led_bits[3],
        };

        start_display(display_values, false);
    }
    else if (std::holds_alternative<PowerOff>(current_display_value))
    {
        ESP_LOGI(DISPLAY_TAG, "Setting Poweroff");

        constexpr static uint64_t bits[] = {
            0xc0606060606060c0,
            0x636666e6e66666e3,
            0x606060e3e36060e7,
            0x0000000303000007,
        };
        constexpr std::array<const void *, 4> display_values{
            &bits[0],
            &bits[1],
            &bits[2],
            &bits[3],
        };

        start_display(display_values, true);
    }
    else if (std::holds_alternative<DynVol>(current_display_value))
    {
        // condensed "DynVol"
        constexpr static uint64_t dyn_vol_led_bits[] = {
            0xe789e9a9a9090907,
            0xcaca2a2a2e202020,
            0x5c55555d41414141,
        };

        constexpr static uint64_t dyn_vol_level_led_bits[] = {
            0x3c42858991a1423c,
            0x3c3cff7e3c180000,
            0x3c3c3cff7e3c1800,
            0x3c3c3c3cff7e3c18,
        };

        auto &&dyn_vol = std::get<DynVol>(current_display_value);
        ESP_LOGI(DISPLAY_TAG, "Setting Dynamic Volume:%d", dyn_vol.value);

        std::array<const void *, 4> display_values{
            &dyn_vol_led_bits[0],
            &dyn_vol_led_bits[1],
            &dyn_vol_led_bits[2],
            &dyn_vol_level_led_bits[dyn_vol.value],
        };

        start_display(display_values, true);
    }
}

std::array<const void *, 4U> display::get_display_led_bits(const std::array<uint8_t, 4> &values)
{
    std::array<const void *, 4U> display_values;
    size_t index = 0;
    for (auto &&c : values)
    {
        display_values[index] = get_display_led_bits(c);
        index++;
    }
    return display_values;
}

const void *display::get_display_led_bits(uint8_t c)
{
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

    constexpr static std::array<uint64_t, 26> letters_capital_led_bits = {
        0x6666667e66667e3c, // A
        0x3e66663e3e66663e, // B
        0x3c6606060606663c, // C
        0x3e6666666666663e, // D
        0x7e06063e3e06067e, // E
        0x0606063e3e06067e, // F
        0x3c6666760646663c, // G
        0x6666667e7e666666, // H
        0x3c1818181818183c, // I
        0x1c36363030303078, // J
        0x66361e0e0e1e3666, // K
        0x7e06060606060606, // L
        0xc6c6c6c6d6feeec6, // M
        0xc6e6f6fedecec6c6, // N
        0x3c6666666666663c, // O
        0x06063e7e6666663e, // P
        0x603c76666666663c, // Q
        0x66361e3e6666663e, // R
        0x3c66703c0e06663c, // S
        0x1818181818185a7e, // T
        0x7c66666666666666, // U
        0x183c666666666666, // V
        0xc6eefed6c6c6c6c6, // W
        0xc6c6ee386ceec6c6, // X
        0x1818183c7e666666, // Y
        0x7e060c183060607e, // Z
    };

    constexpr static std::array<uint64_t, 26> letters_small_led_bits = {
        0x7c667c603c000000, 0x3e66663e06060606, 0x3c6606663c000000, 0x7c66667c60606060, 0x3c067e663c000000, 0x0c0c0c3e0c6c6c38, 0x3c607c66667c0000,
        0x6666663e06060606, 0x3c18181800181800, 0x1c36363030003030, 0x66361e3666660606, 0x1818181818181818, 0xd6d6feeec6000000, 0x6666667e3e000000,
        0x3c6666663c000000, 0x06063e66663e0000, 0xf0b03c36363c0000, 0x060666663e000000, 0x3e403c027c000000, 0x181818187e181818, 0x7c66666666000000,
        0x183c666666000000, 0x7cd6d6d6c6000000, 0x663c183c66000000, 0x3c607c6666000000, 0x3c0c18303c000000};

    constexpr static uint64_t plus_small_led_bits = {
        0x0018187e7e181800,
    };

    constexpr static uint64_t zero = 0;

    if (c >= '0' && c <= '9')
    {
        return &digits_led_bits[c - '0'];
    }
    else if (c <= 'Z' && c >= 'A')
    {
        return &letters_capital_led_bits[c - 'A'];
    }
    else if (c <= 'z' && c >= 'a')
    {
        return &letters_small_led_bits[c - 'a'];
    }
    else if (c == '+')
    {
        return &plus_small_led_bits;
    }
    else
    {
        return &zero;
    }
}

void display::set_max7219_display(const std::array<const void *, 4> &values)
{
    CHECK_THROW_ESP(max7219_draw_image_8x8(&handle_, 0, values[0]));
    CHECK_THROW_ESP(max7219_draw_image_8x8(&handle_, 8, values[1]));
    CHECK_THROW_ESP(max7219_draw_image_8x8(&handle_, 16, values[2]));
    CHECK_THROW_ESP(max7219_draw_image_8x8(&handle_, 24, values[3]));
}

void display::restart_display_off_timer()
{
    if (display_off_timer_)
    {
        display_off_timer_->restart(display_off_timeout_);
    }
    else
    {
        display_off_timer_ = std::make_unique<esp32::timer::timer>([this] { set_display_value(None()); }, "display_off_timer");
        display_off_timer_->start_one_shot(display_off_timeout_);
    }
}

void display::set_default_brightness()
{
    const auto default_brightness = config_.get_screen_brightness();
    if (current_brightness_ != default_brightness)
    {
        set_max7219_brightness(default_brightness);
    }
}

void display::set_max7219_brightness(uint8_t value)
{
    CHECK_THROW_ESP(max7219_set_brightness(&handle_, value));
    current_brightness_ = value;
}

void display::gui_task()
{
    ESP_LOGI(DISPLAY_TAG, "Start to run Display Task on core:%d", xPortGetCoreID());

    try
    {
        set_default_brightness();
        CHECK_THROW_ESP(max7219_clear(&handle_));

        do
        {
            uint32_t notification_value = 0;
            xTaskNotifyWait(pdFALSE,             /* Don't clear bits on entry. */
                            ULONG_MAX,           /* Clear all bits on exit. */
                            &notification_value, /* Stores the notified value. */
                            portMAX_DELAY);

            if (notification_value & button_clicked_display_bit)
            {
                const auto current = config_.get_screen_brightness();
                const auto new_value = (current + 1) % 16;
                config_.set_screen_brightness(new_value);
                ESP_LOGI(DISPLAY_TAG, "Setting screen brightness to %d", new_value);
                set_display_value(ScreenBrightnessLevel(new_value));
                update_display_based_on_display_value();
            }
            else if (notification_value & set_display_changed_bit)
            {
                update_display_based_on_display_value();
            }
            else if (notification_value & fade_display_bit)
            {
                if (current_brightness_ <= 1)
                {
                    display_fade_timer_.reset();
                    ESP_LOGI(DISPLAY_TAG, "Display off");
                    CHECK_THROW_ESP(max7219_clear(&handle_));
                }
                else
                {
                    current_brightness_--;
                    set_max7219_brightness(current_brightness_);
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
        break;
    case NEW_FEEDBACK_RECEIVED: {
        const auto feedback_string = denon_avr_.get_last_feedback();

        if (feedback_string.empty())
        {
            set_display_value(None());
            break;
        }

        constexpr static std::string_view mute_on_command("MUON");
        constexpr static std::string_view mute_off_command("MUOFF");
        constexpr static std::string_view volume_prefix_command("MV");
        constexpr static std::string_view dynvol_prefix_command("PSDYNVOL");
        constexpr static std::string_view off_prefix_command("PWSTANDBY");
        constexpr static std::string_view on_prefix_command("PWON");
        if (feedback_string == mute_on_command)
        {
            set_display_value(MuteOn());
        }
        else if (feedback_string == mute_off_command)
        {
            set_display_value(None());
        }
        else if (feedback_string == off_prefix_command)
        {
            set_display_value(PowerOff());
        }
        else if (feedback_string == on_prefix_command)
        {
            set_display_value(FourChars({' ', 'O', 'N', ' '}));
        }
        else if (feedback_string.starts_with(volume_prefix_command))
        {
            constexpr static std::string_view volume_max_prefix_command("MVMAX");
            if (!feedback_string.starts_with(volume_max_prefix_command))
            {
                const auto volume_string = feedback_string.substr(volume_prefix_command.size());

                if ((volume_string.size() == 3) && (volume_string[2] == '5'))
                {
                    set_display_value(FourChars({' ', volume_string[0], volume_string[1], '+'}));
                }
                else
                {
                    set_display_value(FourChars({' ', volume_string[0], volume_string[1], ' '}));
                }
            }
        }
        else if (feedback_string.starts_with(dynvol_prefix_command))
        {
            const auto dynvol_string = feedback_string.substr(dynvol_prefix_command.size() + 1);

            constexpr static std::string_view off_command("OFF");
            constexpr static std::string_view light_command("LIT");
            constexpr static std::string_view med_command("MED");
            constexpr static std::string_view hev_command("HEV");
            uint8_t value = 0;
            if (off_command == dynvol_string)
            {
                value = 0;
            }
            else if (light_command == dynvol_string)
            {
                value = 1;
            }
            else if (med_command == dynvol_string)
            {
                value = 2;
            }
            else if (hev_command == dynvol_string)
            {
                value = 3;
            }

            set_display_value(DynVol(value));
        }

        break;
    }
    }
}

void display::button_click()
{
    ESP_LOGI(DISPLAY_TAG, "Button clicked");
    xTaskNotify(gui_task_.handle(), button_clicked_display_bit, eSetBits);
}