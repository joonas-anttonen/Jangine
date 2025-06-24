#pragma once

#include "Event.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace Jangine::Logging
{
    class Log;

    class Logger
    {
    public:
        Logger(Log &log, std::string name);

        const std::string &Name() const noexcept;

        void Error(std::string_view message,
                   std::string_view callerName = "") const;

        void Warning(std::string_view message,
                     std::string_view callerName = "") const;

        void Information(std::string_view message,
                         std::string_view callerName = "") const;

        void Debug(std::string_view message,
                   std::string_view callerName = "") const;

        void Func(std::string_view func) const
        {
            Debug("", func);
        }

    private:
        Log &m_log;
        std::string m_name;
    };

} // namespace Jangine::Logging