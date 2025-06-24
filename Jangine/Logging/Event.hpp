#pragma once

#include <string>
#include <chrono>
#include <format>
#include <stdexcept>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <algorithm>
#include <thread>

namespace Jangine::Logging
{
    enum class Severity
    {
        Debug = -1,
        Information = 0,
        Warning = 1,
        Error = 2
    };

    class Event
    {
    public:
        Severity severity;
        std::chrono::system_clock::time_point timestamp;
        std::string message;
        std::string type;
        std::string method;
        int32_t thread;

        Event(
            Severity severity,
            std::chrono::system_clock::time_point timestamp,
            std::string message,
            std::string type,
            std::string method,
            int thread)
            : severity(severity),
              timestamp(timestamp),
              message(std::move(message)),
              type(std::move(type)),
              method(std::move(method)),
              thread(thread)
        {
        }

        std::string ToShortString() const
        {
            return std::format("[{}] {} {}", thread, OriginString(type, method), message);
        }

        std::string ToString() const
        {
            return std::format(
                "[{}] [{}] [{}] {} {}",
                thread,
                DateTimeISO8601(timestamp),
                SeverityString(severity),
                OriginString(type, method),
                message);
        }

    private:
        static std::string OriginString(const std::string &type, const std::string &method)
        {
            return std::format("{}::{}", type, method);
        }

        static std::string SeverityString(Severity level)
        {
            switch (level)
            {
            case Severity::Error:
                return "ERROR";
            case Severity::Warning:
                return "WARNING";
            case Severity::Information:
                return "INFO";
            case Severity::Debug:
                return "DEBUG";
            default:
                throw std::logic_error("Unknown severity");
            }
        }

        static std::string DateTimeISO8601(const std::chrono::system_clock::time_point &tp)
        {
            std::time_t t = std::chrono::system_clock::to_time_t(tp);
            std::tm tm;
#if defined(_WIN32)
            gmtime_s(&tm, &t);
#else
            gmtime_r(&t, &tm);
#endif
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
            return buf;
        }
    };
}