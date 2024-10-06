#pragma once

#include "preferences.h"
#include "util/noncopyable.h"
#include "util/singleton.h"
#include "util/semaphore_lockable.h"
#include <atomic>
#include <mutex>
#include <optional>
#include <vector>

class config : public esp32::singleton<config>
{
  public:
    void begin();
    void save();

    uint8_t get_screen_brightness();
    void set_screen_brightness(uint8_t screen_brightness);

  private:
    config() = default;

    friend class esp32::singleton<config>;

    mutable esp32::semaphore data_mutex_;
    preferences nvs_storage_;
};
