#include "denon_avr.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "logging/logging_tags.h"
#include "util/cores.h"
#include "util/default_event.h"
#include "util/exceptions.h"
#include "util/helper.h"
#include <driver/spi_master.h>
#include <esp_log.h>

constexpr static int TX_PIN = 26;
constexpr static int RX_PIN = 25;
constexpr static size_t MAX_COMMAND_SIZE = 135;
constexpr static char PATTERN_CHAR = 0x0D;
constexpr static size_t PATTERN_SIZE = 1;
constexpr static uart_port_t UART_SEL = UART_NUM_2;

void denon_avr::begin()
{
    ESP_LOGI(DENON_AVR_TAG, "Initializing UART");

    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // We won't use a buffer for sending data.
    CHECK_THROW_ESP(uart_driver_install(UART_SEL, MAX_COMMAND_SIZE * 10, 0, 32, &uart_queue, ESP_INTR_FLAG_IRAM));
    CHECK_THROW_ESP(uart_param_config(UART_SEL, &uart_config));
    CHECK_THROW_ESP(uart_set_pin(UART_SEL, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    uart_set_mode(UART_SEL, UART_MODE_UART);
    ESP_LOGI(DENON_AVR_TAG, "Setting up denon_avr");

    CHECK_THROW_ESP(uart_task_.spawn_pinned("uart", 1024 * 8, esp32::task::default_priority, esp32::uart_core));
    ESP_LOGI(DENON_AVR_TAG, "denon_avr setup done");
}

void denon_avr::uart_task()
{
    std::vector<char> read_data(512);

    ESP_LOGI(DENON_AVR_TAG, "Start to run denon_avr Task on core:%d", xPortGetCoreID());
    try
    {
        while (true)
        {
            // Waiting for UART event.
            uart_event_t event{};
            if (xQueueReceive(uart_queue, &event, portMAX_DELAY))
            {
                ESP_LOGI(DENON_AVR_TAG, "uart event:%d", event.type);
                switch (event.type)
                {
                // Event of UART receiving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/
                case UART_DATA: {
                    ESP_LOGD(DENON_AVR_TAG, "UART DATA size: %d", event.size);
                    const auto length = uart_read_bytes(UART_SEL, read_data.data(), event.size, 20 / portTICK_PERIOD_MS);
                    ESP_LOGI(DENON_AVR_TAG, "Data:%.*s", length, reinterpret_cast<const char *>(read_data.data()));
                    const bool command_changed = processor.add_data(std::string_view(read_data.data(), length));
                    if (command_changed)
                    {
                        CHECK_THROW_ESP(esp32::event_post(APP_COMMON_EVENT, NEW_FEEDBACK_RECEIVED));
                    }
                }
                break;

                case UART_FIFO_OVF: { // Event of HW FIFO overflow detected
                    ESP_LOGW(DENON_AVR_TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    CHECK_THROW_ESP(uart_flush_input(UART_SEL));
                    xQueueReset(uart_queue);
                }
                break;

                case UART_BUFFER_FULL: { // Event of UART ring buffer full
                    ESP_LOGW(DENON_AVR_TAG, "ring buffer full");
                    // If buffer full happened, you should consider increasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    CHECK_THROW_ESP(uart_flush_input(UART_SEL));
                    xQueueReset(uart_queue);
                }
                break;

                case UART_BREAK: { // Event of UART RX break detected
                    ESP_LOGI(DENON_AVR_TAG, "uart rx break");
                }
                break;

                case UART_PARITY_ERR: { // Event of UART parity check error
                    ESP_LOGI(DENON_AVR_TAG, "uart parity error");
                }
                break;

                case UART_FRAME_ERR: { // Event of UART frame error
                    ESP_LOGI(DENON_AVR_TAG, "uart frame error");
                }
                break;

                // Others
                default: {
                    ESP_LOGI(DENON_AVR_TAG, "uart event type: %d", event.type);
                }
                break;
                }
            }
        }
        vTaskDelete(NULL);
    }
    catch (const std::exception &ex)
    {
        ESP_LOGE(OPERATIONS_TAG, "Uart Task Failure:%s", ex.what());
        vTaskDelay(pdMS_TO_TICKS(3000));
        throw;
    }

    vTaskDelete(NULL);
}
