#pragma once

#include "Event.hpp"

#include <string>
#include <memory>
#include <queue>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <atomic>

namespace Jangine::Logging
{
    enum class ConsoleColor
    {
        Default = 0,
        Blue,
        Yellow,
        Red
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
            case Severity::Debug:
                color = ConsoleColor::Blue;
                return;
            case Severity::Information:
                color = ConsoleColor::Default;
                break;
            case Severity::Warning:
                color = ConsoleColor::Yellow;
                break;
            case Severity::Error:
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

        const std::string &OutputPath() const { return outputPath_; }
        std::size_t OutputMaximumSize() const { return outputMaximumSize_; }

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

} // namespace Jangine::Logging