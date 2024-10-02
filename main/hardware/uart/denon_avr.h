#pragma once

#include "app_events.h"
#include "command_processor.h"
#include "util/default_event.h"
#include "util/semaphore_lockable.h"
#include "util/singleton.h"
#include "util/task_wrapper.h"
#include "util/timer/timer.h"
#include <variant>

class denon_avr final : public esp32::singleton<denon_avr>
{
  public:
    void begin();
    auto get_last_feedback() const
    {
        return processor.get_last_command();
    }

  private:
    denon_avr() : uart_task_([this] { denon_avr::uart_task(); })
    {
    }

    friend class esp32::singleton<denon_avr>;

    esp32::task uart_task_;
    QueueHandle_t uart_queue;
    command_processor processor;

    void uart_task();
};