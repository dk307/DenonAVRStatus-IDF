#pragma once

#include "util/semaphore_lockable.h"
#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>

class command_processor
{
  public:
    bool add_data(std::string_view data)
    {
        std::lock_guard<esp32::semaphore> lock(mutex);
        constexpr char separator = 0x0D;
        auto iter = std::find(data.begin(), data.end(), separator);

        // no separator found
        if (iter == data.end())
        {
            buffer.append(data);
            return false;
        }
        else
        {
            const auto pos = std::distance(data.begin(), iter);
            buffer.append(data.substr(0, pos));
            last_command = std::move(buffer);
            buffer = data.substr(pos + 1);
            return true;
        }
    }

    const auto &get_last_command() const
    {
        std::lock_guard<esp32::semaphore> lock(mutex);
        return last_command;
    }

  private:
    mutable esp32::semaphore mutex;
    std::string buffer;
    std::string last_command;
};
