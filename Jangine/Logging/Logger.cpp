#include "Logger.hpp"
#include "Log.hpp"   // For Log
#include "Event.hpp" // For Severity and Log

#include <string>
#include <string_view>

namespace Jangine::Logging
{
    Logger::Logger(Log &log, std::string name)
        : m_log(log), m_name(std::move(name)) {}

    const std::string &Logger::Name() const noexcept { return m_name; }

    void Logger::Error(std::string_view message,
                       std::string_view callerName) const
    {
        m_log.Append(Severity::Error, message, m_name, callerName);
    }

    void Logger::Warning(std::string_view message,
                         std::string_view callerName) const
    {
        m_log.Append(Severity::Warning, message, m_name, callerName);
    }

    void Logger::Information(std::string_view message,
                             std::string_view callerName) const
    {
        m_log.Append(Severity::Information, message, m_name, callerName);
    }

    void Logger::Debug(std::string_view message,
                       std::string_view callerName) const
    {
        m_log.Append(Severity::Debug, message, m_name, callerName);
    }
} // namespace Jangine::Logging