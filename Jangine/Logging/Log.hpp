#pragma once

#include "../Shared.hpp"

namespace Jangine::Logging
{
    class Log;

    class Event
    {
    public:
        enum class Severity
        {
            Debug = -1,
            Information = 0,
            Warning = 1,
            Error = 2
        };

        Severity severity;
        std::chrono::system_clock::time_point timestamp;
        std::string message;
        std::string type;
        std::string method;
        std::string thread;

        Event(Severity severity,
              std::chrono::system_clock::time_point timestamp,
              std::string message,
              std::string type,
              std::string method,
              std::string thread)
            : severity(severity),
              timestamp(timestamp),
              message(std::move(message)),
              type(std::move(type)),
              method(std::move(method)),
              thread(std::move(thread))
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

    // Log listener interface
    class ILogListener
    {
    public:
        virtual ~ILogListener() = default;
        virtual void Append(const Event &logEvent) = 0;
        virtual void Flush() = 0;
    };

    // Log listener that writes log events to the console
    class ConsoleLogListener final : public ILogListener
    {
        enum class ConsoleColor
        {
            Default = 0,
            Blue,
            Yellow,
            Red
        };

        void SetColor(ConsoleColor color)
        {
            switch (color)
            {
            case ConsoleColor::Blue:
                std::cout << "\033[34m";
                break;
            case ConsoleColor::Yellow:
                std::cout << "\033[33m";
                break;
            case ConsoleColor::Red:
                std::cout << "\033[31m";
                break;
            case ConsoleColor::Default:
            default:
                std::cout << "\033[0m";
                break;
            }
        }
        void ResetColor()
        {
            std::cout << "\033[0m";
        }

    public:
        void Append(const Event &logEvent) override
        {
            // Set color based on severity
            ConsoleColor color = ConsoleColor::Default;
            switch (logEvent.severity)
            {
            case Event::Severity::Debug:
                color = ConsoleColor::Blue;
                break;
            case Event::Severity::Information:
                color = ConsoleColor::Default;
                break;
            case Event::Severity::Warning:
                color = ConsoleColor::Yellow;
                break;
            case Event::Severity::Error:
                color = ConsoleColor::Red;
                break;
            default:
                color = ConsoleColor::Default;
                break;
            }
            SetColor(color);
            std::cout << logEvent.ToShortString() << std::endl;
            ResetColor();
        }

        void Flush() override
        {
            std::cout.flush();
        }
    };

    // Log listener that writes log events to a file
    class FileLogListener final : public ILogListener
    {
    public:
        explicit FileLogListener(const std::string &path, std::size_t maximumFileSize = 10 * 1024 * 1024)
            : outputPath_(std::filesystem::absolute(path).string()),
              outputMaximumSize_(maximumFileSize),
              stopFlag_(false)
        {
            // Ensure the directory exists
            std::filesystem::create_directories(std::filesystem::path(outputPath_).parent_path());
            outputWriter_.open(outputPath_, std::ios::app);
            workerThread_ = std::thread([this]
                                        { this->Worker(); });
        }

        ~FileLogListener() override
        {
            stopFlag_ = true;
            cv_.notify_all();
            if (workerThread_.joinable())
                workerThread_.join();
            Flush();
            outputWriter_.close();
        }

        void Append(const Event &logEvent) override
        {
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                events_.push(logEvent);
            }
            cv_.notify_one();
        }

        void Flush() override
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            while (!events_.empty())
            {
                WriteEvent(events_.front());
                events_.pop();
            }
            outputWriter_.flush();
        }

    private:
        void Worker()
        {
            while (!stopFlag_)
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                cv_.wait_for(lock, std::chrono::milliseconds(200), [this]
                             { return !events_.empty() || stopFlag_; });
                while (!events_.empty())
                {
                    WriteEvent(events_.front());
                    events_.pop();
                    LimitFileSize();
                }
                outputWriter_.flush();
            }
        }

        void WriteEvent(const Event &ev)
        {
            outputWriter_ << ev.ToString() << std::endl;
        }

        void LimitFileSize()
        {
            outputWriter_.flush();
            if (outputWriter_.tellp() > static_cast<std::streamoff>(outputMaximumSize_))
            {
                outputWriter_.close();
                std::filesystem::copy_file(outputPath_, outputPath_ + ".old", std::filesystem::copy_options::overwrite_existing);
                outputWriter_.open(outputPath_, std::ios::trunc);
            }
        }

        std::string outputPath_;
        std::size_t outputMaximumSize_;
        std::ofstream outputWriter_;
        std::queue<Event> events_;
        std::mutex queueMutex_;
        std::condition_variable cv_;
        std::thread workerThread_;
        std::atomic<bool> stopFlag_;
    };

    /// @brief Logger instance associated with a specific name
    class Logger
    {
    public:
        Logger(Log &log, std::string name);

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

    /// @brief Central logging class managing loggers and listeners
    class JANGINE_API Log final
    {
    public:
        Log() = default;
        ~Log() { FlushAll(); }

        /// @brief Adds a ConsoleLogListener to the collection of listeners (thread-safe)
        ILogListener *AddConsoleListener()
        {
            auto listener = std::make_unique<ConsoleLogListener>();
            ILogListener *ptr = listener.get();
            {
                std::unique_lock lock(listenersMutex_);
                listeners_.emplace_back(std::move(listener));
            }
            return ptr;
        }

        /// @brief Adds a FileLogListener to the collection of listeners (thread-safe)
        ILogListener *AddFileListener(const std::string &path)
        {
            auto listener = std::make_unique<FileLogListener>(path);
            ILogListener *ptr = listener.get();
            {
                std::unique_lock lock(listenersMutex_);
                listeners_.emplace_back(std::move(listener));
            }
            return ptr;
        }

        /// @brief Adds a custom ILogListener to the collection (thread-safe)
        void AddListener(std::unique_ptr<ILogListener> listener)
        {
            std::unique_lock lock(listenersMutex_);
            listeners_.emplace_back(std::move(listener));
        }

        /// @brief Maps the current thread ID to a name (thread-safe)
        void SetCurrentThreadName(const std::string &name)
        {
            std::unique_lock lock(threadIdLock_);
            threadIdToName_[std::this_thread::get_id()] = name;
        }

        /// @brief Returns a copy of the listeners (thread-safe)
        std::vector<ILogListener *> GetListeners() const
        {
            std::shared_lock lock(listenersMutex_);
            std::vector<ILogListener *> result;
            for (const auto &l : listeners_)
                result.push_back(l.get());
            return result;
        }

        /// @brief Flushes all listeners (thread-safe)
        void FlushAll()
        {
            std::shared_lock lock(listenersMutex_);
            for (const auto &l : listeners_)
                l->Flush();
        }

        /// @brief Gets a Logger instance with the specified name (thread-safe, cached)
        const Logger &GetLogger(const std::string &name)
        {
            std::unique_lock lock(loggersMutex_);
            auto it = loggers_.find(name);
            if (it != loggers_.end())
                return *it->second.get();

            auto logger = std::make_unique<Logger>(*this, name);
            Logger *ptr = logger.get();
            loggers_[name] = std::move(logger);
            return *ptr;
        }

        /// @brief Appends a message to the log (thread-safe)
        void Append(Event::Severity severity,
                    std::string_view message,
                    std::string_view typeName,
                    std::string_view methodName)
        {
            std::string threadName;
            {
                std::scoped_lock lock(threadIdLock_);
                auto maybeThreadName = threadIdToName_.find(std::this_thread::get_id());
                threadName = maybeThreadName != threadIdToName_.end() ? maybeThreadName->second : unknownThreadName;
            }

            Event logEvent(
                severity,
                std::chrono::system_clock::now(),
                std::string(message),
                std::string(typeName),
                std::string(methodName),
                threadName);

            std::shared_lock lock(listenersMutex_);
            for (const auto &l : listeners_)
                l->Append(logEvent);
        }

    private:
        static constexpr const char *unknownThreadName = "unknown";

        SpinLock threadIdLock_;
        std::unordered_map<std::thread::id, std::string> threadIdToName_;

        mutable std::shared_mutex listenersMutex_;
        std::vector<std::unique_ptr<ILogListener>> listeners_;

        mutable std::mutex loggersMutex_;
        std::unordered_map<std::string, std::unique_ptr<Logger>> loggers_;
    };

    inline Logger::Logger(Log &log, std::string name)
        : m_log(log), m_name(std::move(name)) {}

    inline void Logger::Error(std::string_view message, std::string_view callerName) const
    {
        m_log.Append(Event::Severity::Error, message, m_name, callerName);
    }

    inline void Logger::Warning(std::string_view message, std::string_view callerName) const
    {
        m_log.Append(Event::Severity::Warning, message, m_name, callerName);
    }

    inline void Logger::Information(std::string_view message, std::string_view callerName) const
    {
        m_log.Append(Event::Severity::Information, message, m_name, callerName);
    }

    inline void Logger::Debug(std::string_view message, std::string_view callerName) const
    {
        m_log.Append(Event::Severity::Debug, message, m_name, callerName);
    }
}