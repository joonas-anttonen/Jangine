#pragma once

#include "../Shared.hpp"

#include "Event.hpp"
#include "ILogListener.hpp"
#include "Logger.hpp"

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
    class JANGINE_API Log final
    {
    public:
        Log() = default;
        ~Log() { FlushAll(); }

        // Adds a ConsoleLogListener to the collection of listeners (thread-safe)
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

        // Adds a FileLogListener to the collection of listeners (thread-safe)
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

        // Adds a custom ILogListener to the collection (thread-safe)
        void AddListener(std::unique_ptr<ILogListener> listener)
        {
            std::unique_lock lock(listenersMutex_);
            listeners_.emplace_back(std::move(listener));
        }

        // Maps the current thread ID to a name (thread-safe)
        void SetThreadIdMapping(const std::string &name)
        {
            std::unique_lock lock(threadIdLock_);
            threadIdToName_[std::this_thread::get_id()] = name;
        }

        // Returns a copy of the listeners (thread-safe)
        std::vector<ILogListener *> GetListeners() const
        {
            std::shared_lock lock(listenersMutex_);
            std::vector<ILogListener *> result;
            for (const auto &l : listeners_)
                result.push_back(l.get());
            return result;
        }

        // Flushes all listeners (thread-safe)
        void FlushAll()
        {
            std::shared_lock lock(listenersMutex_);
            for (const auto &l : listeners_)
                l->Flush();
        }

        // Gets a Logger instance with the specified name (thread-safe, cached)
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

        // Appends a message to the log (thread-safe)
        void Append(Severity severity,
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
}