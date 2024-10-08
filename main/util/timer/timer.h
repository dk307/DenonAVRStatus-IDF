#pragma once

#include "util/exceptions.h"
#include "util/noncopyable.h"
#include <chrono>
#include <esp_timer.h>
#include <functional>
#include <string>

namespace esp32::timer
{

/**
 * @brief Get time since boot
 * @return time since \c esp_timer_init() was called (this normally happens early during application startup).
 */
static inline std::chrono::microseconds get_time()
{
    return std::chrono::microseconds(esp_timer_get_time());
}

/**
 * @brief Get the timestamp when the next timeout is expected to occur
 * @return Timestamp of the nearest timer event.
 *         The timebase is the same as for the values returned by \c get_time().
 */
static inline std::chrono::microseconds get_next_alarm()
{
    return std::chrono::microseconds(esp_timer_get_next_alarm());
}

/**
 * @brief
 * A timer using the esp_timer component which can be started either as one-shot timer or periodically.
 */
class timer : esp32::noncopyable
{
  public:
    /**
     * @param timeout_cb The timeout callback.
     * @param timer_name The name of the timer (optional). This is for debugging using \c esp_timer_dump().
     */
    timer(const std::function<void()> &timeout_cb, const std::string &timer_name);

    /**
     * Stop the timer if necessary and delete it.
     */
    ~timer();

    /**
     * @brief Start one-shot timer
     *
     * Timer should not be running (started) when this function is called.
     *
     * @param timeout timer timeout, in microseconds relative to the current moment.
     *
     * @throws ESPException with error ESP_ERR_INVALID_STATE if the timer is already running.
     */
    inline void start_one_shot(const std::chrono::microseconds &timeout)
    {
        CHECK_THROW_ESP(esp_timer_start_once(timer_handle_, timeout.count()));
    }

    /**
     * @brief Start periodic timer
     *
     * Timer should not be running when this function is called. This function will
     * start a timer which will trigger every 'period' microseconds.
     *
     * Timer should not be running (started) when this function is called.
     *
     * @param timeout timer timeout, in microseconds relative to the current moment.
     *
     * @throws ESPException with error ESP_ERR_INVALID_STATE if the timer is already running.
     */
    inline void start_periodic(const std::chrono::microseconds &period)
    {
        CHECK_THROW_ESP(esp_timer_start_periodic(timer_handle_, period.count()));
    }

    inline void restart(const std::chrono::microseconds &period)
    {
        CHECK_THROW_ESP(esp_timer_restart(timer_handle_, period.count()));
    }

    /**
     * @brief Stop the previously started timer.
     *
     * This function stops the timer previously started using \c start() or \c start_periodic().
     *
     * @throws ESPException with error ESP_ERR_INVALID_STATE if the timer has not been started yet.
     */
    inline void stop()
    {
        CHECK_THROW_ESP(esp_timer_stop(timer_handle_));
    }

  private:
    /**
     * Internal callback to hook into esp_timer component.
     */
    static void esp_timer_cb(void *arg);

    /**
     * Timer instance of the underlying esp_event component.
     */
    esp_timer_handle_t timer_handle_;

    /**
     * Callback which will be called once the timer triggers.
     */
    std::function<void()> timeout_cb_;

    /**
     * Name of the timer, will be passed to the underlying timer framework and is used for debugging.
     */
    const std::string name_;
};

} // namespace esp32::timer